/**
 * @file le_cli.h
 * @brief Interactive UART Terminal CLI & Secure XMODEM-CRC File Transfer.
 */

#ifndef LE_CLI_H
#define LE_CLI_H

#include "le_types.h"
#include "le_vm.h"
#include "le_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Operational modes for the UART command line interface.
 */
typedef enum {
    LE_CLI_MODE_NORMAL = 0, /**< Interactive text shell mode for issuing ASCII commands. */
    LE_CLI_MODE_XMODEM,     /**< Binary XMODEM-CRC file transfer mode. */
    LE_CLI_MODE_HEX         /**< Raw hexadecimal text stream upload mode. */
} le_cli_mode_t;

/**
 * @brief Command line interface and file transfer session context.
 */
typedef struct {
    le_vm_t*       vm;                      /**< Target virtual machine instance. */
    le_storage_t*  storage;                 /**< Target storage manager for slot writes. */
    le_cli_mode_t  mode;                    /**< Current parser mode. */

    /* Line Editing Buffer */
    char           line_buf[128];           /**< Buffer storing the current line input. */
    uint16_t       line_len;                /**< Length in bytes of valid line input. */
    bool           last_was_cr;             /**< Indicates previous byte was \r to collapse CRLF pairs. */

    /* Upload State */
    uint8_t        upload_slot;             /**< Target configuration slot for incoming program upload. */
    uint32_t       upload_offset;           /**< Byte offset within slot partition during transfer. */

    /* XMODEM-CRC State */
    uint8_t        xmodem_buf[133];         /**< Packet buffer for 128-byte XMODEM block plus headers and CRC. */
    uint16_t       xmodem_idx;              /**< Number of bytes collected in xmodem_buf. */
    uint8_t        xmodem_expected_block;   /**< Expected sequential block index (1 to 255). */
    uint8_t        xmodem_retries;          /**< Number of retry attempts for current block. */
    uint32_t       xmodem_last_c_ms;        /**< Timestamp in milliseconds of last 'C' synchronization poll. */

    /* Hex Paste State */
    uint8_t        hex_byte;                /**< Partially decoded byte accumulator. */
    bool           hex_high_nibble;         /**< Indicates whether the next character represents the high nibble. */
} le_cli_t;

/**
 * @brief Initializes the CLI terminal shell and transfer context.
 *
 * @param cli Pointer to the CLI structure to initialize.
 * @param vm Pointer to the virtual machine instance.
 * @param storage Pointer to the multi-slot storage manager.
 */
void le_cli_init(le_cli_t* cli, le_vm_t* vm, le_storage_t* storage);

/**
 * @brief Processes an incoming character from the UART receiver.
 *
 * @param cli Pointer to the CLI structure.
 * @param ch Incoming ASCII character or binary byte.
 */
void le_cli_process_char(le_cli_t* cli, uint8_t ch);

/**
 * @brief Periodic timer service handling protocol timeouts and polling.
 *
 * @param cli Pointer to the CLI structure.
 * @param now_ms Current monotonic system timestamp in milliseconds.
 */
void le_cli_poll(le_cli_t* cli, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* LE_CLI_H */
