/**
 * @file custom_hal.c
 * @brief Adopter hardware abstraction layer implementation for custom nodes.
 *
 * Demonstrates how a microcontroller port integrates custom nodes using the
 * le_hal_t::ext_call and le_hal_t::get_custom_nodes_json callbacks.
 */

#include <stdio.h>
#include <string.h>
#include "le_hal.h"
#include "le_process_image.h"
#include "custom_nodes.h"

/* ========================================================================== */
/* Hardware Driver Implementations (or Simulator Stubs)                       */
/* ========================================================================== */

static float s_simulated_pwm_duty = 0.0f;
static bool  s_simulated_pwm_active = false;
static int32_t s_simulated_encoder_ticks = 0;
static float s_simulated_filter_state = 0.0f;

bool hardware_pwm_set(float duty_percent, bool enable)
{
    if (enable) {
        if (duty_percent < 0.0f) duty_percent = 0.0f;
        if (duty_percent > 100.0f) duty_percent = 100.0f;
        s_simulated_pwm_duty = duty_percent;
        s_simulated_pwm_active = true;
        /* On hardware: __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)duty); */
    } else {
        s_simulated_pwm_duty = 0.0f;
        s_simulated_pwm_active = false;
        /* On hardware: __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0); */
    }
    return s_simulated_pwm_active;
}

int32_t hardware_encoder_read(bool reset)
{
    if (reset) {
        s_simulated_encoder_ticks = 0;
        /* On hardware: __HAL_TIM_SET_COUNTER(&htim3, 0); */
    } else {
        s_simulated_encoder_ticks += 4; /* Simulate rotation increments */
    }
    return s_simulated_encoder_ticks;
}

float hardware_filter_apply(float raw_input)
{
    const float alpha = 0.10f;
    s_simulated_filter_state += alpha * (raw_input - s_simulated_filter_state);
    return s_simulated_filter_state;
}

/* ========================================================================== */
/* Self-Describing Metadata JSON Definition                                   */
/* ========================================================================== */

static const char s_board_custom_nodes_json[] =
"["
"  {"
"    \"type\": \"BOARD_PWM\","
"    \"func_id\": 0x81,"
"    \"category\": \"Motion\","
"    \"description\": \"Hardware PWM duty cycle generator (0..100%)\","
"    \"inputs\": [\"duty\", \"enable\"],"
"    \"outputs\": [\"active\"]"
"  },"
"  {"
"    \"type\": \"BOARD_ENCODER\","
"    \"func_id\": 0x82,"
"    \"category\": \"Motion\","
"    \"description\": \"Quadrature encoder pulse counter\","
"    \"inputs\": [\"reset\"],"
"    \"outputs\": [\"position\"]"
"  },"
"  {"
"    \"type\": \"BOARD_FILTER\","
"    \"func_id\": 0x83,"
"    \"category\": \"DSP\","
"    \"description\": \"Hardware low-pass filter\","
"    \"inputs\": [\"raw\"],"
"    \"outputs\": [\"filtered\"]"
"  }"
"]";

/**
 * @brief HAL callback returning custom node metadata JSON.
 */
static const char* custom_board_get_nodes_json(void)
{
    return s_board_custom_nodes_json;
}

/**
 * @brief HAL callback servicing LE_OP_BLOCK custom-node instructions (func_id >= LE_FUNC_CUSTOM_BASE).
 *
 * The operand list is variable arity: args[0..in_count) are live input addresses,
 * followed by out_count output addresses. Operand data types are implied by each
 * address's process-image region.
 *
 * @param func_id Numeric function identifier assigned in board profile.
 * @param args Address list: inputs first, then outputs.
 * @param in_count Number of input operand addresses in @p args.
 * @param out_count Number of output operand addresses in @p args (following inputs).
 * @param img Pointer to the runtime process image memory.
 * @return LE_OK on successful execution; LE_ERROR on invalid operand or ID.
 */
static le_status_t custom_board_ext_call(uint8_t func_id,
                                         const uint16_t* args,
                                         int in_count,
                                         int out_count,
                                         le_process_image_t* img)
{
    if (!img || !args || in_count < 0 || out_count < 1) {
        return LE_ERR_NULL_PTR;
    }

    switch (func_id) {
    case 0x81: { /* BOARD_PWM */
        if (in_count < 2) return LE_ERR_OUT_OF_BOUNDS;
        /* args[0]: float duty cycle address (%R), args[1]: boolean enable (%M or %I) */
        float duty = le_process_image_get_float(img, args[0]);
        bool enable = le_process_image_get_bool(img, args[1]);

        bool active = hardware_pwm_set(duty, enable);

        /* Write active status to the first output destination. */
        uint16_t out_addr = args[in_count];
        if (out_addr != LE_ADDR_UNUSED) {
            le_process_image_set_bool(img, out_addr, active);
        }
        return LE_OK;
    }

    case 0x82: { /* BOARD_ENCODER */
        if (in_count < 1) return LE_ERR_OUT_OF_BOUNDS;
        /* args[0]: boolean reset index strobe */
        bool reset = (args[0] != LE_ADDR_UNUSED) ? le_process_image_get_bool(img, args[0]) : false;

        int32_t count = hardware_encoder_read(reset);

        /* Write position to the first float-register output destination. */
        uint16_t out_addr = args[in_count];
        if (out_addr != LE_ADDR_UNUSED) {
            le_process_image_set_float(img, out_addr, (float)count);
        }
        return LE_OK;
    }

    case 0x83: { /* BOARD_FILTER */
        if (in_count < 1) return LE_ERR_OUT_OF_BOUNDS;
        /* args[0]: raw float sensor reading */
        float raw = le_process_image_get_float(img, args[0]);
        float filtered = hardware_filter_apply(raw);

        /* Write filtered float result to the first output destination. */
        uint16_t out_addr = args[in_count];
        if (out_addr != LE_ADDR_UNUSED) {
            le_process_image_set_float(img, out_addr, filtered);
        }
        return LE_OK;
    }

    default:
        /* Unrecognized function ID */
        return LE_ERR_UNKNOWN_OPCODE;
    }
}

/**
 * @brief Configures custom node handlers into the target HAL table.
 * @param hal Pointer to mutable HAL structure to configure.
 */
void custom_board_register_hal_callbacks(le_hal_t* hal)
{
    if (hal) {
        hal->ext_call = custom_board_ext_call;
        hal->get_custom_nodes_json = custom_board_get_nodes_json;
    }
}

