/**
 * @file le_serial.h
 * @brief Cross-platform serial communication interface for LogicElements host tools.
 */

#ifndef LE_SERIAL_H
#define LE_SERIAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct le_serial le_serial_t;

/**
 * @brief Opens a serial port with 8N1 configuration at the requested baud rate.
 *
 * @param port_name Name or path of the serial port (e.g. "COM3", "\\\\.\\COM10", "/dev/ttyUSB0").
 * @param baud_rate Communication speed in bits per second (e.g. 115200).
 * @return Pointer to opaque serial handle on success, NULL on failure.
 */
le_serial_t* le_serial_open(const char* port_name, uint32_t baud_rate);

/**
 * @brief Closes an open serial port handle and releases associated system resources.
 *
 * @param port Serial port handle.
 */
void le_serial_close(le_serial_t* port);

/**
 * @brief Reads up to max_len bytes from the serial port within the specified timeout.
 *
 * @param port Serial port handle.
 * @param buffer Output buffer for received bytes.
 * @param max_len Maximum number of bytes to read into buffer.
 * @param timeout_ms Maximum time to wait in milliseconds.
 * @return Number of bytes actually read (>= 0), or negative value on I/O error.
 */
int le_serial_read(le_serial_t* port, uint8_t* buffer, size_t max_len, uint32_t timeout_ms);

/**
 * @brief Writes data to the serial port.
 *
 * @param port Serial port handle.
 * @param data Byte sequence to transmit.
 * @param len Number of bytes to transmit.
 * @return Number of bytes successfully transmitted, or negative value on error.
 */
int le_serial_write(le_serial_t* port, const uint8_t* data, size_t len);

/**
 * @brief Flushes pending input and output serial buffers.
 *
 * @param port Serial port handle.
 * @return 0 on success, negative value on error.
 */
int le_serial_flush(le_serial_t* port);

/**
 * @brief Discovers and populates available serial port names on the host system.
 *
 * @param ports Array of string buffers (each at least 64 bytes) to receive port names.
 * @param max_ports Maximum number of entries that can be written to the array.
 * @return Number of available ports discovered.
 */
int le_serial_list_ports(char ports[][64], size_t max_ports);

#ifdef __cplusplus
}
#endif

#endif /* LE_SERIAL_H */
