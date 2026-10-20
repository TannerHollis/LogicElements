/**
 * @file le_endian.h
 * @brief Little-endian explicit serialization and deserialization utilities.
 *
 * Provides architecture-independent, alignment-safe routines for reading and
 * writing multi-byte integer and IEEE-754 floating-point numbers in canonical
 * little-endian byte order as required by the LogicElements container and VM.
 */

#ifndef LE_ENDIAN_H
#define LE_ENDIAN_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads a 16-bit unsigned integer from a buffer in little-endian byte order.
 *
 * @param[in] ptr Pointer to at least 2 readable bytes.
 * @return Decoded 16-bit unsigned integer.
 */
static inline uint16_t le_read_u16_le(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/**
 * @brief Reads a 32-bit unsigned integer from a buffer in little-endian byte order.
 *
 * @param[in] ptr Pointer to at least 4 readable bytes.
 * @return Decoded 32-bit unsigned integer.
 */
static inline uint32_t le_read_u32_le(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/**
 * @brief Writes a 16-bit unsigned integer to a buffer in little-endian byte order.
 *
 * @param[out] ptr Pointer to at least 2 writable bytes.
 * @param[in]  val 16-bit unsigned integer to serialize.
 */
static inline void le_write_u16_le(void* ptr, uint16_t val) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)(val & 0xFF);
    p[1] = (uint8_t)((val >> 8) & 0xFF);
}

/**
 * @brief Writes a 32-bit unsigned integer to a buffer in little-endian byte order.
 *
 * @param[out] ptr Pointer to at least 4 writable bytes.
 * @param[in]  val 32-bit unsigned integer to serialize.
 */
static inline void le_write_u32_le(void* ptr, uint32_t val) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)(val & 0xFF);
    p[1] = (uint8_t)((val >> 8) & 0xFF);
    p[2] = (uint8_t)((val >> 16) & 0xFF);
    p[3] = (uint8_t)((val >> 24) & 0xFF);
}

/**
 * @brief Reads an IEEE-754 single-precision float from a buffer in little-endian byte order.
 *
 * @param[in] ptr Pointer to at least 4 readable bytes.
 * @return Decoded 32-bit single-precision float.
 */
static inline float le_read_f32_le(const void* ptr) {
    uint32_t raw = le_read_u32_le(ptr);
    float f;
    memcpy(&f, &raw, sizeof(float));
    return f;
}

/**
 * @brief Writes an IEEE-754 single-precision float to a buffer in little-endian byte order.
 *
 * @param[out] ptr Pointer to at least 4 writable bytes.
 * @param[in]  val 32-bit single-precision float to serialize.
 */
static inline void le_write_f32_le(void* ptr, float val) {
    uint32_t raw;
    memcpy(&raw, &val, sizeof(float));
    le_write_u32_le(ptr, raw);
}

#ifdef __cplusplus
}
#endif

#endif /* LE_ENDIAN_H */
