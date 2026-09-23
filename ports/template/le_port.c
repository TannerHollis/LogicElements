/**
 * @file le_port.c
 * @brief Starter Board Port Template (v3 / le_hal_t contract).
 *
 * This is the canonical reference port for custom microcontrollers. It:
 *   - defines GPIO/ADC/UART/storage/I2C/SPI callback implementations,
 *   - assembles them into a complete `le_hal_t` table (`get_my_mcu_hal()`),
 *   - provides the scan-loop helpers `le_port_init`, `le_port_read_inputs`,
 *     `le_port_write_outputs`, `le_port_deenergize_outputs`.
 *
 * Copy this file and `le_port.h` into your project, fill in the TODO hardware
 * calls, and register the table with `le_hal_set(get_my_mcu_hal())`.
 */

#include "le_port.h"
#include "le_storage.h"
#include <string.h>

/* ========================================================================== */
/* User-editable config                                                       */
/* ========================================================================== */

/* Flash offset where LogicElements stores its multi-slot program partitions. */
#define LE_PORT_FLASH_BASE  0x08000000UL

/* Example static RAM flash mirror so the template is self-contained without a
 * real storage driver. On real hardware, back this array with flash/EEPROM. */
static uint8_t s_port_flash[LE_SLOT_SIZE_BYTES * LE_MAX_CONFIG_SLOTS] = {0};

/* ========================================================================== */
/* Pin mapping configuration                                                  */
/* ========================================================================== */

typedef struct {
    uint8_t port_id;
    uint8_t pin_num;
} le_pin_map_t;

static const le_pin_map_t INPUT_MAP[] = {
    { 0, 0 }, /* %I[0] -> Port 0, Pin 0 */
    { 0, 1 }, /* %I[1] -> Port 0, Pin 1 */
    { 0, 2 }, /* %I[2] -> Port 0, Pin 2 */
    { 0, 3 }, /* %I[3] -> Port 0, Pin 3 */
};
#define NUM_INPUTS (sizeof(INPUT_MAP) / sizeof(INPUT_MAP[0]))

static const le_pin_map_t OUTPUT_MAP[] = {
    { 1, 0 }, /* %Q[0] -> Port 1, Pin 0 */
    { 1, 1 }, /* %Q[1] -> Port 1, Pin 1 */
    { 1, 2 }, /* %Q[2] -> Port 1, Pin 2 */
    { 1, 3 }, /* %Q[3] -> Port 1, Pin 3 */
};
#define NUM_OUTPUTS (sizeof(OUTPUT_MAP) / sizeof(OUTPUT_MAP[0]))

/* ========================================================================== */
/* Scan-loop helpers (used by the documented main() loop)                     */
/* ========================================================================== */

void le_port_init(void)
{
    /* TODO: Configure INPUT_MAP pins as Digital Inputs (with pull-up if needed) */
    /* TODO: Configure OUTPUT_MAP pins as Digital Outputs (push-pull) */
}

void le_port_read_inputs(le_process_image_t* img)
{
    if (!img) return;

    for (uint8_t i = 0; i < NUM_INPUTS; i++)
    {
        bool pin_state = false;
        /* TODO: pin_state = (MY_MCU_GPIO_READ(INPUT_MAP[i].port_id, INPUT_MAP[i].pin_num) != 0); */
        (void)pin_state;
        le_process_image_set_bool(img, LE_ADDR_MAKE_DIN(i), pin_state);
    }
}

void le_port_write_outputs(const le_process_image_t* img)
{
    if (!img) return;

    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    {
        bool state = le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(i));
        /* TODO: MY_MCU_GPIO_WRITE(OUTPUT_MAP[i].port_id, OUTPUT_MAP[i].pin_num, state); */
        (void)state;
    }
}

void le_port_deenergize_outputs(void)
{
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    {
        /* TODO: MY_MCU_GPIO_WRITE(OUTPUT_MAP[i].port_id, OUTPUT_MAP[i].pin_num, false); */
    }
}/* ========================================================================== */
/* HAL callback implementations (dispatched through le_hal_t)                 */
/* ========================================================================== */

static bool port_hal_init(void)
{
    le_port_init();
    return true;
}

static void port_hal_shutdown(void)
{
    le_port_deenergize_outputs();
}

static const char* port_hal_get_platform_name(void)
{
    return "Custom MCU";
}

static bool port_gpio_read(uint8_t pin)
{
    (void)pin;
    return false; /* TODO: read physical pin */
}

static void port_gpio_write(uint8_t pin, bool value)
{
    (void)pin; (void)value; /* TODO: write physical pin */
}

static float port_adc_read(uint8_t channel)
{
    (void)channel;
    return 0.0f; /* TODO: read ADC channel as scaled float */
}

static uint32_t port_adc_read_raw(uint8_t channel)
{
    (void)channel;
    return 0UL; /* TODO: read ADC channel raw value */
}

static void port_dac_write(uint8_t channel, float value)
{
    (void)channel; (void)value; /* TODO: write DAC channel */
}

static uint32_t port_get_time_ms(void)
{
    /* TODO: return current millisecond tick (e.g. SysTick or free-running timer) */
    return 0;
}

static uint64_t port_get_time_us(void)
{
    /* TODO: return current microsecond timestamp */
    return (uint64_t)port_get_time_ms() * 1000ULL;
}

static size_t port_uart_available(void)
{
    return 0; /* TODO: return number of buffered UART RX bytes */
}

static size_t port_uart_read(uint8_t* buf, size_t max_len)
{
    (void)buf; (void)max_len;
    return 0; /* TODO: non-blocking read into buf */
}

static size_t port_uart_write(const uint8_t* buf, size_t len)
{
    (void)buf; (void)len;
    /* TODO: transmit len bytes over UART */
    return len;
}

static bool port_storage_read(uint32_t offset, uint8_t* buf, size_t len)
{
    uint32_t base = LE_PORT_FLASH_BASE;
    uint32_t rel = offset - base;
    if (rel + (uint32_t)len > sizeof(s_port_flash)) return false;
    memcpy(buf, &s_port_flash[rel], len);
    return true;
}

static bool port_storage_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    uint32_t base = LE_PORT_FLASH_BASE;
    uint32_t rel = offset - base;
    if (rel + (uint32_t)len > sizeof(s_port_flash)) return false;
    memcpy(&s_port_flash[rel], buf, len);
    return true;
}

#if LE_ENABLE_SERIAL_BUS
static bool port_i2c_write(uint8_t addr_7bit, const uint8_t* tx_data, size_t len)
{
    (void)addr_7bit; (void)tx_data; (void)len;
    return true; /* TODO: perform I2C write */
}

static bool port_i2c_read(uint8_t addr_7bit, uint8_t* rx_data, size_t len)
{
    (void)addr_7bit; (void)rx_data; (void)len;
    return true; /* TODO: perform I2C read */
}

static bool port_i2c_write_read(uint8_t addr_7bit, const uint8_t* tx_data, size_t tx_len,
                                uint8_t* rx_data, size_t rx_len)
{
    (void)addr_7bit; (void)tx_data; (void)tx_len; (void)rx_data; (void)rx_len;
    return true; /* TODO: perform I2C write-then-read */
}

static bool port_spi_transfer(uint8_t cs_pin, const uint8_t* tx_data, uint8_t* rx_data, size_t len)
{
    (void)cs_pin; (void)tx_data; (void)rx_data; (void)len;
    return true; /* TODO: perform SPI transfer with chip-select control */
}
#endif

static le_status_t port_ext_call(uint8_t func_id, const uint16_t* args,
                                 int in_count, int out_count, le_process_image_t* img)
{
    (void)func_id; (void)args; (void)in_count; (void)out_count; (void)img;
    /* TODO: dispatch board-specific custom nodes (func_id >= LE_FUNC_CUSTOM_BASE) */
    return LE_OK;
}

static const char* port_get_custom_nodes_json(void)
{
    return NULL; /* TODO: return JSON array declaring board custom nodes */
}

/* ========================================================================== */
/* HAL table                                                                  */
/* ========================================================================== */

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

    .uart_available = port_uart_available,
    .uart_read = port_uart_read,
    .uart_write = port_uart_write,

    .storage_read = port_storage_read,
    .storage_write = port_storage_write,

#if LE_ENABLE_SERIAL_BUS
    .i2c_write = port_i2c_write,
    .i2c_read = port_i2c_read,
    .i2c_write_read = port_i2c_write_read,
    .spi_transfer = port_spi_transfer,
#endif

    .ext_call = port_ext_call,
    .get_custom_nodes_json = port_get_custom_nodes_json,
};

const le_hal_t* get_my_mcu_hal(void)
{
    return &s_my_mcu_hal;
}
