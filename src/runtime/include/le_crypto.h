/**
 * @file le_crypto.h
 * @brief Self-contained Ed25519 public-key signature verification and signing for LogicElements.
 *
 * Implements deterministic Ed25519 (RFC 8032) authentication for .lebin chunked
 * containers with zero heap allocations, zero external dependencies, and a constant
 * memory footprint suitable for embedded microcontrollers (ARM Cortex-M, RISC-V, etc.).
 */

#ifndef LE_CRYPTO_H
#define LE_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "le_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LE_CRYPTO_PUBLIC_KEY_BYTES  32
#define LE_CRYPTO_SECRET_KEY_BYTES  64
#define LE_CRYPTO_SIGNATURE_BYTES   64
#define LE_CRYPTO_SEED_BYTES        32

/**
 * @brief Derives a 32-byte Ed25519 public key from a 32-byte seed.
 *
 * @param out_pk 32-byte destination buffer for the public key.
 * @param out_sk 64-byte destination buffer for the expanded secret key (seed + pk).
 * @param seed 32-byte random seed / private key seed.
 * @return Returns LE_OK on success, or LE_ERR_NULL_PTR if pointers are NULL.
 */
le_status_t le_crypto_ed25519_keypair_from_seed(uint8_t out_pk[LE_CRYPTO_PUBLIC_KEY_BYTES],
                                                uint8_t out_sk[LE_CRYPTO_SECRET_KEY_BYTES],
                                                const uint8_t seed[LE_CRYPTO_SEED_BYTES]);

/**
 * @brief Computes a 64-byte Ed25519 detached signature over a message buffer.
 *
 * @param out_sig 64-byte destination buffer for the signature.
 * @param msg Pointer to the message payload to sign.
 * @param msg_len Length of the message in bytes.
 * @param sk 64-byte expanded secret key.
 * @return Returns LE_OK on success, or LE_ERR_NULL_PTR if pointers are NULL.
 */
le_status_t le_crypto_ed25519_sign(uint8_t out_sig[LE_CRYPTO_SIGNATURE_BYTES],
                                   const uint8_t* msg,
                                   size_t msg_len,
                                   const uint8_t sk[LE_CRYPTO_SECRET_KEY_BYTES]);

/**
 * @brief Verifies a 64-byte Ed25519 detached signature against a message and public key.
 *
 * Constant-time field operations and point decompression guarantee resistance
 * to timing side channels and fault injection on embedded targets.
 *
 * @param sig 64-byte signature to verify.
 * @param msg Pointer to the signed message payload.
 * @param msg_len Length of the message in bytes.
 * @param pk 32-byte Ed25519 public key.
 * @return Returns LE_OK on signature match, LE_ERR_SIGNATURE_INVALID on mismatch or corrupted point,
 *         or LE_ERR_NULL_PTR on NULL parameters.
 */
le_status_t le_crypto_ed25519_verify(const uint8_t sig[LE_CRYPTO_SIGNATURE_BYTES],
                                     const uint8_t* msg,
                                     size_t msg_len,
                                     const uint8_t pk[LE_CRYPTO_PUBLIC_KEY_BYTES]);

/**
 * @brief Computes standard SHA-512 over a buffer (used as part of Ed25519).
 *
 * @param out_hash 64-byte destination buffer for the SHA-512 digest.
 * @param in Pointer to the input data.
 * @param in_len Length of the input data in bytes.
 */
void le_crypto_sha512(uint8_t out_hash[64], const uint8_t* in, size_t in_len);

#ifdef __cplusplus
}
#endif

#endif /* LE_CRYPTO_H */
