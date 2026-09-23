/**
 * @file le_compiler_types.h
 * @brief Internal data types and metadata structures for compiler configuration and custom nodes.
 */

#ifndef LE_COMPILER_TYPES_H
#define LE_COMPILER_TYPES_H

#include <stdint.h>
#include <string>
#include <vector>

namespace LogicElements {

/**
 * @brief Metadata descriptor for an analog input linear scaling block.
 */
struct ScalerInfo {
    int index = 0;              /**< Scaler slot index. */
    std::string name;           /**< User-assigned symbolic identifier for the scaler block. */
    int channel = 0;            /**< Physical analog input channel index. */
    float raw_min = 0.0f;       /**< Minimum expected raw ADC integer value. */
    float raw_max = 4095.0f;    /**< Maximum expected raw ADC integer value. */
    float scale_min = 0.0f;     /**< Scaled engineering unit minimum value. */
    float scale_max = 100.0f;   /**< Scaled engineering unit maximum value. */
    std::string units;          /**< Engineering unit label (e.g. "degC", "PSI", "V"). */
    bool clamp = true;          /**< Clamps the output between scale_min and scale_max when true. */
};

/**
 * @brief Port metadata descriptor for custom board node inputs and outputs.
 */
struct CustomPinInfo {
    std::string name;           /**< Port name identifier. */
    std::string type;           /**< Data type descriptor ("bool", "float", "int"). */
    float default_val = 0.0f;   /**< Default value when the port is unconnected. */
};

/**
 * @brief Board-specific custom logic node definition.
 */
struct CustomNodeInfo {
    std::string type_id;        /**< Unique schematic element type string (e.g. "DSP_FILTER_1"). */
    std::string display_name;   /**< Human-readable label displayed in UI schematic editors. */
    std::string category;       /**< Palette category grouping (e.g. "DSP", "Motor Control"). */
    std::string description;    /**< Description of the custom node functionality. */
    uint8_t function_id = 1;    /**< Numeric function identifier passed to @ref le_hal_t::ext_call. */
    std::string c_header;       /**< Optional C header include required by the board BSP. */
    std::vector<CustomPinInfo> inputs;  /**< List of input terminal definitions. */
    std::vector<CustomPinInfo> outputs; /**< List of output terminal definitions. */
};

} // namespace LogicElements

#endif /* LE_COMPILER_TYPES_H */
