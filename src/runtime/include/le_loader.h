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
 * @brief Performs a bump-free online program swap into a running virtual machine.
 *
 * Validates the candidate program in @p buffer without modifying @p vm.
 * If valid, captures live outputs (%DOUT, %AOUT), common direct registers (%B, %F, %I, %C),
 * re-mapped symbolic alias values, and live internal state blocks (timers, counters, filters, PID),
 * loads the new bytecode program, and restores live state so execution continues seamlessly
 * without physical terminal glitches or dropping state.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param buffer Pointer to the candidate .lebin binary image.
 * @param size Total byte size of the binary buffer.
 * @return Returns @ref LE_OK on success, or an error code from @ref le_status_t if validation fails.
 */
le_status_t le_loader_swap_online(le_vm_t* vm, const uint8_t* buffer, size_t size);

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

/**
 * @brief Parses and validates individual chunks from a chunked container buffer (.lebin v13).
 *
 * Verifies the container header, total size, each chunk's boundaries, and each chunk's
 * independent CRC32 checksum.
 *
 * @param buffer Pointer to the container buffer.
 * @param size Total byte size of the buffer.
 * @param out_view Pointer to an @ref le_chunked_view_t struct receiving zero-copy pointers to chunks.
 * @return Returns @ref LE_OK on success, or an error code from @ref le_status_t if invalid.
 */
le_status_t le_loader_parse_chunks(const uint8_t* buffer, size_t size, le_chunked_view_t* out_view);

/**
 * @brief Converts a legacy v12 flat binary into an extensible v13 chunked container.
 *
 * @param flat_bin Pointer to valid v12 flat binary buffer.
 * @param flat_size Size in bytes of the flat binary.
 * @param out_buf Output buffer to receive the chunked container.
 * @param max_out_size Maximum capacity of out_buf.
 * @param out_size Pointer receiving the total written bytes of the container.
 * @return Returns @ref LE_OK on success, or an error code from @ref le_status_t.
 */
le_status_t le_loader_convert_to_chunked(const uint8_t* flat_bin, size_t flat_size,
                                        uint8_t* out_buf, size_t max_out_size, size_t* out_size);

/**
 * @brief Extended chunked conversion supporting optional multi-rate descriptor (Phase 4.2).
 *
 * @param flat_bin Pointer to validated flat .lebin payload.
 * @param flat_size Size of flat_bin in bytes.
 * @param rate Optional pointer to multi-rate descriptor chunk (or NULL).
 * @param out_buf Output buffer to receive the chunked container.
 * @param max_out_size Maximum capacity of out_buf.
 * @param out_size Pointer receiving the total written bytes of the container.
 * @return Returns @ref LE_OK on success, or an error code from @ref le_status_t.
 */
le_status_t le_loader_convert_to_chunked_ex(const uint8_t* flat_bin, size_t flat_size,
                                            const le_rate_desc_t* rate,
                                            uint8_t* out_buf, size_t max_out_size, size_t* out_size);

/**
 * @brief Extended chunked conversion supporting multi-rate and dynamic NVRAM descriptors.
 *
 * @param flat_bin Pointer to validated flat .lebin payload.
 * @param flat_size Size of flat_bin in bytes.
 * @param rate Optional pointer to multi-rate descriptor chunk (or NULL).
 * @param nvmd Optional pointer to non-volatile RAM requirements descriptor chunk (or NULL).
 * @param out_buf Output buffer to receive the chunked container.
 * @param max_out_size Maximum capacity of out_buf.
 * @param out_size Pointer receiving the total written bytes of the container.
 * @return Returns @ref LE_OK on success, or an error code from @ref le_status_t.
 */
le_status_t le_loader_convert_to_chunked_nv(const uint8_t* flat_bin, size_t flat_size,
                                            const le_rate_desc_t* rate,
                                            const le_nvram_desc_t* nvmd,
                                            uint8_t* out_buf, size_t max_out_size, size_t* out_size);

/**
 * @brief Signs an extensible chunked container (LEB2) by appending a "SIGN" chunk (Phase 4.4).
 *
 * @param in_container Pointer to input container buffer (must be LEB2 format).
 * @param in_size Size of in_container in bytes.
 * @param secret_key 64-byte Ed25519 expanded secret key.
 * @param out_buf Destination buffer for signed container.
 * @param max_out_size Capacity of destination buffer.
 * @param out_size Pointer receiving written bytes.
 * @return Returns LE_OK on success, or an error code from @ref le_status_t.
 */
le_status_t le_loader_sign_container(const uint8_t* in_container, size_t in_size,
                                     const uint8_t secret_key[32 + 32],
                                     uint8_t* out_buf, size_t max_out_size, size_t* out_size);

/**
 * @brief Configures the system-wide trusted Ed25519 public key in ROM/RAM.
 *
 * @param pk 32-byte Ed25519 public key (or NULL to disable signature requirement).
 */
void le_loader_set_trusted_key(const uint8_t pk[32]);

/**
 * @brief Retrieves the current trusted Ed25519 public key (or NULL if unconfigured).
 *
 * @return Pointer to 32-byte public key or NULL.
 */
const uint8_t* le_loader_get_trusted_key(void);

/**
 * @brief Validates a binary program with optional cryptographic Ed25519 signature verification (Phase 4.4).
 *
 * If @p public_key is NULL and no trusted key is configured, standard CRC32 and structural
 * validation is performed. If a public key is specified or trusted key is set, the binary MUST
 * be an extensible chunked container (LEB2) containing a valid "SIGN" chunk whose 64-byte signature
 * verifies all preceding container bytes against the public key.
 *
 * @param buffer Pointer to binary buffer.
 * @param size Total byte size.
 * @param public_key Optional 32-byte Ed25519 public key (or NULL to use configured trusted key).
 * @param out_header Optional pointer to receive validated header.
 * @return Returns LE_OK on success, LE_ERR_SIGNATURE_MISSING, LE_ERR_SIGNATURE_INVALID, or error.
 */
le_status_t le_loader_validate_signed(const uint8_t* buffer, size_t size,
                                      const uint8_t public_key[32],
                                      le_header_t* out_header);

/**
 * @brief Loads a program into the virtual machine with signature verification.
 *
 * @param vm Pointer to the virtual machine instance.
 * @param buffer Pointer to binary buffer.
 * @param size Total byte size.
 * @param public_key Optional 32-byte public key (or NULL).
 * @return Returns LE_OK on success, or error code.
 */
le_status_t le_loader_load_signed(le_vm_t* vm, const uint8_t* buffer, size_t size,
                                  const uint8_t public_key[32]);

#ifdef __cplusplus
}
#endif

#endif /* LE_LOADER_H */
