/**
 * @file le_host_comms.h
 * @brief Host-side framed binary packet protocol and MCU communication engine.
 */

#ifndef LE_HOST_COMMS_H
#define LE_HOST_COMMS_H

#include "le_types.h"
#include "le_comms.h"
#include "le_serial.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct le_host_comms le_host_comms_t;

/**
 * @brief Callback for monitoring program upload progress.
 *
 * @param uploaded_bytes Total bytes transferred so far.
 * @param total_bytes Total bytecode program length in bytes.
 * @param user_data Opaque pointer passed to the upload function.
 */
typedef void (*le_upload_progress_cb)(size_t uploaded_bytes, size_t total_bytes, void* user_data);

/**
 * @brief Opens serial communication to a target microcontroller.
 *
 * @param port_name Name of the serial port (e.g. "COM3" or "/dev/ttyUSB0").
 * @param baud_rate Baud rate in symbols/sec (typically 115200).
 * @return Pointer to host comms context on success, NULL on failure.
 */
le_host_comms_t* le_host_open(const char* port_name, uint32_t baud_rate);

/**
 * @brief Closes the host communication session and releases the serial port.
 *
 * @param client Host comms context.
 */
void le_host_close(le_host_comms_t* client);

/**
 * @brief Calculates the standard CCITT CRC-16 checksum for a byte array.
 *
 * @param crc Initial CRC state (typically 0xFFFF).
 * @param data Byte sequence to hash.
 * @param len Length in bytes.
 * @return Computed 16-bit CRC checksum.
 */
uint16_t le_host_crc16(uint16_t crc, const uint8_t* data, size_t len);

/**
 * @brief Formats and sends a framed binary packet over the serial link.
 *
 * @param client Host comms context.
 * @param cmd Protocol command opcode (e.g. LE_CMD_PING).
 * @param payload Pointer to packet payload bytes, or NULL if empty.
 * @param len Payload length in bytes (must be <= LE_COMMS_MAX_PAYLOAD).
 * @return 0 on success, negative value on error.
 */
int le_host_send_packet(le_host_comms_t* client, uint8_t cmd, const uint8_t* payload, uint16_t len);

/**
 * @brief Waits for and receives a complete valid framed binary packet.
 *
 * @param client Host comms context.
 * @param out_cmd Output pointer for the received command opcode.
 * @param out_seq Output pointer for the received packet sequence number.
 * @param out_payload Buffer to receive packet payload bytes.
 * @param max_payload_len Capacity of @p out_payload.
 * @param out_payload_len Output pointer for actual payload length.
 * @param timeout_ms Maximum time to wait in milliseconds.
 * @return 0 on success, negative value on timeout or protocol error.
 */
int le_host_recv_packet(
    le_host_comms_t* client,
    uint8_t* out_cmd,
    uint8_t* out_seq,
    uint8_t* out_payload,
    uint16_t max_payload_len,
    uint16_t* out_payload_len,
    uint32_t timeout_ms
);

/**
 * @brief Sends a ping request and waits for a pong response.
 *
 * @param client Host comms context.
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, negative on error.
 */
int le_host_ping(le_host_comms_t* client, uint32_t timeout_ms);

/**
 * @brief Queries board capabilities and memory limits from connected hardware.
 *
 * @param client Host comms context.
 * @param out_caps Destination structure for unpacked capabilities.
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, negative on error.
 */
int le_host_get_caps(le_host_comms_t* client, le_caps_payload_t* out_caps, uint32_t timeout_ms);

/**
 * @brief Queries custom hardware peripheral blocks JSON definitions from connected hardware.
 *
 * @param client Host comms context.
 * @param out_json Buffer to receive the JSON string.
 * @param max_json_len Maximum size of @p out_json.
 * @param timeout_ms Timeout in milliseconds.
 * @return Length of JSON string received, or negative on error.
 */
int le_host_get_custom_nodes(le_host_comms_t* client, char* out_json, size_t max_json_len, uint32_t timeout_ms);

/**
 * @brief Queries live deterministic timing and achievability model from hardware.
 *
 * @param client Host comms context.
 * @param out_timing Destination timing report structure.
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, negative on error.
 */
int le_host_get_timing(le_host_comms_t* client, le_timing_t* out_timing, uint32_t timeout_ms);

/**
 * @brief Sends runtime control commands (start, stop, reset).
 *
 * @param client Host comms context.
 * @param action 1=START, 2=STOP, 3=RESET.
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, negative on error.
 */
int le_host_control(le_host_comms_t* client, uint8_t action, uint32_t timeout_ms);

/**
 * @brief Activate a stored configuration slot on the device (load into the VM).
 *
 * @param client Host comms context.
 * @param slot Zero-based slot index to activate.
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, negative on error.
 */
int le_host_activate_slot(le_host_comms_t* client, uint8_t slot, uint32_t timeout_ms);

/**
 * @brief Forces a digital input or output to high or low for testing.
 *
 * @param client Host comms context.
 * @param addr Process-image bit address.
 * @param value 1 for high / true, 0 for low / false.
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, negative on error.
 */
int le_host_force_io(le_host_comms_t* client, uint16_t addr, uint8_t value, uint32_t timeout_ms);

/**
 * @brief Pulses a register high for a specified duration in milliseconds.
 *
 * @param client Host comms context.
 * @param addr Process-image register address.
 * @param duration_ms Pulse duration in milliseconds.
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, negative on error.
 */
int le_host_pulse(le_host_comms_t* client, uint16_t addr, uint32_t duration_ms, uint32_t timeout_ms);

/**
 * @brief Requests a live snapshot of the MCU process image (%IN, %OUT, %B).
 *
 * @param client Host comms context.
 * @param out_buf Buffer to receive raw bitfield payload.
 * @param max_len Capacity of @p out_buf.
 * @param out_len Number of bytes populated.
 * @param timeout_ms Timeout in milliseconds.
 * @return 0 on success, negative on error.
 */
int le_host_get_image(
    le_host_comms_t* client,
    uint8_t* out_buf,
    size_t max_len,
    size_t* out_len,
    uint32_t timeout_ms
);

/**
 * @brief Streams and uploads a compiled .lebin program to the target microcontroller.
 *
 * @param client Host comms context.
 * @param target_slot User slot to program (must not be the currently active slot).
 * @param lebin_data Raw binary bytecode payload.
 * @param lebin_size Length in bytes.
 * @param chunk_size Chunk size to transfer (recommended 64 bytes).
 * @param progress_cb Optional callback receiving progress updates.
 * @param user_data Opaque pointer passed to callback.
 * @param error_buf Optional buffer to receive human-readable error description on failure.
 * @param error_buf_size Size of @p error_buf.
 * @return 0 on success, negative error code on failure.
 */
int le_host_upload_program(
    le_host_comms_t* client,
    uint8_t target_slot,
    const uint8_t* lebin_data,
    size_t lebin_size,
    uint16_t chunk_size,
    le_upload_progress_cb progress_cb,
    void* user_data,
    char* error_buf,
    size_t error_buf_size
);

/**
 * @brief Runs an interactive text terminal session connected to MCU's embedded le_cli.
 *
 * @param client Host comms context.
 */
void le_host_run_terminal(le_host_comms_t* client);

#ifdef __cplusplus
}
#endif

#endif /* LE_HOST_COMMS_H */
