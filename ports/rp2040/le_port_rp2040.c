
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
static uint8_t s_port_flash[LE_SLOT_SIZE_BYTES * (LE_MAX_CONFIG_SLOTS + 1)] = {0};

/* %DIN[0..n] -> Pico GPIO numbers */
static const uint8_t INPUT_PINS[] = { 14, 15, 16, 17 };
#define NUM_INPUTS (sizeof(INPUT_PINS) / sizeof(INPUT_PINS[0]))
/* %DOUT[0..n] -> Pico GPIO numbers */
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
static bool port_set_timer_rate(uint32_t rate_hz, uint32_t period_us) { (void)rate_hz; (void)period_us; return true; }
static size_t port_uart_available(void) { return 0; }
static size_t port_uart_read(uint8_t* buf, size_t max_len) { (void)buf; (void)max_len; return 0; }
static size_t port_uart_write(const uint8_t* buf, size_t len)
{
    if (!buf || len == 0) return 0;
    uart_write_blocking(UART_ID, buf, len);
    return len;
}

static bool port_storage_read(const le_storage_sector_t* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len)
{
    uint32_t rel = (sector ? sector->phys_addr : 0) + offset_in_sector;
    if (rel + (uint32_t)len > sizeof(s_port_flash)) return false;
    memcpy(buf, &s_port_flash[rel], len);
    return true;
}

static bool port_storage_erase_sector(const le_storage_sector_t* sector)
{
    uint32_t rel = sector ? sector->phys_addr : 0;
    uint32_t sz = (sector && sector->erase_size > 0) ? sector->erase_size : LE_STORAGE_SECTOR_SIZE;
    if (rel + sz > sizeof(s_port_flash)) return false;
    memset(&s_port_flash[rel], 0xFF, sz);
    return true;
}

static bool port_storage_write_page(const le_storage_sector_t* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len)
{
    uint32_t rel = (sector ? sector->phys_addr : 0) + offset_in_sector;
    if (rel + (uint32_t)len > sizeof(s_port_flash)) return false;
    memcpy(&s_port_flash[rel], buf, len);
    return true;
}

static const uint8_t* port_storage_get_xip_ptr(const le_storage_sector_t* sector, uint32_t offset_in_sector)
{
    uint32_t rel = (sector ? sector->phys_addr : 0) + offset_in_sector;
    if (rel < sizeof(s_port_flash)) return &s_port_flash[rel];
    return NULL;
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

#include "le_custom_node.h"

static le_status_t port_ext_call(uint8_t func_id, const uint16_t* args,
                                 int in_count, int out_count, le_process_image_t* img)
{
    return le_custom_nodes_dispatch(func_id, args, in_count, out_count, img);
}

static const char* port_get_custom_nodes_json(void)
{
    return le_custom_nodes_get_json();
}

const le_hal_t s_my_mcu_hal = {
    .init = port_hal_init,
    .shutdown = port_hal_shutdown,
    .get_platform_name = port_hal_get_platform_name,
    .gpio_read = port_gpio_read,
    .gpio_write = port_gpio_write,
    .adc_read = port_adc_read,
    .adc_read_raw = port_adc_read_raw,
    .dac_write = port_dac_write,
    .get_time_ms = port_get_time_ms,
    .get_time_us = port_get_time_us,
    .set_timer_rate = port_set_timer_rate,
    .uart_available = port_uart_available,
    .uart_read = port_uart_read,
    .uart_write = port_uart_write,
    .storage_read = port_storage_read,
    .storage_erase_sector = port_storage_erase_sector,
    .storage_write_page = port_storage_write_page,
    .storage_get_xip_ptr = port_storage_get_xip_ptr,
#if LE_ENABLE_SERIAL_BUS
    .i2c_write = port_i2c_write,
    .i2c_read = port_i2c_read,
    .i2c_write_read = port_i2c_write_read,
    .spi_transfer = port_spi_transfer,
#endif
    .ext_call = port_ext_call,
    .get_custom_nodes_json = port_get_custom_nodes_json,
};

const le_hal_t* get_my_mcu_hal(void) { return &s_my_mcu_hal; }
 
#if (LE_CORE_MODE == LE_CORE_AMP)
#include "pico/multicore.h"
#include "hardware/sync.h"

static le_vm_t* s_amp_vm = NULL;

static void core1_amp_worker(void)
{
    while (1) {
        uint32_t timestamp_ms = multicore_fifo_pop_blocking();
        if (timestamp_ms == 0xDEADBEEFU) {
            break;
        }
        if (s_amp_vm) {
            le_vm_step(s_amp_vm, timestamp_ms);
        }
        multicore_fifo_push_blocking(0xCAFEFEEDA);
    }
}

void le_port_amp_start(le_vm_t* vm)
{
    s_amp_vm = vm;
    multicore_reset_core1();
    multicore_launch_core1(core1_amp_worker);
}

bool le_port_amp_step(le_vm_t* vm, uint32_t timeout_us)
{
    (void)vm;
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    multicore_fifo_push_blocking(now_ms);

    uint32_t ack = 0;
    bool ok = multicore_fifo_pop_timeout_us(timeout_us, &ack);
    if (!ok || ack != 0xCAFEFEEDA) {
        /* Watchdog violation: Core 1 failed to complete scan in time */
        le_port_deenergize_outputs();
        return false;
    }
    return true;
}
#endif
