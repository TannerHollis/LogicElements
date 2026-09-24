/**
 * @file le_loader.h
 * @brief Binary loader and integrity verifier for LogicElements (.lebin).
 */

#ifndef LE_LOADER_H
#define LE_LOADER_H

#include "le_types.h"
#include "le_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Computes standard IEEE 802.3 CRC32 over a byte sequence.
 *
 * @param data Pointer to the input byte buffer.
 * @param length Total number of bytes to hash.
 * @return Returns the computed 32-bit CRC checksum.
 */
uint32_t le_crc32(const uint8_t* data, size_t length);

/**
 * @brief Validates the structural integrity and bounds of a .lebin binary image.
 *
 * Verifies the 26-byte header magic identifier, version compatibility, resource
 * bounds against compile-time limits, payload size, and IEEE 802.3 CRC32 checksum.
 *
 * @param buffer Pointer to the binary buffer in flash or RAM.
 * @param size Total byte size of the binary buffer.
 * @param out_header Optional pointer to an @ref le_header_t struct to receive parsed metadata (can be NULL).
 * @return Returns @ref LE_OK if the binary is valid, or a corresponding error code from @ref le_status_t.
 */
le_status_t le_loader_validate(const uint8_t* buffer, size_t size, le_header_t* out_header);

/**
 * @brief Validates and activates a .lebin bytecode program into the virtual machine.
 *
 * Performs integrity checks with @ref le_loader_validate and configures the virtual
 * machine instruction pointer without copying memory, supporting zero-copy Execute-In-Place (XIP).
 *
 * @param vm Pointer to the virtual machine instance.
 * @param buffer Pointer to the binary image buffer.
 * @param size Total byte size of the buffer.
 * @return Returns @ref LE_OK on success, or an error code from @ref le_status_t if validation fails.
 */
le_status_t le_loader_load(le_vm_t* vm, const uint8_t* buffer, size_t size);

/**
 * @brief Computes the timing / achievability report for the loaded program.
 *
 * Combines the binary's compiler cost (le_timing_desc_t), the board's
 * calibrated LE_NS_PER_ABSTRACT_CYCLE cost model, and the VM's configured
 * scan period (le_vm_set_scan_period_us). If the period is configured and the
 * worst-case scan does not fit, `feasible` is 0 and loading is rejected with
 * @ref LE_ERR_TIMING_BUDGET.
 *
 * @param vm The VM that a program was (or was not) loaded into.
 * @param out Pointer receiving the timing report (must be non-NULL).
 * @return Returns @ref LE_OK on success, or @ref LE_ERR_NULL_PTR if inputs are NULL.
 */
le_status_t le_loader_timing(const le_vm_t* vm, le_timing_t* out);

#ifdef __cplusplus
}
#endif

#endif /* LE_LOADER_H */
