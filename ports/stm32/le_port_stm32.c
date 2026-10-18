
/**
 * @file le_port_stm32.c  STM32 Board Port (STM32Cube HAL) - v3 / le_hal_t contract.
 */

#include "le_port.h"
#include "le_storage.h"
#include <string.h>

#if defined(STM32F4) || defined(STM32F401xE) || defined(STM32F407xx) || defined(STM32F411xE)
#include "stm32f4xx_hal.h"
#elif defined(STM32F1) || defined(STM32F103xB)
#include "stm32f1xx_hal.h"
#elif defined(STM32G4) || defined(STM32G474xx)
#include "stm32g4xx_hal.h"
#elif defined(STM32H7)
#include "stm32h7xx_hal.h"
#elif defined(STM32L4)
#include "stm32l4xx_hal.h"
#else
#include "main.h"
#endif

#define LE_PORT_FLASH_BASE 0x08000000UL
static uint8_t s_port_flash[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT] = {0};

typedef struct { GPIO_TypeDef* port; uint16_t pin; } stm32_pin_t;
static const stm32_pin_t INPUT_PINS[] = {
    { GPIOA, GPIO_PIN_0 }, { GPIOA, GPIO_PIN_1 },
    { GPIOA, GPIO_PIN_4 }, { GPIOC, GPIO_PIN_13 },
};
#define NUM_INPUTS (sizeof(INPUT_PINS) / sizeof(INPUT_PINS[0]))
static const stm32_pin_t OUTPUT_PINS[] = {
    { GPIOA, GPIO_PIN_5 }, { GPIOB, GPIO_PIN_0 },
    { GPIOB, GPIO_PIN_7 }, { GPIOB, GPIO_PIN_14 },
};
#define NUM_OUTPUTS (sizeof(OUTPUT_PINS) / sizeof(OUTPUT_PINS[0]))
extern UART_HandleTypeDef huart2;

void le_port_init(void) { /* CubeMX MX_GPIO_Init handles setup. */ }

void le_port_read_inputs(le_process_image_t* img)
{
    if (!img) return;
    for (uint8_t i = 0; i < NUM_INPUTS; i++) {
        GPIO_PinState st = HAL_GPIO_ReadPin(INPUT_PINS[i].port, INPUT_PINS[i].pin);
        le_process_image_set_bool(img, LE_ADDR_MAKE_DIN(i), st == GPIO_PIN_SET);
    }
}

void le_port_write_outputs(const le_process_image_t* img)
{
    if (!img) return;
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        bool s = le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(i));
        HAL_GPIO_WritePin(OUTPUT_PINS[i].port, OUTPUT_PINS[i].pin, s ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void le_port_deenergize_outputs(void)
{
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        HAL_GPIO_WritePin(OUTPUT_PINS[i].port, OUTPUT_PINS[i].pin, GPIO_PIN_RESET);
}

/* --------------------- HAL callbacks (via le_hal_t) --------------------- */

static bool port_hal_init(void) { le_port_init(); return true; }
static void port_hal_shutdown(void) { le_port_deenergize_outputs(); }
static const char* port_hal_get_platform_name(void) { return "STM32 (STM32Cube HAL)"; }

static bool port_gpio_read(uint8_t pin)
{
    if (pin < NUM_INPUTS)
        return HAL_GPIO_ReadPin(INPUT_PINS[pin].port, INPUT_PINS[pin].pin) == GPIO_PIN_SET;
    if (pin < NUM_INPUTS + NUM_OUTPUTS) {
        uint8_t o = (uint8_t)(pin - NUM_INPUTS);
        return HAL_GPIO_ReadPin(OUTPUT_PINS[o].port, OUTPUT_PINS[o].pin) == GPIO_PIN_SET;
    }
    return false;
}
static void port_gpio_write(uint8_t pin, bool value)
{
    if (pin < NUM_OUTPUTS)
        HAL_GPIO_WritePin(OUTPUT_PINS[pin].port, OUTPUT_PINS[pin].pin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static float port_adc_read(uint8_t channel) { (void)channel; return 0.0f; }
static uint32_t port_adc_read_raw(uint8_t channel) { (void)channel; return 0UL; }
static void port_dac_write(uint8_t channel, float value) { (void)channel; (void)value; }
static uint32_t port_get_time_ms(void) { return HAL_GetTick(); }
static uint64_t port_get_time_us(void) { return (uint64_t)HAL_GetTick() * 1000ULL; }
static bool port_set_timer_rate(uint32_t rate_hz, uint32_t period_us)
{
    (void)rate_hz; (void)period_us;
    /* Optional: Hardware timer (e.g. TIM2 / TIM3) ARR/PSC reload configuration */
    return true;
}
static size_t port_uart_available(void) { return 0; }
static size_t port_uart_read(uint8_t* buf, size_t max_len) { (void)buf; (void)max_len; return 0; }
static size_t port_uart_write(const uint8_t* buf, size_t len)
{
    if (!buf || len == 0) return 0;
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, (uint16_t)len, 50);
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
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
static bool port_i2c_write(uint8_t addr, const uint8_t* tx, size_t len)
{
    if (!tx || len == 0) return true;
    return HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(addr << 1), (uint8_t*)tx, (uint16_t)len, 50) == HAL_OK;
}
static bool port_i2c_read(uint8_t addr, uint8_t* rx, size_t len)
{
    if (!rx || len == 0) return true;
    return HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(addr << 1), rx, (uint16_t)len, 50) == HAL_OK;
}
static bool port_i2c_write_read(uint8_t addr, const uint8_t* tx, size_t tl, uint8_t* rx, size_t rl)
{
    if (!port_i2c_write(addr, tx, tl)) return false;
    return port_i2c_read(addr, rx, rl);
}
static bool port_spi_transfer(uint8_t cs, const uint8_t* tx, uint8_t* rx, size_t len)
{
    HAL_GPIO_WritePin(GPIOA, (uint16_t)(1U << cs), GPIO_PIN_RESET);
    HAL_StatusTypeDef st = HAL_OK;
    if (tx && rx) st = HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)tx, rx, (uint16_t)len, 50);
    else if (tx) st = HAL_SPI_Transmit(&hspi1, (uint8_t*)tx, (uint16_t)len, 50);
    else if (rx) st = HAL_SPI_Receive(&hspi1, rx, (uint16_t)len, 50);
    HAL_GPIO_WritePin(GPIOA, (uint16_t)(1U << cs), GPIO_PIN_SET);
    return (st == HAL_OK);
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
