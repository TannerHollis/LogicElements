/**
 * @file le_port_rp2350.c  RP2350 / Raspberry Pi Pico 2 W Board Port (Pico C/C++ SDK)
 *                        v3 / le_hal_t contract.
 *
 * Bridges the platform-independent LogicElements runtime to the RP2350. The
 * console and the LogicElements comms protocol run over USB CDC (the board's
 * virtual COM port) via TinyUSB - no hardware UART is used. The onboard LED on
 * the Pico 2 W lives on the wireless (CYW43439) chip, so it is driven through
 * the CYW43 driver only when LE_PORT_CYW43_LED is defined (linking
 * pico_cyw43_arch_none / pico_cyw43_driver). Otherwise %%Q0 is an external GPIO.
 */

#include "le_port.h"
#include "le_storage.h"
#include <string.h>
#include <stddef.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "tusb.h"

#ifdef LE_PORT_CYW43_LED
#include "pico/cyw43_arch.h"
#endif

/* g_le_hal / le_hal_set normally live in src/hal/le_hal_sim.c (desktop only).
 * This device port supplies them so the runtime sources link cleanly without
 * pulling in the simulator. */
const le_hal_t* g_le_hal = NULL;
void le_hal_set(const le_hal_t* hal) { g_le_hal = hal; }

/* ------------------------------------------------------------------------- */
/*  Pin mapping tables (isolated, easy to edit - does not touch the engine).  */
/* ------------------------------------------------------------------------- */
/* %%I[0..n] -> GPIO numbers (active-low pushbuttons / sensors, pulled up).   */
static const uint8_t INPUT_PINS[] = { 14, 15, 22, 13 };
#define NUM_INPUTS (sizeof(INPUT_PINS) / sizeof(INPUT_PINS[0]))

/* %%Q[0..n] -> GPIO numbers (external relays/LEDs). With LE_PORT_CYW43_LED the
 * first slot (%%Q0) drives the Pico 2 W onboard LED through the wireless chip. */
static const uint8_t OUTPUT_PINS[] = { 2, 3, 7 };
#define NUM_OUTPUTS (sizeof(OUTPUT_PINS) / sizeof(OUTPUT_PINS[0]))
#ifdef LE_PORT_CYW43_LED
#define LE_PORT_LED_OUT_IDX 0u
#endif

/* %%AIN[0..3] -> RP2350 ADC channels 0..3 on GP26..GP29. */
#define LE_ADC_CHANNELS 4
static const uint8_t ADC_INPUT_PINS[] = { 26, 27, 28, 29 };

/* RP2350 XIP flash base (differs from the RP2040 0x08000000). Storage is a
 * RAM shim for bring-up; real flash writes need flash_safe_execute()/sector
 * erase which is left as a follow-up. */
#define LE_PORT_FLASH_BASE 0x10000000UL
static uint8_t s_port_flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};

void le_port_init(void)
{
#ifdef LE_PORT_CYW43_LED
    cyw43_arch_init();
    /* GPIO2 (OUTPUT_PINS[0]) is unused while %%Q0 is the onboard LED. */
    for (uint8_t i = 1; i < NUM_OUTPUTS; i++) {
        gpio_init(OUTPUT_PINS[i]); gpio_set_dir(OUTPUT_PINS[i], GPIO_OUT); gpio_put(OUTPUT_PINS[i], 0);
    }
#else
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        gpio_init(OUTPUT_PINS[i]); gpio_set_dir(OUTPUT_PINS[i], GPIO_OUT); gpio_put(OUTPUT_PINS[i], 0);
    }
#endif

    for (uint8_t i = 0; i < NUM_INPUTS; i++) { gpio_init(INPUT_PINS[i]); gpio_set_dir(INPUT_PINS[i], GPIO_IN); gpio_pull_up(INPUT_PINS[i]); }

    /* RP2350 ADC: init once, route the four 12-bit channels on GP26..GP29. */
    adc_init();
    for (uint8_t i = 0; i < LE_ADC_CHANNELS; i++) adc_gpio_init(ADC_INPUT_PINS[i]);
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
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        bool raw = le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(i)) ? 1 : 0;
#ifdef LE_PORT_CYW43_LED
        if (i == LE_PORT_LED_OUT_IDX) { cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, raw ? 1 : 0); continue; }
#endif
        gpio_put(OUTPUT_PINS[i], raw ? 1 : 0);
    }
}

void le_port_deenergize_outputs(void)
{
#ifdef LE_PORT_CYW43_LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    for (uint8_t i = 1; i < NUM_OUTPUTS; i++) gpio_put(OUTPUT_PINS[i], 0);
#else
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) gpio_put(OUTPUT_PINS[i], 0);
#endif
}

/* --------------------- HAL callbacks (via le_hal_t) --------------------- */

static bool port_hal_init(void) { le_port_init(); return true; }
static void port_hal_shutdown(void) { le_port_deenergize_outputs(); }
static const char* port_hal_get_platform_name(void) { return "RP2350 / Raspberry Pi Pico 2 W"; }

static bool port_gpio_read(uint8_t pin)
{
    if (pin < NUM_INPUTS) return !!gpio_get(INPUT_PINS[pin]);
    if (pin < NUM_INPUTS + NUM_OUTPUTS) return !!gpio_get(OUTPUT_PINS[pin - NUM_INPUTS]);
    return false;
}
static void port_gpio_write(uint8_t pin, bool value)
{
    if (pin < NUM_OUTPUTS) {
#ifdef LE_PORT_CYW43_LED
        if (pin == LE_PORT_LED_OUT_IDX) { cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, value ? 1 : 0); return; }
#endif
        gpio_put(OUTPUT_PINS[pin], value ? 1 : 0);
    }
}

static float port_adc_read(uint8_t channel)
{
    if (channel >= LE_ADC_CHANNELS) return 0.0f;
    adc_select_input(channel);
    /* Scale the 12-bit reading to volts (0..3.3 V). */
    return (float)adc_read() * 3.3f / 4095.0f;
}
static uint32_t port_adc_read_raw(uint8_t channel)
{
    if (channel >= LE_ADC_CHANNELS) return 0UL;
    adc_select_input(channel);
    return (uint32_t)adc_read();
}
static void port_dac_write(uint8_t channel, float value) { (void)channel; (void)value; }

static uint32_t port_get_time_ms(void) { return to_ms_since_boot(get_absolute_time()); }
static uint64_t port_get_time_us(void) { return to_us_since_boot(get_absolute_time()); }

/* ------------------------------------------------------------------------- */
/*  Async USB-CDC transport.                                                 */
/*  The RP2350 USB-CDC data endpoint has NO hardware DMA channel - it is      */
/*  TinyUSB over USB IRQ + DPRAM. We make the I/O asynchronous in practice:   */
/*   - RX: USB IRQ -> TinyUSB RX FIFO (polled each scan).                     */
/*   - TX: writers enqueue into a ring buffer (never block on USB); a         */
/*         background flush (le_port_cdc_flush) pushes it to the CDC FIFO.    */
/*  This keeps large uploads/responses from stalling the PLC scan loop.       */
/* ------------------------------------------------------------------------- */
#define LE_CDC_TX_RING_SIZE 2048u /* must be a power of two */
static uint8_t           s_tx_ring[LE_CDC_TX_RING_SIZE];
static volatile uint16_t s_tx_head = 0; /* producer next-slot (index to fill) */
static volatile uint16_t s_tx_tail = 0; /* consumer next-slot (index to send) */

void le_port_cdc_flush(void)
{
    uint16_t mask = LE_CDC_TX_RING_SIZE - 1;
    if (!tud_cdc_connected()) { s_tx_tail = s_tx_head; return; } /* no host: drop queued */
    while (s_tx_tail != s_tx_head)
    {
        uint32_t avail = tud_cdc_write_available();
        if (avail == 0) break;
        uint16_t tail = s_tx_tail;
        uint32_t chunk = (s_tx_head > tail)
                         ? (uint32_t)(s_tx_head - tail)
                         : (uint32_t)(LE_CDC_TX_RING_SIZE - tail);
        if (chunk > avail) chunk = avail;
        if (chunk == 0) break;
        uint32_t w = tud_cdc_write(&s_tx_ring[tail], chunk);
        if (w == 0) break;
        s_tx_tail = (uint16_t)((s_tx_tail + w) & mask);
    }
    tud_cdc_write_flush();
}

static size_t port_uart_available(void) { return tud_cdc_connected() ? (size_t)tud_cdc_available() : 0u; }
static size_t port_uart_read(uint8_t* buf, size_t max_len)
{
    if (!buf || max_len == 0 || !tud_cdc_connected()) return 0;
    return tud_cdc_read(buf, max_len);
}
static size_t port_uart_write(const uint8_t* buf, size_t len)
{
    if (!buf || len == 0) return 0;
    uint16_t mask = LE_CDC_TX_RING_SIZE - 1;
    size_t written = 0;
    for (size_t i = 0; i < len; i++)
    {
        /* Ring full: flush to USB and wait a short, bounded time for room so we
         * never silently corrupt a protocol frame. ~4 ms budget, then give up. */
        uint32_t spins = 0;
        while (((uint16_t)((s_tx_head + 1 - s_tx_tail) & mask)) == 0)
        {
            le_port_cdc_flush();
            if (++spins > 80) return written; /* backpressure: drop remainder */
            busy_wait_us(50);
        }
        s_tx_ring[s_tx_head] = buf[i];
        s_tx_head = (uint16_t)((s_tx_head + 1) & mask);
        written++;
    }
    return written;
}

static bool port_storage_read(uint32_t offset, uint8_t* buf, size_t len)
{
    uint32_t rel = offset - LE_PORT_FLASH_BASE;
    if (offset < LE_PORT_FLASH_BASE) return false;
    if (rel + (uint32_t)len > sizeof(s_port_flash)) return false;
    memcpy(buf, &s_port_flash[rel], len);
    return true;
}
static bool port_storage_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    uint32_t rel = offset - LE_PORT_FLASH_BASE;
    if (offset < LE_PORT_FLASH_BASE) return false;
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
