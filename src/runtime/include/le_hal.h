/**
 * @file le_hal.h
 * @brief Hardware Abstraction Layer interface for LogicElements runtime.
 */

#ifndef LE_HAL_H
#define LE_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "le_types.h"
#include "le_process_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hardware Abstraction Layer (HAL) function pointer table.
 *
 * Defines the contract between the platform-independent LogicElements runtime
 * and target microcontroller hardware drivers.
 */
typedef struct {
    /* Platform lifecycle */
    bool        (*init)(void);
    void        (*shutdown)(void);
    const char* (*get_platform_name)(void);

    /* GPIO Digital I/O */
    bool        (*gpio_read)(uint8_t pin);
    void        (*gpio_write)(uint8_t pin, bool value);

    /* Analog I/O */
    float       (*adc_read)(uint8_t channel);
    uint32_t    (*adc_read_raw)(uint8_t channel);
    void        (*dac_write)(uint8_t channel, float value);

    /* System time */
    uint32_t    (*get_time_ms)(void);
    uint64_t    (*get_time_us)(void);

    /* UART communications */
    size_t      (*uart_available)(void);
    size_t      (*uart_read)(uint8_t* buf, size_t max_len);
    size_t      (*uart_write)(const uint8_t* buf, size_t len);

    /* Non-volatile storage (flash, EEPROM, or battery-backed SRAM) */
    bool        (*storage_read)(uint32_t offset, uint8_t* buf, size_t len);
    bool        (*storage_write)(uint32_t offset, const uint8_t* buf, size_t len);

#if LE_ENABLE_SERIAL_BUS
    /* I2C master bus callbacks */
    bool        (*i2c_write)(uint8_t addr_7bit, const uint8_t* tx_data, size_t len);
    bool        (*i2c_read)(uint8_t addr_7bit, uint8_t* rx_data, size_t len);
    bool        (*i2c_write_read)(uint8_t addr_7bit, const uint8_t* tx_data, size_t tx_len, uint8_t* rx_data, size_t rx_len);

    /* SPI master bus callback */
    bool        (*spi_transfer)(uint8_t cs_pin, const uint8_t* tx_data, uint8_t* rx_data, size_t len);
#endif

    /* Board custom node and external hardware callbacks */

    /**
     * @brief Executes a board-specific custom logic node or hardware routine.
     *
     * Invoked when the runtime engine encounters an @ref LE_OP_BLOCK whose
     * function id is >= @ref LE_FUNC_CUSTOM_BASE. Unlike the legacy two-input
     * form, the operand list is variable arity:
     *     args[0..in_count)      live inputs
     *     args[in_count..)       outputs
     * Operand data types are implied by their process-image address region.
     *
     * @param func_id Numeric function identifier assigned to the custom node.
     * @param args Address list: inputs first, then outputs.
     * @param in_count Number of input operand addresses in @p args.
     * @param out_count Number of output operand addresses in @p args (following inputs).
     * @param img Pointer to the active process image table.
     * @return Returns @ref LE_OK on successful execution; otherwise, returns an error status.
     */
    le_status_t (*ext_call)(uint8_t func_id, const uint16_t* args,
                            int in_count, int out_count, le_process_image_t* img);

    /**
     * @brief Retrieves the JSON string declaring all custom nodes supported by the board.
     *
     * @return Null-terminated JSON string describing custom nodes, or `NULL` if none exist.
     */
    const char* (*get_custom_nodes_json)(void);
} le_hal_t;

/**
 * @brief Global pointer to the active hardware abstraction layer table.
 */
extern const le_hal_t* g_le_hal;

/**
 * @brief Registers the active hardware abstraction layer implementation.
 *
 * Configures the global HAL pointer used by the runtime engine, communications,
 * and storage subsystems.
 *
 * @param hal Pointer to the board-specific HAL table.
 */
void le_hal_set(const le_hal_t* hal);

#ifdef __cplusplus
}
#endif

#endif /* LE_HAL_H */
