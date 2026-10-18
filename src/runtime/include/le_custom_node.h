/**
 * @file le_custom_node.h
 * @brief Declarative custom node definitions, dispatch engine, and profile generator.
 */

#ifndef LE_CUSTOM_NODE_H
#define LE_CUSTOM_NODE_H

#include "le_types.h"
#include "le_process_image.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LE_WEAK
#if defined(_MSC_VER)
#define LE_WEAK __declspec(selectany)
#elif defined(__GNUC__) || defined(__clang__)
#define LE_WEAK __attribute__((weak))
#else
#define LE_WEAK
#endif
#endif

/**
 * @brief Data types supported by custom node input and output ports.
 */
typedef enum {
    LE_PORT_TYPE_BOOL   = 0,
    LE_PORT_TYPE_INT    = 1,
    LE_PORT_TYPE_FLOAT  = 2,
    LE_PORT_TYPE_COMPLEX= 3,
} le_custom_port_type_t;

/**
 * @brief Single port descriptor for a custom node (input or output).
 */
typedef struct {
    const char*             name;           /**< Port identifier (e.g. "A", "In", "Duty", "Enable") */
    le_custom_port_type_t   type;           /**< Port data type */
    float                   default_val;    /**< Default input value (used by schematic editors) */
} le_custom_port_t;

/**
 * @brief Complete descriptor for a board-specific custom node.
 *
 * Variable-length N-inputs and M-outputs are represented via pointer + count:
 *   - inputs: pointer to an array of le_custom_port_t (in_count elements)
 *   - outputs: pointer to an array of le_custom_port_t (out_count elements)
 * This allows arbitrary arity with zero memory waste and supports C99 compound literals inline.
 */
typedef struct le_custom_node_s {
    uint8_t                 func_id;        /**< Numeric function identifier (1..255 or 0x80..0xFF) */
    const char*             type_id;        /**< Unique schematic type string, e.g. "BOARD_PWM" */
    const char*             display_name;   /**< Human-readable name, e.g. "Hardware PWM" */
    const char*             category;       /**< Palette category, e.g. "Motion", "DSP", "Math" */
    const char*             description;    /**< Documentation summary tooltip */

    /* N Inputs: pointer to contiguous descriptor array + count */
    const le_custom_port_t* inputs;
    uint8_t                 in_count;

    /* M Outputs: pointer to contiguous descriptor array + count */
    const le_custom_port_t* outputs;
    uint8_t                 out_count;

    /**
     * @brief Node execution handler.
     * @param args Array of address operands: inputs first [0..in_count), then outputs [in_count..in_count+out_count).
     * @param in_count Number of input arguments passed.
     * @param out_count Number of output arguments passed.
     * @param img Pointer to the active process image.
     */
    le_status_t (*handler)(const uint16_t* args, int in_count, int out_count, le_process_image_t* img);
} le_custom_node_t;

/**
 * @brief Board-level custom node table defined in le_config.c.
 */
extern const le_custom_node_t g_board_custom_nodes[];
extern const size_t g_board_custom_node_count;

/**
 * @brief Finds a custom node descriptor by numeric function identifier.
 *
 * Matches either exact `func_id` or normalized 0x80-offset values across
 * entries registered in @ref g_board_custom_nodes.
 *
 * @param func_id Numeric function identifier to search for.
 * @return Pointer to matching @ref le_custom_node_t, or `NULL` if not found.
 */
const le_custom_node_t* le_custom_nodes_find(uint8_t func_id);

/**
 * @brief Dispatches execution of a custom node by function identifier.
 *
 * Validates operand arity against declared inputs/outputs and invokes the node handler.
 *
 * @param func_id Numeric function identifier assigned to the custom node.
 * @param args Array of address operands: inputs first, followed by outputs.
 * @param in_count Number of input operands passed in @p args.
 * @param out_count Number of output operands passed in @p args.
 * @param img Pointer to the active process image.
 * @return Returns @ref LE_OK on success, @ref LE_ERR_NOT_FOUND if unknown, or handler status code.
 */
le_status_t le_custom_nodes_dispatch(uint8_t func_id, const uint16_t* args,
                                     int in_count, int out_count, le_process_image_t* img);

/**
 * @brief Serializes all registered custom nodes to a JSON array string.
 *
 * @return Null-terminated JSON array string stored in an internal static buffer, or "[]" if none.
 */
const char* le_custom_nodes_get_json(void);

/**
 * @brief Streaming callback type for chunked JSON generation without large intermediate buffers.
 *
 * @param chunk Pointer to null-terminated text chunk.
 * @param user_data Caller-provided context pointer.
 */
typedef void (*le_json_write_fn)(const char* chunk, void* user_data);

/**
 * @brief Generates the complete board profile (`.leconfig` format) via a streaming callback.
 *
 * Emits JSON in bounded chunks to avoid allocating large static or heap buffers on microcontrollers.
 *
 * @param write_fn Callback function invoked for each JSON chunk.
 * @param user_data Caller context pointer forwarded to @p write_fn.
 */
void le_config_stream_profile_json(le_json_write_fn write_fn, void* user_data);

/**
 * @brief Serializes the complete board profile (`.leconfig` format) into a memory buffer.
 *
 * @param buf Output buffer to receive JSON text.
 * @param max_len Maximum capacity of @p buf in bytes.
 * @return Total number of bytes written (excluding the null terminator).
 */
size_t le_config_generate_profile_json(char* buf, size_t max_len);

/* ========================================================================== */
/* Convenient Typed Argument Accessor Helpers for Handlers                    */
/* ========================================================================== */

#ifndef LE_ALWAYS_INLINE
#if defined(_MSC_VER)
#define LE_ALWAYS_INLINE static __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define LE_ALWAYS_INLINE static inline __attribute__((always_inline))
#else
#define LE_ALWAYS_INLINE static inline
#endif
#endif

/**
 * @brief Retrieves a floating-point input argument from the process image.
 *
 * @param img Pointer to the active process image.
 * @param args Array of address operands.
 * @param idx Zero-based input argument index.
 * @return Floating-point value stored at `args[idx]`.
 */
LE_ALWAYS_INLINE float le_custom_get_float(const le_process_image_t* img, const uint16_t* args, int idx) {
    return le_process_image_get_float(img, args[idx]);
}

/**
 * @brief Retrieves a boolean input argument from the process image.
 *
 * @param img Pointer to the active process image.
 * @param args Array of address operands.
 * @param idx Zero-based input argument index.
 * @return Boolean value stored at `args[idx]`.
 */
LE_ALWAYS_INLINE bool le_custom_get_bool(const le_process_image_t* img, const uint16_t* args, int idx) {
    return le_process_image_get_bool(img, args[idx]);
}

/**
 * @brief Retrieves a 32-bit integer input argument from the process image.
 *
 * @param img Pointer to the active process image.
 * @param args Array of address operands.
 * @param idx Zero-based input argument index.
 * @return 32-bit integer value stored at `args[idx]`.
 */
LE_ALWAYS_INLINE int32_t le_custom_get_int(const le_process_image_t* img, const uint16_t* args, int idx) {
    return le_process_image_get_int(img, args[idx]);
}

/**
 * @brief Writes a floating-point output value to the process image if connected.
 *
 * Skips writing if the output address is `LE_ADDR_UNUSED`.
 *
 * @param img Pointer to the active process image.
 * @param args Array of address operands.
 * @param in_count Total count of input arguments preceding output arguments.
 * @param out_idx Zero-based output index.
 * @param val Floating-point value to write.
 */
LE_ALWAYS_INLINE void le_custom_set_float(le_process_image_t* img, const uint16_t* args, int in_count, int out_idx, float val) {
    uint16_t addr = args[in_count + out_idx];
    if (addr != LE_ADDR_UNUSED) le_process_image_set_float(img, addr, val);
}

/**
 * @brief Writes a boolean output value to the process image if connected.
 *
 * Skips writing if the output address is `LE_ADDR_UNUSED`.
 *
 * @param img Pointer to the active process image.
 * @param args Array of address operands.
 * @param in_count Total count of input arguments preceding output arguments.
 * @param out_idx Zero-based output index.
 * @param val Boolean value to write.
 */
LE_ALWAYS_INLINE void le_custom_set_bool(le_process_image_t* img, const uint16_t* args, int in_count, int out_idx, bool val) {
    uint16_t addr = args[in_count + out_idx];
    if (addr != LE_ADDR_UNUSED) le_process_image_set_bool(img, addr, val);
}

/**
 * @brief Writes a 32-bit integer output value to the process image if connected.
 *
 * Skips writing if the output address is `LE_ADDR_UNUSED`.
 *
 * @param img Pointer to the active process image.
 * @param args Array of address operands.
 * @param in_count Total count of input arguments preceding output arguments.
 * @param out_idx Zero-based output index.
 * @param val 32-bit integer value to write.
 */
LE_ALWAYS_INLINE void le_custom_set_int(le_process_image_t* img, const uint16_t* args, int in_count, int out_idx, int32_t val) {
    uint16_t addr = args[in_count + out_idx];
    if (addr != LE_ADDR_UNUSED) le_process_image_set_int(img, addr, val);
}

#ifdef __cplusplus
}
#endif

#endif /* LE_CUSTOM_NODE_H */
