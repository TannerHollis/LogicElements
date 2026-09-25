/**
 * @file le_hal_sim.c
 * @brief Simulator HAL implementation for desktop and automated testing.
 */

#include "le_hal.h"
#include "le_storage.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

const le_hal_t* g_le_hal = NULL;

void le_hal_set(const le_hal_t* hal)
{
    g_le_hal = hal;
}

/* Simulated hardware state */
static bool s_sim_gpio[64] = {0};
static float s_sim_adc[16] = {0.0f};
static uint32_t s_sim_adc_raw[16] = {0};
static uint8_t s_sim_flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};

#if LE_ENABLE_SERIAL_BUS
static uint8_t s_sim_i2c_last_startup[16] = {0};
static size_t  s_sim_i2c_startup_len = 0;
static uint8_t s_sim_i2c_mock_reg_data[16] = {0x00, 0x19}; /* 0x0019 = 25 */
static size_t  s_sim_i2c_transfers = 0;

static uint8_t s_sim_spi_last_startup[16] = {0};
static size_t  s_sim_spi_startup_len = 0;
static uint8_t s_sim_spi_mock_rx_data[16] = {0x04, 0xD2};  /* 0x04D2 = 1234 */
static size_t  s_sim_spi_transfers = 0;
#endif

static bool sim_init(void)
{
    memset(s_sim_gpio, 0, sizeof(s_sim_gpio));
    memset(s_sim_adc, 0, sizeof(s_sim_adc));
    memset(s_sim_adc_raw, 0, sizeof(s_sim_adc_raw));
    memset(s_sim_flash, 0, sizeof(s_sim_flash));
#if LE_ENABLE_SERIAL_BUS
    s_sim_i2c_transfers = 0;
    s_sim_i2c_startup_len = 0;
    s_sim_spi_transfers = 0;
    s_sim_spi_startup_len = 0;
#endif
    return true;
}

static void sim_shutdown(void)
{
}

static const char* sim_get_platform_name(void)
{
    return "Simulator / Desktop";
}

static bool sim_gpio_read(uint8_t pin)
{
    if (pin < 64) return s_sim_gpio[pin];
    return false;
}

static void sim_gpio_write(uint8_t pin, bool value)
{
    if (pin < 64) s_sim_gpio[pin] = value;
}

static float sim_adc_read(uint8_t channel)
{
    if (channel < 16) return s_sim_adc[channel];
    return 0.0f;
}

static uint32_t sim_adc_read_raw(uint8_t channel)
{
    if (channel < 16) return s_sim_adc_raw[channel];
    return 0;
}

static void sim_dac_write(uint8_t channel, float value)
{
    (void)channel;
    (void)value;
}

static uint32_t sim_get_time_ms(void)
{
#if defined(_WIN32)
    return (uint32_t)GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
#endif
}

static uint64_t sim_get_time_us(void)
{
#if defined(_WIN32)
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000ULL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)((ts.tv_sec * 1000000ULL) + (ts.tv_nsec / 1000ULL));
#endif
}

static size_t sim_uart_available(void)
{
    return 0;
}

static size_t sim_uart_read(uint8_t* buf, size_t max_len)
{
    (void)buf; (void)max_len;
    return 0;
}

/* Test-bench TX capture: every byte written by the firmware (comms/CLI
 * responses) is appended here so tests can assert ACK/NACK and prompts. */
#define SIM_TX_CAPTURE_MAX    2048
static uint8_t  s_sim_tx[2048] = {0};
static size_t   s_sim_tx_len = 0;

static size_t sim_uart_write(const uint8_t* buf, size_t len)
{
    if (buf && len > 0) {
        fwrite(buf, 1, len, stdout);
        fflush(stdout);
        size_t room = (size_t)SIM_TX_CAPTURE_MAX - s_sim_tx_len;
        size_t n = (len < room) ? len : room;
        if (n > 0) {
            memcpy(s_sim_tx + s_sim_tx_len, buf, n);
            s_sim_tx_len += n;
        }
    }
    return len;
}

/** @brief Clears the sim TX capture buffer (call before a command under test). */
void le_sim_capture_tx_reset(void)
{
    s_sim_tx_len = 0;
}

/** @brief Returns the number of captured TX bytes. */
size_t le_sim_capture_tx_len(void)
{
    return s_sim_tx_len;
}

/** @brief Copies captured TX bytes into @p out (up to @p cap). */
void le_sim_capture_tx_get(uint8_t* out, size_t cap)
{
    if (!out || cap == 0) return;
    size_t n = (s_sim_tx_len < cap) ? s_sim_tx_len : cap;
    memcpy(out, s_sim_tx, n);
}

static bool sim_storage_read(uint32_t offset, uint8_t* buf, size_t len)
{
    if (offset + len <= sizeof(s_sim_flash)) {
        memcpy(buf, &s_sim_flash[offset], len);
        return true;
    }
    return false;
}

static bool sim_storage_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    if (offset + len <= sizeof(s_sim_flash)) {
        memcpy(&s_sim_flash[offset], buf, len);
        return true;
    }
    return false;
}

#if LE_ENABLE_SERIAL_BUS
static bool sim_i2c_write(uint8_t addr_7bit, const uint8_t* tx_data, size_t len)
{
    (void)addr_7bit;
    if (tx_data && len > 0) {
        size_t copy_len = len > sizeof(s_sim_i2c_last_startup) ? sizeof(s_sim_i2c_last_startup) : len;
        memcpy(s_sim_i2c_last_startup, tx_data, copy_len);
        s_sim_i2c_startup_len = copy_len;
    }
    s_sim_i2c_transfers++;
    return true;
}

static bool sim_i2c_read(uint8_t addr_7bit, uint8_t* rx_data, size_t len)
{
    (void)addr_7bit;
    if (rx_data && len > 0) {
        size_t copy_len = len > sizeof(s_sim_i2c_mock_reg_data) ? sizeof(s_sim_i2c_mock_reg_data) : len;
        memcpy(rx_data, s_sim_i2c_mock_reg_data, copy_len);
    }
    s_sim_i2c_transfers++;
    return true;
}

static bool sim_i2c_write_read(uint8_t addr_7bit, const uint8_t* tx_data, size_t tx_len, uint8_t* rx_data, size_t rx_len)
{
    (void)addr_7bit;
    (void)tx_data;
    (void)tx_len;
    if (rx_data && rx_len > 0) {
        size_t copy_len = rx_len > sizeof(s_sim_i2c_mock_reg_data) ? sizeof(s_sim_i2c_mock_reg_data) : rx_len;
        memcpy(rx_data, s_sim_i2c_mock_reg_data, copy_len);
    }
    s_sim_i2c_transfers++;
    return true;
}

static bool sim_spi_transfer(uint8_t cs_pin, const uint8_t* tx_data, uint8_t* rx_data, size_t len)
{
    (void)cs_pin;
    if (tx_data && len > 0) {
        size_t copy_len = len > sizeof(s_sim_spi_last_startup) ? sizeof(s_sim_spi_last_startup) : len;
        memcpy(s_sim_spi_last_startup, tx_data, copy_len);
        if (!rx_data) {
            s_sim_spi_startup_len = copy_len;
        }
    }
    if (rx_data && len > 0) {
        size_t copy_len = len > sizeof(s_sim_spi_mock_rx_data) ? sizeof(s_sim_spi_mock_rx_data) : len;
        memcpy(rx_data, s_sim_spi_mock_rx_data, copy_len);
    }
    s_sim_spi_transfers++;
    return true;
}

void le_sim_set_mock_i2c_response(const uint8_t* data, size_t len)
{
    if (data && len > 0) {
        size_t c = len > sizeof(s_sim_i2c_mock_reg_data) ? sizeof(s_sim_i2c_mock_reg_data) : len;
        memcpy(s_sim_i2c_mock_reg_data, data, c);
    }
}

void le_sim_set_mock_spi_response(const uint8_t* data, size_t len)
{
    if (data && len > 0) {
        size_t c = len > sizeof(s_sim_spi_mock_rx_data) ? sizeof(s_sim_spi_mock_rx_data) : len;
        memcpy(s_sim_spi_mock_rx_data, data, c);
    }
}

size_t le_sim_get_i2c_transfers(void) { return s_sim_i2c_transfers; }
size_t le_sim_get_spi_transfers(void) { return s_sim_spi_transfers; }
size_t le_sim_get_i2c_startup_len(void) { return s_sim_i2c_startup_len; }
size_t le_sim_get_spi_startup_len(void) { return s_sim_spi_startup_len; }
#endif

void le_sim_set_adc_raw(uint8_t channel, uint32_t value)
{
    if (channel < 16) {
        s_sim_adc_raw[channel] = value;
    }
}

static le_status_t sim_ext_call(uint8_t func_id, const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    if (!img) return LE_ERR_NULL_PTR;

    switch (func_id)
    {
        case 0x81: {
            if (in_count < 1 || out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
            /* Function 0x81: Hardware Math - Square Root: out = sqrtf(in) */
            float a = le_process_image_get_float(img, args[0]);
            float res = (a > 0.0f) ? sqrtf(a) : 0.0f;
            le_process_image_set_float(img, args[in_count], res);
            return LE_OK;
        }

        case 0x82: {
            if (in_count < 2 || out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
            /* Function 0x82: Dual-Input Adder with Scaling: out = (in_a + in_b) * 1.5 */
            float a = le_process_image_get_float(img, args[0]);
            float b = le_process_image_get_float(img, args[1]);
            le_process_image_set_float(img, args[in_count], (a + b) * 1.5f);
            return LE_OK;
        }

        case 0x83: {
            if (in_count < 2 || out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
            /* Function 0x83: Boolean Pulse / Strobe: out = in_a && !in_b */
            bool a = le_process_image_get_bool(img, args[0]);
            bool b = le_process_image_get_bool(img, args[1]);
            le_process_image_set_bool(img, args[in_count], a && !b);
            return LE_OK;
        }

        case 0x84: {
            /* Function 0x84: Two-of-three majority (3-in/1-out) - proves N-arity. */
            if (in_count < 3 || out_count < 1) return LE_ERR_OUT_OF_BOUNDS;
            int ones = 0;
            for (int i = 0; i < 3; i++) {
                if (le_process_image_get_bool(img, args[i])) ones++;
            }
            le_process_image_set_bool(img, args[in_count], ones >= 2);
            return LE_OK;
        }

        default:
            break;
    }

    return LE_OK;
}

static const char* sim_get_custom_nodes_json(void)
{
    return "[\n"
           "  {\n"
           "    \"type_id\": \"Sim_HW_Sqrt\",\n"
           "    \"display_name\": \"HW Square Root\",\n"
           "    \"category\": \"Simulator\",\n"
           "    \"description\": \"Hardware-accelerated square root calculation\",\n"
           "    \"function_id\": 0x81,\n"
           "    \"inputs\": [{\"name\": \"In\", \"type\": \"float\", \"default\": 0.0}],\n"
           "    \"outputs\": [{\"name\": \"Out\", \"type\": \"float\"}]\n"
           "  },\n"
           "  {\n"
           "    \"type_id\": \"Sim_Scaled_Add\",\n"
           "    \"display_name\": \"Scaled Adder\",\n"
           "    \"category\": \"Simulator\",\n"
           "    \"description\": \"Adds two inputs and scales by 1.5\",\n"
           "    \"function_id\": 0x82,\n"
           "    \"inputs\": [{\"name\": \"A\", \"type\": \"float\", \"default\": 0.0}, {\"name\": \"B\", \"type\": \"float\", \"default\": 0.0}],\n"
           "    \"outputs\": [{\"name\": \"Out\", \"type\": \"float\"}]\n"
           "  }\n"
           "]";
}

static const le_hal_t s_sim_hal = {
    .init = sim_init,
    .shutdown = sim_shutdown,
    .get_platform_name = sim_get_platform_name,
    .gpio_read = sim_gpio_read,
    .gpio_write = sim_gpio_write,
    .adc_read = sim_adc_read,
    .adc_read_raw = sim_adc_read_raw,
    .dac_write = sim_dac_write,
    .get_time_ms = sim_get_time_ms,
    .get_time_us = sim_get_time_us,
    .uart_available = sim_uart_available,
    .uart_read = sim_uart_read,
    .uart_write = sim_uart_write,
    .storage_read = sim_storage_read,
    .storage_write = sim_storage_write,
#if LE_ENABLE_SERIAL_BUS
    .i2c_write = sim_i2c_write,
    .i2c_read = sim_i2c_read,
    .i2c_write_read = sim_i2c_write_read,
    .spi_transfer = sim_spi_transfer,
#endif
    .ext_call = sim_ext_call,
    .get_custom_nodes_json = sim_get_custom_nodes_json,
};

const le_hal_t* le_hal_get_sim(void)
{
    return &s_sim_hal;
}
