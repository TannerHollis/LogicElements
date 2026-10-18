/**
 * @file le_hal_sim.c
 * @brief Simulator HAL implementation for desktop and automated testing.
 */

#include "le_hal.h"
#include "le_storage.h"
#include "le_nvram.h"
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
static float s_sim_dac[16] = {0.0f};
static uint8_t s_sim_flash[LE_SLOT_SIZE_BYTES * (LE_MAX_CONFIG_SLOTS + 1)] = {0};
static uint8_t s_sim_nvram[LE_NVRAM_TOTAL_BYTES] = {0};

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
    memset(s_sim_dac, 0, sizeof(s_sim_dac));
    memset(s_sim_flash, 0, sizeof(s_sim_flash));
    memset(s_sim_nvram, 0, sizeof(s_sim_nvram));
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
    if (channel < 16) s_sim_dac[channel] = value;
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

static bool sim_storage_read(const le_storage_sector_t* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len)
{
    uint32_t abs_addr = (sector ? sector->phys_addr : 0) + offset_in_sector;
    if (abs_addr + len <= sizeof(s_sim_flash)) {
        memcpy(buf, &s_sim_flash[abs_addr], len);
        return true;
    }
    return false;
}

static bool sim_storage_erase_sector(const le_storage_sector_t* sector)
{
    uint32_t abs_addr = sector ? sector->phys_addr : 0;
    uint32_t erase_sz = (sector && sector->erase_size > 0) ? sector->erase_size : LE_STORAGE_SECTOR_SIZE;
    if (abs_addr + erase_sz <= sizeof(s_sim_flash)) {
        memset(&s_sim_flash[abs_addr], 0xFF, erase_sz);
        return true;
    }
    return false;
}

static bool sim_storage_write_page(const le_storage_sector_t* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len)
{
    uint32_t abs_addr = (sector ? sector->phys_addr : 0) + offset_in_sector;
    if (abs_addr + len <= sizeof(s_sim_flash)) {
        memcpy(&s_sim_flash[abs_addr], buf, len);
        return true;
    }
    return false;
}

static const uint8_t* sim_storage_get_xip_ptr(const le_storage_sector_t* sector, uint32_t offset_in_sector)
{
    uint32_t abs_addr = (sector ? sector->phys_addr : 0) + offset_in_sector;
    if (abs_addr < sizeof(s_sim_flash)) {
        return &s_sim_flash[abs_addr];
    }
    return NULL;
}

static bool sim_nvram_read(uint32_t offset, uint8_t* buf, size_t len)
{
    if (offset + len <= sizeof(s_sim_nvram)) {
        memcpy(buf, &s_sim_nvram[offset], len);
        return true;
    }
    return false;
}

static bool sim_nvram_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    if (offset + len <= sizeof(s_sim_nvram)) {
        memcpy(&s_sim_nvram[offset], buf, len);
        return true;
    }
    return false;
}

static bool sim_nvram_erase_sector(uint32_t sector_idx, uint32_t phys_addr)
{
    (void)sector_idx;
    if (phys_addr + LE_NVRAM_BLOCK_SIZE <= sizeof(s_sim_nvram)) {
        memset(&s_sim_nvram[phys_addr], 0xFF, LE_NVRAM_BLOCK_SIZE);
        return true;
    }
    return false;
}

static bool sim_nvram_write_page(uint32_t phys_addr, const uint8_t* buf, size_t len)
{
    if (phys_addr + len <= sizeof(s_sim_nvram)) {
        memcpy(&s_sim_nvram[phys_addr], buf, len);
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

float le_sim_get_dac(uint8_t channel)
{
    if (channel < 16) return s_sim_dac[channel];
    return 0.0f;
}

void le_sim_set_dac(uint8_t channel, float value)
{
    if (channel < 16) s_sim_dac[channel] = value;
}

#include "le_custom_node.h"

static le_status_t sim_ext_call(uint8_t func_id, const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    return le_custom_nodes_dispatch(func_id, args, in_count, out_count, img);
}

static const char* sim_get_custom_nodes_json(void)
{
    return le_custom_nodes_get_json();
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
    .storage_erase_sector = sim_storage_erase_sector,
    .storage_write_page = sim_storage_write_page,
    .storage_get_xip_ptr = sim_storage_get_xip_ptr,
    .nvram_read = sim_nvram_read,
    .nvram_write = sim_nvram_write,
    .nvram_write_page = sim_nvram_write_page,
    .nvram_erase_sector = sim_nvram_erase_sector,
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
