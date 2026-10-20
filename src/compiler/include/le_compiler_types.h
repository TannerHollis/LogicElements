/**
 * @file le_compiler_types.h
 * @brief Internal data types and metadata descriptors for compiler configuration and custom board nodes.
 *
 * This header defines data structures representing analog signal scaling configuration (@ref ScalerInfo),
 * custom board I/O pin descriptors (@ref CustomPinInfo), and board-specific custom node definitions
 * (@ref CustomNodeInfo) declared in target board profiles (.leconfig).
 */

#ifndef LE_COMPILER_TYPES_H
#define LE_COMPILER_TYPES_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace LogicElements {

/**
 * @brief Metadata descriptor for an analog input linear calibration and scaling block.
 *
 * Configures conversion of raw integer ADC sensor readings into engineering units
 * according to the linear transfer function:
 * \f[
 *   y = \text{scale\_min} + \frac{x - \text{raw\_min}}{\text{raw\_max} - \text{raw\_min}} \cdot (\text{scale\_max} - \text{scale\_min})
 * \f]
 */
struct ScalerInfo {
    int index = 0;              /**< Scaler slot index within the runtime state table (0–15). */
    std::string name;           /**< User-assigned symbolic identifier for the scaler element. */
    int channel = 0;            /**< Physical hardware analog input channel index (0-based). */
    float raw_min = 0.0f;       /**< Minimum expected raw ADC integer reading (e.g. 0.0f). */
    float raw_max = 4095.0f;    /**< Maximum expected raw ADC integer reading (e.g. 4095.0f for 12-bit ADC). */
    float scale_min = 0.0f;     /**< Scaled engineering unit minimum value. */
    float scale_max = 100.0f;   /**< Scaled engineering unit maximum value. */
    std::string units = "%";    /**< Engineering unit label (e.g. "degC", "PSI", "V", "%"). */
    bool clamp = true;          /**< When true, clamps the output strictly between scale_min and scale_max. */
};

/**
 * @brief Terminal metadata descriptor for custom board node inputs and outputs.
 */
struct CustomPinInfo {
    std::string name;           /**< Port name identifier (e.g. "in_a", "speed", "fault"). */
    std::string type = "float"; /**< Data type descriptor string: "bool", "float", or "int". */
    float default_val = 0.0f;   /**< Default numerical value assumed when the port is unconnected. */
};

/**
 * @brief Board-specific custom logic node definition.
 *
 * Defines an external hardware block or DSP kernel implemented by the board Board Support Package (BSP)
 * and invoked at runtime via `LE_OP_EXT_CALL` (`le_hal_t::ext_call`).
 */
struct CustomNodeInfo {
    std::string type_id;        /**< Unique schematic element type string (e.g. "DSP_FILTER_1"). */
    std::string display_name;   /**< Human-readable label displayed in UI schematic editors. */
    std::string category;       /**< Palette category grouping (e.g. "DSP", "Motor Control"). */
    std::string description;    /**< Description of the custom node functionality. */
    uint8_t function_id = 1;    /**< Numeric function identifier passed to @ref le_hal_t::ext_call (1–255). */
    std::string c_header;       /**< Optional C header include required by the board BSP. */
    std::vector<CustomPinInfo> inputs;  /**< List of input terminal definitions. */
    std::vector<CustomPinInfo> outputs; /**< List of output terminal definitions. */
};

/**
 * @brief Parsed configuration and hardware constraints extracted from a target board profile (.leconfig).
 *
 * Encapsulates pin aliases, BSP-implemented custom nodes, C header includes, and derived I/O limits
 * for hardware platform targeting.
 */
struct BoardProfile {
    uint16_t effective_target_version = 13;             /**< Effective container bytecode target version. */
    bool has_board = false;                             /**< True if a board profile document was provided and parsed. */
    bool has_pinmap = false;                            /**< True if the profile explicitly specifies a pin_map block. */
    std::map<std::string, std::string> alias_to_addr;   /**< Map of symbolic board pin aliases to process-image addresses. */
    std::map<std::string, CustomNodeInfo> custom_nodes; /**< Map of custom hardware node type IDs to their definitions. */
    std::vector<std::string> custom_headers;            /**< List of external C header paths required by custom nodes. */
    int derived_din = 0;                                /**< Inferred maximum digital input pin count from pinmap. */
    int derived_dout = 0;                               /**< Inferred maximum digital output pin count from pinmap. */
    int derived_ain = 0;                                /**< Inferred maximum analog input pin count from pinmap. */
    int derived_aout = 0;                               /**< Inferred maximum analog output pin count from pinmap. */
};

} // namespace LogicElements

#endif /* LE_COMPILER_TYPES_H */
