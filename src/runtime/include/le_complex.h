/**
 * @file le_complex.h
 * @brief Zero-overhead pure C complex number definitions and inline operations.
 */
#ifndef LE_COMPLEX_H
#define LE_COMPLEX_H

#include "le_math.h"


#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

/**
 * @brief Single-precision floating-point complex number representation.
 */
typedef struct {
    float r; /**< Real component. */
    float i; /**< Imaginary component. */
} le_complex_t;

#pragma pack(pop)

/**
 * @brief Constructs a complex number from Cartesian coordinates.
 *
 * @param real Real component.
 * @param imag Imaginary component.
 * @return Resulting complex number.
 */
LE_ALWAYS_INLINE le_complex_t le_c_make(float real, float imag)
{
    le_complex_t c = { real, imag };
    return c;
}

/**
 * @brief Computes the sum of two complex numbers.
 *
 * @param a First addend.
 * @param b Second addend.
 * @return Result of `a + b`.
 */
LE_ALWAYS_INLINE le_complex_t le_c_add(le_complex_t a, le_complex_t b)
{
    return le_c_make(a.r + b.r, a.i + b.i);
}

/**
 * @brief Computes the difference of two complex numbers.
 *
 * @param a Minuend.
 * @param b Subtrahend.
 * @return Result of `a - b`.
 */
LE_ALWAYS_INLINE le_complex_t le_c_sub(le_complex_t a, le_complex_t b)
{
    return le_c_make(a.r - b.r, a.i - b.i);
}

/**
 * @brief Computes the product of two complex numbers.
 *
 * @param a First factor.
 * @param b Second factor.
 * @return Result of `a * b`.
 */
LE_ALWAYS_INLINE le_complex_t le_c_mul(le_complex_t a, le_complex_t b)
{
    return le_c_make(a.r * b.r - a.i * b.i, a.r * b.i + a.i * b.r);
}

/**
 * @brief Multiplies a complex number by a real scalar.
 *
 * @param a Complex multiplicand.
 * @param s Real scalar value.
 * @return Scaled complex number.
 */
LE_ALWAYS_INLINE le_complex_t le_c_scale(le_complex_t a, float s)
{
    return le_c_make(a.r * s, a.i * s);
}

/**
 * @brief Computes the division of two complex numbers.
 *
 * If the denominator magnitude squared is less than `1e-12f`, returns `0 + 0j`
 * to protect against division by zero without raising exceptions.
 *
 * @param a Dividend.
 * @param b Divisor.
 * @return Result of `a / b`.
 */
LE_ALWAYS_INLINE le_complex_t le_c_div(le_complex_t a, le_complex_t b)
{
    float denom = b.r * b.r + b.i * b.i;
    if (denom < 1e-12f) {
        return le_c_make(0.0f, 0.0f);
    }
    return le_c_make((a.r * b.r + a.i * b.i) / denom,
                     (a.i * b.r - a.r * b.i) / denom);
}

/**
 * @brief Calculates the magnitude (absolute value) of a complex number.
 *
 * @param a Complex number.
 * @return Magnitude \f$|a| = \sqrt{r^2 + i^2}\f$.
 */
LE_ALWAYS_INLINE float le_c_mag(le_complex_t a)
{
    return le_fast_cmplx_mag(a.r, a.i);
}

/**
 * @brief Calculates the phase angle of a complex number in radians.
 *
 * @param a Complex number.
 * @return Phase angle in radians within the interval \f$[-\pi, \pi]\f$.
 */
LE_ALWAYS_INLINE float le_c_ang(le_complex_t a)
{
    return le_fast_atan2(a.i, a.r);
}

/**
 * @brief Constructs a complex number from polar coordinates.
 *
 * @param mag Magnitude of the vector.
 * @param angle_rad Angle in radians.
 * @return Resulting complex number.
 */
LE_ALWAYS_INLINE le_complex_t le_c_polar(float mag, float angle_rad)
{
    float s, c;
    le_sincos(angle_rad, &s, &c);
    return le_c_make(mag * c, mag * s);
}

/**
 * @brief Rotates a complex number counter-clockwise by an angular offset.
 *
 * @param a Complex number to rotate.
 * @param delta_rad Angle of rotation in radians.
 * @return Rotated complex number.
 */
LE_ALWAYS_INLINE le_complex_t le_c_rotate(le_complex_t a, float delta_rad)
{
    float s, c;
    le_sincos(delta_rad, &s, &c);
    return le_c_make(a.r * c - a.i * s, a.r * s + a.i * c);
}

#ifdef __cplusplus
}
#endif

#endif /* LE_COMPLEX_H */
