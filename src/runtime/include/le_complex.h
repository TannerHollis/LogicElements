/**
 * @file le_complex.h
 * @brief Zero-overhead pure C complex number definitions and inline operations.
 */

#ifndef LE_COMPLEX_H
#define LE_COMPLEX_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

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
static inline le_complex_t le_c_make(float real, float imag)
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
static inline le_complex_t le_c_add(le_complex_t a, le_complex_t b)
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
static inline le_complex_t le_c_sub(le_complex_t a, le_complex_t b)
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
static inline le_complex_t le_c_mul(le_complex_t a, le_complex_t b)
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
static inline le_complex_t le_c_scale(le_complex_t a, float s)
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
static inline le_complex_t le_c_div(le_complex_t a, le_complex_t b)
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
static inline float le_c_mag(le_complex_t a)
{
    return sqrtf(a.r * a.r + a.i * a.i);
}

/**
 * @brief Calculates the phase angle of a complex number in radians.
 *
 * @param a Complex number.
 * @return Phase angle in radians within the interval \f$[-\pi, \pi]\f$.
 */
static inline float le_c_ang(le_complex_t a)
{
    return atan2f(a.i, a.r);
}

/**
 * @brief Constructs a complex number from polar coordinates.
 *
 * @param mag Magnitude of the vector.
 * @param angle_rad Angle in radians.
 * @return Resulting complex number.
 */
static inline le_complex_t le_c_polar(float mag, float angle_rad)
{
    return le_c_make(mag * cosf(angle_rad), mag * sinf(angle_rad));
}

/**
 * @brief Rotates a complex number counter-clockwise by an angular offset.
 *
 * @param a Complex number to rotate.
 * @param delta_rad Angle of rotation in radians.
 * @return Rotated complex number.
 */
static inline le_complex_t le_c_rotate(le_complex_t a, float delta_rad)
{
    float mag = le_c_mag(a);
    float ang = le_c_ang(a) + delta_rad;
    return le_c_polar(mag, ang);
}

#ifdef __cplusplus
}
#endif

#endif /* LE_COMPLEX_H */
