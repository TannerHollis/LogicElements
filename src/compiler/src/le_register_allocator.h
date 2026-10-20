/**
 * @file le_register_allocator.h
 * @brief Dedicated virtual machine process image register allocator for LogicElements.
 *
 * Manages allocation of both persistent user registers and recycled intermediate temporary
 * registers within the fixed process image address regions (%B, %N, %F, %C).
 *
 * ## Allocation Invariants
 * - **User Registers**: Monotonically assigned 0-based indices during initial schematic discovery.
 *   These persist across scan cycles and correspond to named registers or physical I/O.
 * - **Temporary Registers**: Transient intermediate registers reused via LIFO/min-element pooling.
 *   Mapped immediately above user registers (`user_count + temp_idx`).
 * - **Peak Tracking**: Records maximum concurrent temporary register occupancy (`peak_temp_*`)
 *   to determine total memory requirement and partition timing.
 */

#ifndef LE_REGISTER_ALLOCATOR_H
#define LE_REGISTER_ALLOCATOR_H

#include "le_addr.h"
#include "le_types.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace LogicElements {

/**
 * @brief Register allocation domain tags.
 */
enum class RegType {
    Bool,    /**< 1-bit boolean register (%B). */
    Int,     /**< 32-bit signed integer register (%N). */
    Float,   /**< 32-bit single-precision floating-point register (%F). */
    Complex  /**< Complex float register (%C, real and imag pair). */
};

/**
 * @brief Encapsulates state and pool algorithms for virtual machine registers.
 */
class RegisterAllocator {
public:
    RegisterAllocator() = default;

    /**
     * @brief Resets all allocation counters and free pools to their initial state.
     */
    void reset() noexcept {
        m_user_bool_count = 0;
        m_user_int_count = 0;
        m_user_float_count = 0;
        m_user_complex_count = 0;

        m_peak_temp_bool = 0;
        m_peak_temp_float = 0;
        m_peak_temp_complex = 0;
        m_peak_temp_int = 0;

        m_free_temp_bool_pool.clear();
        m_free_temp_float_pool.clear();
        m_free_temp_complex_pool.clear();
        m_free_temp_int_pool.clear();
    }

    /* ====================================================================== */
    /* User Register Allocation                                               */
    /* ====================================================================== */

    /** @brief Allocates a named user boolean register. @return 16-bit process image address. */
    uint16_t allocate_user_bool() noexcept {
        uint16_t addr = static_cast<uint16_t>(LE_REGION_BOOL_REG | (m_user_bool_count & 0x1FFF));
        m_user_bool_count++;
        return addr;
    }

    /** @brief Allocates a named user 32-bit integer register. @return 16-bit process image address. */
    uint16_t allocate_user_int() noexcept {
        uint16_t addr = static_cast<uint16_t>(LE_REGION_INT_REG | (m_user_int_count & 0x0FFF));
        m_user_int_count++;
        return addr;
    }

    /** @brief Allocates a named user 32-bit floating-point register. @return 16-bit process image address. */
    uint16_t allocate_user_float() noexcept {
        uint16_t addr = static_cast<uint16_t>(LE_REGION_FLOAT | (m_user_float_count & 0x3FFF));
        m_user_float_count++;
        return addr;
    }

    /** @brief Allocates a named user complex register. @return 16-bit process image address. */
    uint16_t allocate_user_complex() noexcept {
        uint16_t addr = static_cast<uint16_t>(LE_REGION_CMPLX | (m_user_complex_count & LE_ADDR_INDEX_MASK));
        m_user_complex_count++;
        return addr;
    }

    /* ====================================================================== */
    /* Temporary Register Acquisition and Release                            */
    /* ====================================================================== */

    /**
     * @brief Acquires a temporary boolean register, reusing released registers where available.
     * @param[out] out_temp_idx Receives the zero-based temporary pool index.
     * @return 16-bit process image address.
     */
    uint16_t acquire_temp_bool(int& out_temp_idx) {
        if (!m_free_temp_bool_pool.empty()) {
            auto it = std::min_element(m_free_temp_bool_pool.begin(), m_free_temp_bool_pool.end());
            out_temp_idx = *it;
            m_free_temp_bool_pool.erase(it);
        } else {
            out_temp_idx = m_peak_temp_bool++;
        }
        return static_cast<uint16_t>(LE_REGION_BOOL_REG | ((m_user_bool_count + out_temp_idx) & 0x1FFF));
    }

    /** @brief Releases a temporary boolean register back to the free pool. */
    void release_temp_bool(int temp_idx) {
        if (temp_idx >= 0 && std::find(m_free_temp_bool_pool.begin(), m_free_temp_bool_pool.end(), temp_idx) == m_free_temp_bool_pool.end()) {
            m_free_temp_bool_pool.push_back(temp_idx);
        }
    }

    /**
     * @brief Acquires a temporary float register, reusing released registers where available.
     * @param[out] out_temp_idx Receives the zero-based temporary pool index.
     * @return 16-bit process image address.
     */
    uint16_t acquire_temp_float(int& out_temp_idx) {
        if (!m_free_temp_float_pool.empty()) {
            auto it = std::min_element(m_free_temp_float_pool.begin(), m_free_temp_float_pool.end());
            out_temp_idx = *it;
            m_free_temp_float_pool.erase(it);
        } else {
            out_temp_idx = m_peak_temp_float++;
        }
        return static_cast<uint16_t>(LE_REGION_FLOAT | ((m_user_float_count + out_temp_idx) & 0x3FFF));
    }

    /** @brief Releases a temporary float register back to the free pool. */
    void release_temp_float(int temp_idx) {
        if (temp_idx >= 0 && std::find(m_free_temp_float_pool.begin(), m_free_temp_float_pool.end(), temp_idx) == m_free_temp_float_pool.end()) {
            m_free_temp_float_pool.push_back(temp_idx);
        }
    }

    /**
     * @brief Acquires a temporary complex register, reusing released registers where available.
     * @param[out] out_temp_idx Receives the zero-based temporary pool index.
     * @return 16-bit process image address.
     */
    uint16_t acquire_temp_complex(int& out_temp_idx) {
        if (!m_free_temp_complex_pool.empty()) {
            auto it = std::min_element(m_free_temp_complex_pool.begin(), m_free_temp_complex_pool.end());
            out_temp_idx = *it;
            m_free_temp_complex_pool.erase(it);
        } else {
            out_temp_idx = m_peak_temp_complex++;
        }
        return static_cast<uint16_t>(LE_REGION_CMPLX | ((m_user_complex_count + out_temp_idx) & LE_ADDR_INDEX_MASK));
    }

    /** @brief Releases a temporary complex register back to the free pool. */
    void release_temp_complex(int temp_idx) {
        if (temp_idx >= 0 && std::find(m_free_temp_complex_pool.begin(), m_free_temp_complex_pool.end(), temp_idx) == m_free_temp_complex_pool.end()) {
            m_free_temp_complex_pool.push_back(temp_idx);
        }
    }

    /**
     * @brief Acquires a temporary integer register, reusing released registers where available.
     * @param[out] out_temp_idx Receives the zero-based temporary pool index.
     * @return 16-bit process image address.
     */
    uint16_t acquire_temp_int(int& out_temp_idx) {
        if (!m_free_temp_int_pool.empty()) {
            auto it = std::min_element(m_free_temp_int_pool.begin(), m_free_temp_int_pool.end());
            out_temp_idx = *it;
            m_free_temp_int_pool.erase(it);
        } else {
            out_temp_idx = m_peak_temp_int++;
        }
        return static_cast<uint16_t>(LE_REGION_INT_REG | ((m_user_int_count + out_temp_idx) & 0x0FFF));
    }

    /** @brief Releases a temporary integer register back to the free pool. */
    void release_temp_int(int temp_idx) {
        if (temp_idx >= 0 && std::find(m_free_temp_int_pool.begin(), m_free_temp_int_pool.end(), temp_idx) == m_free_temp_int_pool.end()) {
            m_free_temp_int_pool.push_back(temp_idx);
        }
    }

    /* ====================================================================== */
    /* Accessors                                                              */
    /* ====================================================================== */

    [[nodiscard]] int user_bool_count() const noexcept { return m_user_bool_count; }
    [[nodiscard]] int user_int_count() const noexcept { return m_user_int_count; }
    [[nodiscard]] int user_float_count() const noexcept { return m_user_float_count; }
    [[nodiscard]] int user_complex_count() const noexcept { return m_user_complex_count; }

    [[nodiscard]] int peak_temp_bool() const noexcept { return m_peak_temp_bool; }
    [[nodiscard]] int peak_temp_float() const noexcept { return m_peak_temp_float; }
    [[nodiscard]] int peak_temp_complex() const noexcept { return m_peak_temp_complex; }
    [[nodiscard]] int peak_temp_int() const noexcept { return m_peak_temp_int; }

    [[nodiscard]] int total_bool_registers() const noexcept { return m_user_bool_count + m_peak_temp_bool; }
    [[nodiscard]] int total_int_registers() const noexcept { return m_user_int_count + m_peak_temp_int; }
    [[nodiscard]] int total_float_registers() const noexcept { return m_user_float_count + m_peak_temp_float; }
    [[nodiscard]] int total_complex_registers() const noexcept { return m_user_complex_count + m_peak_temp_complex; }

private:
    int m_user_bool_count = 0;
    int m_user_int_count = 0;
    int m_user_float_count = 0;
    int m_user_complex_count = 0;

    int m_peak_temp_bool = 0;
    int m_peak_temp_float = 0;
    int m_peak_temp_complex = 0;
    int m_peak_temp_int = 0;

    std::vector<int> m_free_temp_bool_pool;
    std::vector<int> m_free_temp_float_pool;
    std::vector<int> m_free_temp_complex_pool;
    std::vector<int> m_free_temp_int_pool;
};

} // namespace LogicElements

#endif /* LE_REGISTER_ALLOCATOR_H */
