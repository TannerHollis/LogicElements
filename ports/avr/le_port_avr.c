
/**
 * @file le_port_avr.c  Microchip/Atmel AVR Board Port (ATmega328P, ATmega2560, ...)
 *                      Arduino framework + bare-metal (avr-gcc / MPLAB XC8).
 *                      v3 / le_hal_t contract.
 */

#include "le_port.h"
#include "le_storage.h"
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>

static const uint8_t INP[] = { 2, 3, 4, 5 };
static const uint8_t OUT[] = { 13, 8, 9, 10 };
#define NUM_INPUTS (sizeof(INP) / sizeof(INP[0]))
#define NUM_OUTPUTS (sizeof(OUT) / sizeof(OUT[0]))

void le_port_init(void)
{
    for (uint8_t i = 0; i < NUM_INPUTS; i++) pinMode(INP[i], INPUT_PULLUP);
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) { pinMode(OUT[i], OUTPUT); digitalWrite(OUT[i], LOW); }
}

void le_port_read_inputs(le_process_image_t* img)
{
    if (!img) return;
    for (uint8_t i = 0; i < NUM_INPUTS; i++)
        le_process_image_set_bool(img, LE_ADDR_MAKE_DIN(i), digitalRead(INP[i]) == LOW);
}

void le_port_write_outputs(const le_process_image_t* img)
{
    if (!img) return;
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        digitalWrite(OUT[i], le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(i)) ? HIGH : LOW);
}

void le_port_deenergize_outputs(void)
{
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) digitalWrite(OUT[i], LOW);
}

/* --------------------- HAL callbacks (via le_hal_t) --------------------- */

static bool port_hal_init(void) { le_port_init(); return true; }
static void port_hal_shutdown(void) { le_port_deenergize_outputs(); }
static const char* port_hal_get_platform_name(void) { return "AVR / Arduino (ATmega328P)"; }

static bool port_gpio_read(uint8_t pin)
{
    if (pin < NUM_INPUTS) return digitalRead(INP[pin]) == LOW;
    if (pin < NUM_INPUTS + NUM_OUTPUTS) return digitalRead(OUT[pin - NUM_INPUTS]) == HIGH;
    return false;
}
static void port_gpio_write(uint8_t pin, bool value)
{
    if (pin < NUM_OUTPUTS) digitalWrite(OUT[pin], value ? HIGH : LOW);
}
static float port_adc_read(uint8_t channel) { return (float)analogRead(channel) / 1023.0f; }
static uint32_t port_adc_read_raw(uint8_t channel) { return analogRead(channel); }
static void port_dac_write(uint8_t channel, float value) { (void)channel; (void)value; }
static uint32_t port_get_time_ms(void) { return millis(); }
static uint64_t port_get_time_us(void) { return (uint64_t)micros(); }
static bool port_set_timer_rate(uint32_t rate_hz, uint32_t period_us) { (void)rate_hz; (void)period_us; return true; }
static size_t port_uart_available(void) { return (size_t)Serial.available(); }
static size_t port_uart_read(uint8_t* buf, size_t max_len)
{
    size_t n = 0;
    while (n < max_len && Serial.available()) buf[n++] = (uint8_t)Serial.read();
    return n;
}
static size_t port_uart_write(const uint8_t* buf, size_t len)
{
    if (!buf || len == 0) return 0;
    Serial.write(buf, (int)len);
    return len;
}
static bool port_storage_read(const le_storage_sector_t* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len)
{
    uint32_t rel = (sector ? sector->phys_addr : 0) + offset_in_sector;
    static uint8_t flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};
    if (rel + (uint32_t)len > sizeof(flash)) return false;
    memcpy(buf, &flash[rel], len);
    return true;
}
static bool port_storage_erase_sector(const le_storage_sector_t* sector)
{
    uint32_t rel = sector ? sector->phys_addr : 0;
    uint32_t sz = (sector && sector->erase_size > 0) ? sector->erase_size : LE_STORAGE_SECTOR_SIZE;
    static uint8_t flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};
    if (rel + sz > sizeof(flash)) return false;
    memset(&flash[rel], 0xFF, sz);
    return true;
}
static bool port_storage_write_page(const le_storage_sector_t* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len)
{
    uint32_t rel = (sector ? sector->phys_addr : 0) + offset_in_sector;
    static uint8_t flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};
    if (rel + (uint32_t)len > sizeof(flash)) return false;
    memcpy(&flash[rel], buf, len);
    return true;
}
static const uint8_t* port_storage_get_xip_ptr(const le_storage_sector_t* sector, uint32_t offset_in_sector)
{
    (void)sector; (void)offset_in_sector;
    return NULL;
}

#if LE_ENABLE_SERIAL_BUS
static bool port_i2c_write(uint8_t addr, const uint8_t* tx, size_t len)
{
    (void)addr; (void)tx; (void)len;
    return true; /* TODO: Arduino Wire library. */
}
static bool port_i2c_read(uint8_t addr, uint8_t* rx, size_t len)
{
    (void)addr; (void)rx; (void)len;
    return true; /* TODO: Arduino Wire library. */
}
static bool port_i2c_write_read(uint8_t addr, const uint8_t* tx, size_t tl, uint8_t* rx, size_t rl)
{
    (void)addr; (void)tx; (void)tl; (void)rx; (void)rl;
    return true; /* TODO: Arduino Wire library. */
}
static bool port_spi_transfer(uint8_t cs, const uint8_t* tx, uint8_t* rx, size_t len)
{
    (void)cs; (void)tx; (void)rx; (void)len;
    return true; /* TODO: Arduino SPI library. */
}
#endif

#include "le_custom_node.h"

static le_status_t port_ext_call(uint8_t func_id, const uint16_t* args,
                                 int in_count, int out_count, le_process_image_t* img)
{
    return le_custom_nodes_dispatch(func_id, args, in_count, out_count, img);
}
static const char* port_get_custom_nodes_json(void) { return le_custom_nodes_get_json(); }

const le_hal_t s_my_mcu_hal = {
    .init = port_hal_init, .shutdown = port_hal_shutdown, .get_platform_name = port_hal_get_platform_name,
    .gpio_read = port_gpio_read, .gpio_write = port_gpio_write,
    .adc_read = port_adc_read, .adc_read_raw = port_adc_read_raw, .dac_write = port_dac_write,
    .get_time_ms = port_get_time_ms, .get_time_us = port_get_time_us,
    .set_timer_rate = port_set_timer_rate,
    .uart_available = port_uart_available, .uart_read = port_uart_read, .uart_write = port_uart_write,
    .storage_read = port_storage_read,
    .storage_erase_sector = port_storage_erase_sector,
    .storage_write_page = port_storage_write_page,
    .storage_get_xip_ptr = port_storage_get_xip_ptr,
#if LE_ENABLE_SERIAL_BUS
    .i2c_write = port_i2c_write, .i2c_read = port_i2c_read, .i2c_write_read = port_i2c_write_read,
    .spi_transfer = port_spi_transfer,
#endif
    .ext_call = port_ext_call, .get_custom_nodes_json = port_get_custom_nodes_json,
};

const le_hal_t* get_my_mcu_hal(void) { return &s_my_mcu_hal; }
#else
/* Bare-metal AVR-GCC implementation (ATmega328P). */
#include <avr/io.h>
#include <avr/interrupt.h>

static volatile uint32_t s_millis = 0;

ISR(TIMER0_COMPA_vect) { s_millis++; }

void le_port_init(void)
{
    DDRD &= ~((1 << PD2) | (1 << PD3) | (1 << PD4));
    PORTD |= ((1 << PD2) | (1 << PD3) | (1 << PD4));
    DDRB |= ((1 << PB5) | (1 << PB0) | (1 << PB1));
    PORTB &= ~((1 << PB5) | (1 << PB0) | (1 << PB1));
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A = 249;
    TIMSK0 |= (1 << OCIE0A);
    sei();
    UBRR0H = 0; UBRR0L = 8;
    UCSR0A |= (1 << U2X0);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void le_port_read_inputs(le_process_image_t* img)
{
    if (!img) return;
    uint8_t p = PIND;
    le_process_image_set_bool(img, LE_ADDR_MAKE_DIN(0), !(p & (1 << PD2)));
    le_process_image_set_bool(img, LE_ADDR_MAKE_DIN(1), !(p & (1 << PD3)));
    le_process_image_set_bool(img, LE_ADDR_MAKE_DIN(2), !(p & (1 << PD4)));
}

void le_port_write_outputs(const le_process_image_t* img)
{
    if (!img) return;
    if (le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(0))) PORTB |= (1 << PB5); else PORTB &= ~(1 << PB5);
    if (le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(1))) PORTB |= (1 << PB0); else PORTB &= ~(1 << PB0);
    if (le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(2))) PORTB |= (1 << PB1); else PORTB &= ~(1 << PB1);
}

void le_port_deenergize_outputs(void)
{
    PORTB &= ~((1 << PB5) | (1 << PB0) | (1 << PB1));
}

static bool port_hal_init(void) { le_port_init(); return true; }
static void port_hal_shutdown(void) { le_port_deenergize_outputs(); }
static const char* port_hal_get_platform_name(void) { return "AVR / ATmega328P (bare-metal)"; }
static bool port_gpio_read(uint8_t pin) { return pin < 3 ? !(PIND & (1 << PD2)) : false; }
static void port_gpio_write(uint8_t pin, bool value)
{
    if (pin == 0) { if (value) PORTB |= (1 << PB5); else PORTB &= ~(1 << PB5); }
    else if (pin == 1) { if (value) PORTB |= (1 << PB0); else PORTB &= ~(1 << PB0); }
    else if (pin == 2) { if (value) PORTB |= (1 << PB1); else PORTB &= ~(1 << PB1); }
}
static float port_adc_read(uint8_t channel) { (void)channel; return 0.0f; }
static uint32_t port_adc_read_raw(uint8_t channel) { (void)channel; return 0UL; }
static void port_dac_write(uint8_t channel, float value) { (void)channel; (void)value; }
static uint32_t port_get_time_ms(void)
{
    uint8_t sreg = SREG; cli(); uint32_t ms = s_millis; SREG = sreg; return ms;
}
static uint64_t port_get_time_us(void) { return (uint64_t)port_get_time_ms() * 1000ULL; }
static bool port_set_timer_rate(uint32_t rate_hz, uint32_t period_us) { (void)rate_hz; (void)period_us; return true; }
static size_t port_uart_available(void) { return 0; }
static size_t port_uart_read(uint8_t* buf, size_t max_len) { (void)buf; (void)max_len; return 0; }
static size_t port_uart_write(const uint8_t* buf, size_t len)
{
    for (size_t i = 0; i < len; i++) { while (!(UCSR0A & (1 << UDRE0))); UDR0 = buf[i]; }
    return len;
}
static bool port_storage_read(const le_storage_sector_t* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len)
{
    uint32_t rel = (sector ? sector->phys_addr : 0) + offset_in_sector;
    static uint8_t flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};
    if (rel + (uint32_t)len > sizeof(flash)) return false;
    memcpy(buf, &flash[rel], len);
    return true;
}
static bool port_storage_erase_sector(const le_storage_sector_t* sector)
{
    uint32_t rel = sector ? sector->phys_addr : 0;
    uint32_t sz = (sector && sector->erase_size > 0) ? sector->erase_size : LE_STORAGE_SECTOR_SIZE;
    static uint8_t flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};
    if (rel + sz > sizeof(flash)) return false;
    memset(&flash[rel], 0xFF, sz);
    return true;
}
static bool port_storage_write_page(const le_storage_sector_t* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len)
{
    uint32_t rel = (sector ? sector->phys_addr : 0) + offset_in_sector;
    static uint8_t flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};
    if (rel + (uint32_t)len > sizeof(flash)) return false;
    memcpy(&flash[rel], buf, len);
    return true;
}
static const uint8_t* port_storage_get_xip_ptr(const le_storage_sector_t* sector, uint32_t offset_in_sector)
{
    (void)sector; (void)offset_in_sector;
    return NULL;
}

#if LE_ENABLE_SERIAL_BUS
static bool port_i2c_write(uint8_t addr, const uint8_t* tx, size_t len) { (void)addr; (void)tx; (void)len; return true; }
static bool port_i2c_read(uint8_t addr, uint8_t* rx, size_t len) { (void)addr; (void)rx; (void)len; return true; }
static bool port_i2c_write_read(uint8_t addr, const uint8_t* tx, size_t tl, uint8_t* rx, size_t rl)
{ (void)addr; (void)tx; (void)tl; (void)rx; (void)rl; return true; }
static bool port_spi_transfer(uint8_t cs, const uint8_t* tx, uint8_t* rx, size_t len)
{
    PORTB &= ~(1 << (cs & 0x07));
    for (size_t i = 0; i < len; i++) {
        SPDR = tx ? tx[i] : 0x00;
        while (!(SPSR & (1 << SPIF)));
        if (rx) rx[i] = SPDR;
    }
    PORTB |= (1 << (cs & 0x07));
    return true;
}
#endif
static le_status_t port_ext_call(uint8_t func_id, const uint16_t* args,
                                 int in_count, int out_count, le_process_image_t* img)
{
    return le_custom_nodes_dispatch(func_id, args, in_count, out_count, img);
}
static const char* port_get_custom_nodes_json(void) { return le_custom_nodes_get_json(); }

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
#endif
