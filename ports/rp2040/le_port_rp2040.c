
/**
 * @file le_port.c  RP2040 / Raspberry Pi Pico Board Port (Pico C/C++ SDK)
 *                  v3 / le_hal_t contract.
 */

#include "le_port.h"
#include "le_storage.h"
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

#define LE_PORT_FLASH_BASE 0x08000000UL
static uint8_t s_port_flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};

/* %I[0..n] -> Pico GPIO numbers */
static const uint8_t INPUT_PINS[] = { 14, 15, 16, 17 };
#define NUM_INPUTS (sizeof(INPUT_PINS) / sizeof(INPUT_PINS[0]))
/* %Q[0..n] -> Pico GPIO numbers */
static const uint8_t OUTPUT_PINS[] = { 25, 18, 19, 20 };
#define NUM_OUTPUTS (sizeof(OUTPUT_PINS) / sizeof(OUTPUT_PINS[0]))

#define UART_ID   uart0
#define BAUD_RATE 115200

void le_port_init(void)
{
    for (uint8_t i = 0; i < NUM_INPUTS; i++) { gpio_init(INPUT_PINS[i]); gpio_set_dir(INPUT_PINS[i], GPIO_IN); gpio_pull_up(INPUT_PINS[i]); }
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) { gpio_init(OUTPUT_PINS[i]); gpio_set_dir(OUTPUT_PINS[i], GPIO_OUT); gpio_put(OUTPUT_PINS[i], 0); }
    uart_init(UART_ID, BAUD_RATE);
}

void le_port_read_inputs(le_process_image_t* img)
{
    if (!img) return;
    for (uint8_t i = 0; i < NUM_INPUTS; i++)
        le_process_image_set_bool(img, LE_ADDR_MAKE_DIN(i), !gpio_get(INPUT_PINS[i])); /* active-low button */
}

void le_port_write_outputs(const le_process_image_t* img)
{
    if (!img) return;
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        gpio_put(OUTPUT_PINS[i], le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(i)) ? 1 : 0);
}

void le_port_deenergize_outputs(void)
{
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) gpio_put(OUTPUT_PINS[i], 0);
}

/* --------------------- HAL callbacks (via le_hal_t) --------------------- */

static bool port_hal_init(void) { le_port_init(); return true; }
static void port_hal_shutdown(void) { le_port_deenergize_outputs(); }
static const char* port_hal_get_platform_name(void) { return "RP2040 / Raspberry Pi Pico"; }

static bool port_gpio_read(uint8_t pin)
{
    if (pin < NUM_INPUTS) return !!gpio_get(INPUT_PINS[pin]);
    if (pin < NUM_INPUTS + NUM_OUTPUTS) return !!gpio_get(OUTPUT_PINS[pin - NUM_INPUTS]);
    return false;
}
static void port_gpio_write(uint8_t pin, bool value)
{
    if (pin < NUM_OUTPUTS) gpio_put(OUTPUT_PINS[pin], value ? 1 : 0);
}

static float port_adc_read(uint8_t channel) { (void)channel; return 0.0f; }
static uint32_t port_adc_read_raw(uint8_t channel) { (void)channel; return 0UL; }
static void port_dac_write(uint8_t channel, float value) { (void)channel; (void)value; }
static uint32_t port_get_time_ms(void) { return to_ms_since_boot(get_absolute_time()); }
static uint64_t port_get_time_us(void) { return to_us_since_boot(get_absolute_time()); }
static size_t port_uart_available(void) { return 0; }
static size_t port_uart_read(uint8_t* buf, size_t max_len) { (void)buf; (void)max_len; return 0; }
static size_t port_uart_write(const uint8_t* buf, size_t len)
{
    if (!buf || len == 0) return 0;
    uart_write_blocking(UART_ID, buf, len);
    return len;
}

static bool port_storage_read(uint32_t offset, uint8_t* buf, size_t len)
{
    uint32_t rel = offset - LE_PORT_FLASH_BASE;
    if (rel + (uint32_t)len > sizeof(s_port_flash)) return false;
    memcpy(buf, &s_port_flash[rel], len);
    return true;
}
static bool port_storage_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    uint32_t rel = offset - LE_PORT_FLASH_BASE;
    if (rel + (uint32_t)len > sizeof(s_port_flash)) return false;
    memcpy(&s_port_flash[rel], buf, len);
    return true;
}

#if LE_ENABLE_SERIAL_BUS
#include "hardware/i2c.h"
#include "hardware/spi.h"

static bool port_i2c_write(uint8_t addr, const uint8_t* tx, size_t len)
{
    if (!tx || len == 0) return true;
    return i2c_write_blocking(i2c_default, addr, tx, len, false) >= 0;
}
static bool port_i2c_read(uint8_t addr, uint8_t* rx, size_t len)
{
    if (!rx || len == 0) return true;
    return i2c_read_blocking(i2c_default, addr, rx, len, false) >= 0;
}
static bool port_i2c_write_read(uint8_t addr, const uint8_t* tx, size_t tl, uint8_t* rx, size_t rl)
{
    if (!port_i2c_write(addr, tx, tl)) return false;
    return port_i2c_read(addr, rx, rl);
}
static bool port_spi_transfer(uint8_t cs, const uint8_t* tx, uint8_t* rx, size_t len)
{
    gpio_put(cs, 0);
    int ret = 0;
    if (tx && rx) ret = spi_write_read_blocking(spi_default, tx, rx, len);
    else if (tx) ret = spi_write_blocking(spi_default, tx, len);
    else if (rx) ret = spi_read_blocking(spi_default, 0x00, rx, len);
    gpio_put(cs, 1);
    return ret >= 0;
}
#endif

static le_status_t port_ext_call(uint8_t func_id, const uint16_t* args,
                                 int in_count, int out_count, le_process_image_t* img)
{
    (void)func_id; (void)args; (void)in_count; (void)out_count; (void)img;
    return LE_OK; /* TODO: dispatch board custom nodes. */
}
static const char* port_get_custom_nodes_json(void) { return NULL; }

const le_hal_t s_my_mcu_hal = {
    .init = port_hal_init, .shutdown = port_hal_shutdown, .get_platform_name = port_hal_get_platform_name,
    .gpio_read = port_gpio_read, .gpio_write = port_gpio_write,
    .adc_read = port_adc_read, .adc_read_raw = port_adc_read_raw, .dac_write = port_dac_write,
    .get_time_ms = port_get_time_ms, .get_time_us = port_get_time_us,
    .uart_available = port_uart_available, .uart_read = port_uart_read, .uart_write = port_uart_write,
    .storage_read = port_storage_read, .storage_write = port_storage_write,
#if LE_ENABLE_SERIAL_BUS
    .i2c_write = port_i2c_write, .i2c_read = port_i2c_read, .i2c_write_read = port_i2c_write_read,
    .spi_transfer = port_spi_transfer,
#endif
    .ext_call = port_ext_call, .get_custom_nodes_json = port_get_custom_nodes_json,
};

const le_hal_t* get_my_mcu_hal(void) { return &s_my_mcu_hal; }
