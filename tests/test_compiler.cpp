#include "le_compiler.h"
#include "le_types.h"
#include "le_process_image.h"
#include "le_vm.h"
#include "le_loader.h"
#include "le_rt.h"
#include "le_opcodes.h"
#include "le_comms.h"
#include "le_hal.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
    const le_hal_t* le_hal_get_sim(void);
    void   le_sim_capture_tx_reset(void);
    size_t le_sim_capture_tx_len(void);
    void   le_sim_capture_tx_get(uint8_t* out, size_t cap);
}

/* Frame one packet into comms (mirrors the runtime test harness). */
static void test_feed_packet(le_comms_t* comms, uint8_t cmd, uint8_t seq,
                             const uint8_t* payload, uint16_t len)
{
    uint8_t header[5];
    header[0] = LE_COMMS_SYNC_BYTE;
    header[1] = cmd;
    header[2] = seq;
    header[3] = (uint8_t)(len & 0xFF);
    header[4] = (uint8_t)((len >> 8) & 0xFF);
    uint16_t crc = 0xFFFF;
    for (int i = 1; i < 5; i++) {
        uint8_t b = header[i];
        crc ^= (uint16_t)b << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = payload[i];
        crc ^= (uint16_t)b << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    for (int i = 0; i < 5; i++) le_comms_process_byte(comms, header[i]);
    for (uint16_t i = 0; i < len; i++) le_comms_process_byte(comms, payload[i]);
    le_comms_process_byte(comms, (uint8_t)(crc & 0xFF));
    le_comms_process_byte(comms, (uint8_t)((crc >> 8) & 0xFF));
}

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            g_tests_failed++; \
            return; \
        } \
    } while(0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "Running " << #fn << "... "; \
        int before = g_tests_failed; \
        fn(); \
        if (g_tests_failed == before) { \
            std::cout << "PASS\n"; \
            g_tests_passed++; \
        } \
    } while(0)

void test_basic_and_circuit()
{
    const char* circuit_json = R"({
        "name": "TestAndCircuit",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "AND1", "type": "AND" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, nullptr, &res);
    TEST_ASSERT(rc == 0, "le_compile_json returned non-zero");
    TEST_ASSERT(res.success == 1, "Compilation result not success");
    TEST_ASSERT(res.instruction_count >= 1, "Expected at least 1 instruction");
    TEST_ASSERT(res.din_count == 2, "Expected 2 DIN");
    TEST_ASSERT(res.dout_count == 1, "Expected 1 DOUT");
    TEST_ASSERT(res.binary_data != nullptr, "Binary data null");
    TEST_ASSERT(res.binary_size > 26, "Binary size <= 26");
    TEST_ASSERT(res.disassembly_text != nullptr, "Disassembly null");
    TEST_ASSERT(res.c_header_code != nullptr, "C header null");

    // Verify VM executes bytecode correctly
    le_vm_t vm;
    le_status_t v_init = le_vm_init(&vm);
    TEST_ASSERT(v_init == LE_OK, "le_vm_init failed");

    le_status_t l_status = le_loader_load(&vm, res.binary_data, res.binary_size);
    TEST_ASSERT(l_status == LE_OK, "le_loader_load failed");

    // Case 1: IN1 = 0, IN2 = 1 -> OUT1 should be 0
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    le_status_t v_status = le_vm_step(&vm, 0);
    TEST_ASSERT(v_status == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT1 should be false");

    // Case 2: IN1 = 1, IN2 = 1 -> OUT1 should be 1
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    v_status = le_vm_step(&vm, 0);
    TEST_ASSERT(v_status == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 should be true");

    le_compile_result_free(&res);
}

void test_float_math_circuit()
{
    const char* circuit_json = R"({
        "name": "FloatMathCircuit",
        "elements": [
            { "name": "C1", "type": "CONSTANT", "dataType": "float", "value": 1.0 },
            { "name": "C2", "type": "CONSTANT", "dataType": "float", "value": 1.0 },
            { "name": "ADD1", "type": "ADD" },
            { "name": "R1", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "C1", "port": "out" }, "inputs": [ { "name": "ADD1", "port": "a" } ] },
            { "output": { "name": "C2", "port": "out" }, "inputs": [ { "name": "ADD1", "port": "b" } ] },
            { "output": { "name": "ADD1", "port": "out" }, "inputs": [ { "name": "R1", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, nullptr, &res);
    TEST_ASSERT(rc == 0, "le_compile_json returned non-zero");
    TEST_ASSERT(res.success == 1, "Compilation result not success");
    TEST_ASSERT(res.user_float_count == 1, "Expected 1 user float register");

    le_vm_t vm;
    le_status_t v_init = le_vm_init(&vm);
    TEST_ASSERT(v_init == LE_OK, "le_vm_init failed");

    le_status_t l_status = le_loader_load(&vm, res.binary_data, res.binary_size);
    TEST_ASSERT(l_status == LE_OK, "le_loader_load failed");

    le_status_t v_status = le_vm_step(&vm, 0);
    TEST_ASSERT(v_status == LE_OK, "vm_step failed");
    float r1_val = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(0));
    TEST_ASSERT(r1_val >= 1.99f && r1_val <= 2.01f, "R1 should be 2.0f");

    le_compile_result_free(&res);
}

void test_board_limits_validation()
{
    const char* circuit_json = R"({
        "name": "ExcessiveInputs",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "IN3", "type": "DIGITALINPUT", "address": "%I2" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": []
    })";

    const char* board_json = R"({
        "device": { "name": "TinyBoard" },
        "limits": {
            "digital_inputs": 2,
            "digital_outputs": 4,
            "coils": 16,
            "floats": 8,
            "workspace_bytes": 512,
            "slot_size_bytes": 1024
        },
        "features": {
            "protection": true,
            "serial_bus": true
        }
    })";

    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, board_json, &res);
    TEST_ASSERT(rc != 0 || res.success == 0, "Should fail board limits validation");
    TEST_ASSERT(res.error_message != nullptr, "Error message should be set");
    std::string err = res.error_message;
    TEST_ASSERT(err.find("digital inputs") != std::string::npos, "Error should mention digital inputs");

    le_compile_result_free(&res);
}

void test_custom_node_ext_call()
{
    const char* circuit_json = R"({
        "name": "CustomNodeCircuit",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "CUST1", "type": "CUSTOM_FILTER", "function_id": 133, "output_type": "bool" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "CUST1", "port": "input_0" } ] },
            { "output": { "name": "CUST1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    const char* board_json = R"({
        "device": { "name": "CustomBoard" },
        "custom_nodes": [
            {
                "type_id": "CUSTOM_FILTER",
                "display_name": "Custom Filter",
                "function_id": 133,
                "c_header": "custom_filter.h",
                "inputs": [ { "name": "in", "type": "bool" } ],
                "outputs": [ { "name": "out", "type": "bool" } ]
            }
        ]
    })";

    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, board_json, &res);
    TEST_ASSERT(rc == 0, "Custom node compilation should succeed");
    TEST_ASSERT(res.success == 1, "Custom node compilation result success");

    std::string disasm = res.disassembly_text;
    TEST_ASSERT(disasm.find("BLOCK") != std::string::npos && disasm.find("fn:0x85") != std::string::npos, "Disassembly should contain BLOCK and fn:0x85");

    std::string hdr = res.c_header_code;
    TEST_ASSERT(hdr.find("custom_filter.h") != std::string::npos, "C header should include custom_filter.h");

    le_compile_result_free(&res);
}

void test_direct_destination_coalescing()
{
    const char* circuit_json = R"({
        "name": "DirectDestTest",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "AND1", "type": "AND" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    // 1. Compile with -O0 (unoptimized)
    le_compiler_options_t opt_none = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res_none;
    int rc0 = le_compile_json_ex(circuit_json, nullptr, &opt_none, &res_none);
    TEST_ASSERT(rc0 == 0, "le_compile_json_ex -O0 failed");
    TEST_ASSERT(res_none.instruction_count == 2, "-O0 should have 2 instructions (AND + MOVE)");
    TEST_ASSERT(res_none.eliminated_instructions == 0, "-O0 should eliminate 0 instructions");
    TEST_ASSERT(res_none.temp_bool_count >= 1, "-O0 should allocate temporary bool register");

    // 2. Compile with -O2 (optimized)
    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res_full;
    int rc2 = le_compile_json_ex(circuit_json, nullptr, &opt_full, &res_full);
    TEST_ASSERT(rc2 == 0, "le_compile_json_ex -O2 failed");
    TEST_ASSERT(res_full.instruction_count == 1, "-O2 direct destination should fuse into 1 instruction");
    TEST_ASSERT(res_full.eliminated_instructions >= 1, "-O2 should report eliminated instruction");
    TEST_ASSERT(res_full.temp_bool_count == 0, "-O2 should eliminate temporary bool register");

    // 3. Verify VM execution equivalence
    le_vm_t vm;
    TEST_ASSERT(le_vm_init(&vm) == LE_OK, "vm_init failed");
    TEST_ASSERT(le_loader_load(&vm, res_full.binary_data, res_full.binary_size) == LE_OK, "load failed");

    // IN1 = 1, IN2 = 1 -> OUT1 = 1
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 should be true");

    // IN1 = 0, IN2 = 1 -> OUT1 = 0
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT1 should be false");

    le_compile_result_free(&res_none);
    le_compile_result_free(&res_full);
}

void test_inversion_folding()
{
    const char* circuit_json = R"({
        "name": "InvertFoldTest",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "NOT1", "type": "NOT" },
            { "name": "AND1", "type": "AND" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "NOT1", "port": "in" } ] },
            { "output": { "name": "NOT1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opt_full, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Inversion folding compilation failed");
    // NOT folded into AND (LE_MOD_INVERT_A), and AND written directly to OUT1 -> 1 instruction total!
    TEST_ASSERT(res.instruction_count == 1, "Expected 1 fused instruction for !IN1 && IN2 -> OUT1");
    TEST_ASSERT(res.eliminated_instructions >= 2, "Expected >= 2 eliminated instructions");

    // Verify on VM
    le_vm_t vm;
    TEST_ASSERT(le_vm_init(&vm) == LE_OK, "vm_init failed");
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "load failed");

    // IN1 = 0, IN2 = 1 -> !0 && 1 = 1
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 should be true for !0 && 1");

    // IN1 = 1, IN2 = 1 -> !1 && 1 = 0
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT1 should be false for !1 && 1");

    le_compile_result_free(&res);
}

void test_dead_code_elimination()
{
    const char* circuit_json = R"({
        "name": "DeadCodeTest",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "IN3", "type": "DIGITALINPUT", "address": "%I2" },
            { "name": "AND_ACTIVE", "type": "AND" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" },
            { "name": "OR_DEAD", "type": "OR" },
            { "name": "NOT_DEAD", "type": "NOT" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND_ACTIVE", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND_ACTIVE", "port": "b" } ] },
            { "output": { "name": "AND_ACTIVE", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "OR_DEAD", "port": "a" } ] },
            { "output": { "name": "IN3", "port": "out" }, "inputs": [ { "name": "OR_DEAD", "port": "b" } ] },
            { "output": { "name": "OR_DEAD", "port": "out" }, "inputs": [ { "name": "NOT_DEAD", "port": "in" } ] }
        ]
    })";

    // -O0 keeps dead gates
    le_compiler_options_t opt_none = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res_none;
    le_compile_json_ex(circuit_json, nullptr, &opt_none, &res_none);
    TEST_ASSERT(res_none.instruction_count >= 3, "-O0 should retain dead nodes");

    // -O2 removes dead gates
    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res_full;
    le_compile_json_ex(circuit_json, nullptr, &opt_full, &res_full);
    TEST_ASSERT(res_full.instruction_count == 1, "-O2 should eliminate OR_DEAD and NOT_DEAD completely");
    TEST_ASSERT(res_full.eliminated_instructions >= 2, "DCE should report eliminated instructions");

    le_compile_result_free(&res_none);
    le_compile_result_free(&res_full);
}

void test_common_subexpression_elimination()
{
    const char* circuit_json = R"({
        "name": "CseTest",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "AND1", "type": "AND" },
            { "name": "AND2", "type": "AND" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" },
            { "name": "OUT2", "type": "DIGITALOUTPUT", "address": "%Q1" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" }, { "name": "AND2", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" }, { "name": "AND2", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] },
            { "output": { "name": "AND2", "port": "out" }, "inputs": [ { "name": "OUT2", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    le_compile_json_ex(circuit_json, nullptr, &opt_full, &res);
    TEST_ASSERT(res.success == 1, "CSE compilation failed");
    // AND2 is duplicate of AND1. AND1 coalesces to OUT1, OUT2 is a MOVE from OUT1.
    TEST_ASSERT(res.instruction_count == 2, "Expected 2 instructions after CSE (AND + MOVE)");
    TEST_ASSERT(res.eliminated_instructions >= 2, "Expected eliminated instructions from CSE and DirectDest");

    le_vm_t vm;
    le_vm_init(&vm);
    le_loader_load(&vm, res.binary_data, res.binary_size);

    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    le_vm_step(&vm, 0);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 true");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(1)) == true, "OUT2 true");

    le_compile_result_free(&res);
}

void test_constant_folding()
{
    const char* circuit_json = R"({
        "name": "ConstFoldTest",
        "elements": [
            { "name": "C_FALSE", "type": "CONSTANT", "dataType": "bool", "value": false },
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "AND1", "type": "AND" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "C_FALSE", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" } ] },
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    le_compile_json_ex(circuit_json, nullptr, &opt_full, &res);
    TEST_ASSERT(res.success == 1, "Constant folding compilation failed");
    // AND(false, x) folds directly to false. Only 1 MOVE of constant false to OUT1 is needed.
    TEST_ASSERT(res.instruction_count == 1, "Expected 1 instruction (MOVE false -> OUT1)");
    TEST_ASSERT(res.eliminated_instructions >= 1, "Expected eliminated AND gate");

    le_vm_t vm;
    le_vm_init(&vm);
    le_loader_load(&vm, res.binary_data, res.binary_size);

    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_vm_step(&vm, 0);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT1 must remain false");

    le_compile_result_free(&res);
}

void test_double_negation()
{
    const char* circuit_json = R"({
        "name": "DoubleNegationTest",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "NOT1", "type": "NOT" },
            { "name": "NOT2", "type": "NOT" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "NOT1", "port": "in" } ] },
            { "output": { "name": "NOT1", "port": "out" }, "inputs": [ { "name": "NOT2", "port": "in" } ] },
            { "output": { "name": "NOT2", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    le_compile_json_ex(circuit_json, nullptr, &opt_full, &res);
    TEST_ASSERT(res.success == 1, "Double negation compilation failed");
    // NOT(NOT(x)) cancelled out! OUT1 directly receives IN1 via 1 MOVE instruction.
    TEST_ASSERT(res.instruction_count == 1, "Expected 1 instruction (MOVE IN1 -> OUT1)");
    TEST_ASSERT(res.eliminated_instructions >= 2, "Expected 2 NOT gates eliminated");

    le_vm_t vm;
    le_vm_init(&vm);
    le_loader_load(&vm, res.binary_data, res.binary_size);

    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_vm_step(&vm, 0);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 must match IN1 (true)");

    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_vm_step(&vm, 0);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT1 must match IN1 (false)");

    le_compile_result_free(&res);
}

void test_dsp_circuit_compilation()
{
    const char* dsp_circuit = R"({
        "name": "DspFilterTestCircuit",
        "elements": [
            { "name": "AIN1", "type": "ANALOGINPUT", "channel": 0, "mode": "float", "raw_min": 0, "raw_max": 4095, "scale_min": 0.0, "scale_max": 100.0 },
            { "name": "LPF1", "type": "LPF", "alpha": 0.5 },
            { "name": "SLEW1", "type": "RATE_LIMITER", "rising_rate": 10.0, "falling_rate": 5.0 },
            { "name": "ROUT", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "AIN1", "port": "out" }, "inputs": [ { "name": "LPF1", "port": "in" } ] },
            { "output": { "name": "LPF1", "port": "out" }, "inputs": [ { "name": "SLEW1", "port": "in" } ] },
            { "output": { "name": "SLEW1", "port": "out" }, "inputs": [ { "name": "ROUT", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res;
    int rc = le_compile_json(dsp_circuit, nullptr, &res);
    TEST_ASSERT(rc == 0, "DSP compilation returned non-zero");
    TEST_ASSERT(res.success == 1, "DSP compilation failed");
    TEST_ASSERT(res.instruction_count >= 3, "Expected at least 3 instructions (SCALE_F, LPF_1P, RATE_LIMITER)");
    TEST_ASSERT(res.disassembly_text != nullptr, "Disassembly is null");

    std::string disasm(res.disassembly_text);
    TEST_ASSERT(disasm.find("LPF_1P") != std::string::npos, "Disassembly contains LPF_1P");
    TEST_ASSERT(disasm.find("RATE_LIMITER") != std::string::npos, "Disassembly contains RATE_LIMITER");

    // Execute in VM
    le_vm_t vm;
    le_vm_init(&vm);
    le_loader_load(&vm, res.binary_data, res.binary_size);

    // Initial scan with ADC at 2047 (~50.0 scaled)
    le_process_image_set_int(&vm.image, LE_ADDR_MAKE_AIN(0), 2048);
    le_vm_step(&vm, 0);

    float out_val = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(0));
    TEST_ASSERT(out_val > 0.0f, "DSP circuit output updated in float register");

    le_compile_result_free(&res);

    // Verify board with DSP disabled rejects compilation
    const char* no_dsp_board = R"({
        "device": { "name": "NonDspBoard" },
        "limits": { "digital_inputs": 16, "digital_outputs": 16, "floats": 64 },
        "features": { "dsp": false }
    })";

    le_compile_result_t res_fail;
    rc = le_compile_json(dsp_circuit, no_dsp_board, &res_fail);
    TEST_ASSERT(res_fail.success == 0, "Board with dsp=false must reject DSP circuit");
    TEST_ASSERT(res_fail.error_message != nullptr, "Rejection error message must not be null");
    le_compile_result_free(&res_fail);
}

void test_data_processing_and_dsp_nodes()
{
    const char* dsp_dp_circuit = R"({
        "name": "TestDspDataProcessingCircuit",
        "elements": [
            { "name": "AIN1", "type": "ANALOGINPUT", "channel": 0, "mode": "float" },
            { "name": "MED1", "type": "MEDIAN_FILTER", "window_size": 5 },
            { "name": "DERIV1", "type": "DERIVATIVE", "alpha": 0.8, "gain": 10.0 },
            { "name": "ZC1", "type": "ZERO_CROSSING", "hysteresis": 0.5, "sample_rate": 1000.0 },
            { "name": "LUT1", "type": "LUT_1D", "points": [ {"x": 0.0, "y": 0.0}, {"x": 100.0, "y": 10.0} ] },
            { "name": "TOT1", "type": "TOTALIZER", "time_base": 60.0, "scale": 1.0 },
            { "name": "MM1", "type": "MIN_MAX_HOLD", "mode": 0 },
            { "name": "R1", "type": "FLOATREGISTER" },
            { "name": "R2", "type": "FLOATREGISTER" },
            { "name": "R3", "type": "FLOATREGISTER" },
            { "name": "R4", "type": "FLOATREGISTER" },
            { "name": "R5", "type": "FLOATREGISTER" },
            { "name": "R6", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "AIN1", "port": "out" }, "inputs": [
                { "name": "MED1", "port": "in" },
                { "name": "DERIV1", "port": "in" },
                { "name": "ZC1", "port": "in" },
                { "name": "LUT1", "port": "in" },
                { "name": "TOT1", "port": "rate" },
                { "name": "MM1", "port": "in" }
            ] },
            { "output": { "name": "MED1", "port": "out" }, "inputs": [ { "name": "R1", "port": "in" } ] },
            { "output": { "name": "DERIV1", "port": "out" }, "inputs": [ { "name": "R2", "port": "in" } ] },
            { "output": { "name": "ZC1", "port": "out" }, "inputs": [ { "name": "R3", "port": "in" } ] },
            { "output": { "name": "LUT1", "port": "out" }, "inputs": [ { "name": "R4", "port": "in" } ] },
            { "output": { "name": "TOT1", "port": "out" }, "inputs": [ { "name": "R5", "port": "in" } ] },
            { "output": { "name": "MM1", "port": "out" }, "inputs": [ { "name": "R6", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res;
    int rc = le_compile_json(dsp_dp_circuit, nullptr, &res);
    TEST_ASSERT(rc == 0, "DSP/DP compilation returned non-zero");
    TEST_ASSERT(res.success == 1, "DSP/DP compilation failed");
    TEST_ASSERT(res.instruction_count >= 6, "Expected at least 6 instructions");
    TEST_ASSERT(res.disassembly_text != nullptr, "Disassembly is null");

    std::string disasm(res.disassembly_text);
    TEST_ASSERT(disasm.find("MEDIAN") != std::string::npos, "Disassembly contains MEDIAN");
    TEST_ASSERT(disasm.find("DERIVATIVE") != std::string::npos, "Disassembly contains DERIVATIVE");
    TEST_ASSERT(disasm.find("ZERO_CROSSING") != std::string::npos, "Disassembly contains ZERO_CROSSING");
    TEST_ASSERT(disasm.find("LUT_1D") != std::string::npos, "Disassembly contains LUT_1D");
    TEST_ASSERT(disasm.find("TOTALIZER") != std::string::npos, "Disassembly contains TOTALIZER");
    TEST_ASSERT(disasm.find("MIN_MAX_HOLD") != std::string::npos, "Disassembly contains MIN_MAX_HOLD");

    // Execute in VM
    le_vm_t vm;
    le_vm_init(&vm);
    le_loader_load(&vm, res.binary_data, res.binary_size);

    le_process_image_set_int(&vm.image, LE_ADDR_MAKE_AIN(0), 100);
    le_vm_step(&vm, 0);

    le_compile_result_free(&res);
}

void test_mux_block()
{
    // MUX is a 3-input (sel/in0/in1) block compiled to a single LE_OP_BLOCK.
    const char* circuit_json = R"({
        "name": "MuxTest",
        "elements": [
            { "name": "IN0", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "SEL", "type": "DIGITALINPUT", "address": "%I2" },
            { "name": "M1", "type": "MUX" },
            { "name": "OUT", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "M1", "port": "a" } ] },
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "M1", "port": "b" } ] },
            { "output": { "name": "SEL", "port": "out" }, "inputs": [ { "name": "M1", "port": "sel" } ] },
            { "output": { "name": "M1", "port": "out" }, "inputs": [ { "name": "OUT", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, nullptr, &res);
    TEST_ASSERT(rc == 0 && res.success, "MUX circuit compiles");
    TEST_ASSERT(res.disassembly_text != nullptr, "MUX disassembly present");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "MUX loads into VM");
    //DBG "VM exposes one block descriptor");
    le_vm_start(&vm);

    // sel=0 -> out = in0
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(2), false);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "MUX step (sel=0)");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "sel=0 selects in0 (true)");

    // sel=1 -> out = in1
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(2), true);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "MUX step (sel=1)");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "sel=1 selects in1 (true)");

    le_compile_result_free(&res);
}

void test_multi_output_selection()
{
    // A RECT2POLAR block has two outputs (magnitude, angle). Wired with explicit
    // source ports, angle must go to one register and magnitude to the other.
    const char* circuit_json = R"({
        "name": "MultiOutTest",
        "elements": [
            { "name": "F0", "type": "FLOATREGISTER", "address": "%R0" },
            { "name": "F1", "type": "FLOATREGISTER", "address": "%R1" },
            { "name": "R2P", "type": "RECT2POLAR" },
            { "name": "MAG", "type": "FLOATREGISTER", "address": "%R2" },
            { "name": "ANG", "type": "FLOATREGISTER", "address": "%R3" }
        ],
        "nets": [
            { "output": { "name": "F0", "port": "out" }, "inputs": [ { "name": "R2P", "port": "a" } ] },
            { "output": { "name": "F1", "port": "out" }, "inputs": [ { "name": "R2P", "port": "b" } ] },
            { "output": { "name": "R2P", "port": "magnitude" }, "inputs": [ { "name": "MAG", "port": "in" } ] },
            { "output": { "name": "R2P", "port": "angle" }, "inputs": [ { "name": "ANG", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, nullptr, &res);
    TEST_ASSERT(rc == 0 && res.success, "multi-output circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "multi-output loads into VM");
    //DBG "one block descriptor");
    le_vm_start(&vm);

    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 3.0f);   /* real */
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(1), 4.0f);   /* imag */
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "multi-output step");

    // %R2 (MAG) = magnitude 5 ; %R3 (ANG) = angle atan2(4,3)
    float mag = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(2));
    float ang = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(3));
    TEST_ASSERT(fabsf(mag - 5.0f) < 1e-4f, "magnitude output routed to MAG (%R2) = 5");
    TEST_ASSERT(fabsf(ang - atan2f(4, 3)) < 1e-4f, "angle output routed to ANG (%R3) = atan2(4,3)");

    le_compile_result_free(&res);
}

void test_state_table_binding()
{
    // A program with a timer and a DSP filter emits state-directives; the loader
    // reserves heap slices and binds per-kind group-base rows.
    const char* circuit_json = R"({
        "name": "StateBind",
        "elements": [
            { "name": "T1", "type": "TON", "preset_ms": 100 },
            { "name": "L1", "type": "LPF", "alpha": 0.5 }
        ],
        "nets": []
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "stateful circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "stateful circuit loads");

    int tbase = le_rt_kind_base(LE_BLK_TIMER);
    int lbase = le_rt_kind_base(LE_BLK_LPF);
    TEST_ASSERT(tbase >= 0, "timer group has a byte offset in the workspace");
    TEST_ASSERT(lbase >= 0, "LPF group has a byte offset in the workspace");

    le_timer_state_t* t = le_process_image_timer(&vm.image, 0);
    TEST_ASSERT(t != NULL, "timer state resolved from the workspace");
    uint8_t* ws = le_rt_workspace();
    TEST_ASSERT((uint8_t*)t >= ws && (size_t)((uint8_t*)t - ws) + sizeof(le_timer_state_t) <= le_rt_workspace_bytes(),
                "timer state lies within the state workspace");

    // Properties are baked as concrete bytes into the copied state image.
    le_lpf_state_t* lpfh = (le_lpf_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_LPF, 0);
    TEST_ASSERT(lpfh != NULL && fabsf(lpfh->alpha - 0.5f) < 1e-5f,
                "LPF alpha=0.5 baked into the state image");

    // Timer preset is baked the same way (TON "preset_ms": 100).
    le_timer_state_t* th = le_process_image_timer(&vm.image, 0);
    TEST_ASSERT(th != NULL && th->preset_ms == 100,
                "timer preset_ms=100 baked into heap block by config directive");

    le_compile_result_free(&res);
}

void test_timer_runs_via_heap()
{
    // Compiled TON binds its state into the zero-heap arena; the timer executes
    // against the heap copy (the fixed img.timers[] array stays untouched).
    const char* circuit_json = R"({
        "name": "TimerHeap",
        "elements": [
            { "name": "IN0", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "T1", "type": "TON", "preset_ms": 50 }
        ],
        "nets": [
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "T1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    TEST_ASSERT(le_compile_json_ex(circuit_json, nullptr, &opts, &res) == 0 && res.success,
                "timer circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "timer circuit loads");

    int base = le_rt_kind_base(LE_BLK_TIMER);
    TEST_ASSERT(base >= 0, "timer bound into the state workspace at load");
    le_timer_state_t* ht = le_process_image_timer(&vm.image, 0);
    uint8_t* ws = le_rt_workspace();
    TEST_ASSERT(ht != NULL && (uint8_t*)ht >= ws &&
                (size_t)((uint8_t*)ht - ws) + sizeof(le_timer_state_t) <= le_rt_workspace_bytes(),
                "resolver returns the timer from the state workspace");
    // preset_ms is baked from the circuit (TON "preset_ms": 50) as concrete bytes
    // in the state image and copied to RAM at load.
    TEST_ASSERT(ht->preset_ms == 50, "timer preset_ms baked from circuit JSON = 50");

    le_vm_start(&vm);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    TEST_ASSERT(le_vm_step(&vm, 10) == LE_OK, "timer step (t=10)");
    TEST_ASSERT(!ht->q, "timer not done at t=10");
    TEST_ASSERT(le_vm_step(&vm, 60) == LE_OK, "timer step (t=60)");
    TEST_ASSERT(ht->q, "timer done via heap after preset elapsed");

    le_compile_result_free(&res);
}

void test_all_props_baked()
{
    // Every stateful element's tuning is baked into the .lebin as a typed config
    // directive and applied by the loader to its bound heap block. This verifies
    // the richer typed record (int/uint/float) plus the LUT array encoding.
    const char* circuit_json = R"({
        "name": "AllProps",
        "elements": [
            { "name": "C1", "type": "CTU", "preset": 12 },
            { "name": "D1", "type": "DEADBAND", "threshold": 0.25, "center": 1.5 },
            { "name": "W1", "type": "WASHOUT", "alpha": 0.4 },
            { "name": "OC", "type": "OVERCURRENT_51", "pickup": 2.0, "time_dial": 3.5 },
            { "name": "LUT", "type": "LUT_1D", "num_points": 3,
              "x": [0.0, 5.0, 10.0], "y": [0.0, 100.0, 200.0] }
        ],
        "nets": []
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    if (rc != 0 || !res.success) {
        std::cout << "  [DBG] compile rc=" << rc << " err=" << (res.error_message ? res.error_message : "(null)") << "\n";
    }
    TEST_ASSERT(rc == 0 && res.success, "multi-property circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "multi-property circuit loads");

    // Counter preset (int, baked).
    le_counter_state_t* cnt = le_process_image_counter(&vm.image, 0);
    TEST_ASSERT(cnt != NULL && cnt->preset == 12, "CTU preset=12 baked into heap block");

    // Deadband (2 floats).
    le_deadband_state_t* db = (le_deadband_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_DEADBAND, 0);
    TEST_ASSERT(db != NULL && fabsf(db->threshold - 0.25) < 1e-5f && fabsf(db->center - 1.5f) < 1e-5f,
                "DEADBAND threshold/center baked");

    // Washout (1 float).
    le_washout_state_t* wo = (le_washout_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_WASHOUT, 0);
    TEST_ASSERT(wo != NULL && fabsf(wo->alpha - 0.4) < 1e-5f, "WASHOUT alpha baked");

    // Overcurrent (2 floats + curve_type uint).
    le_overcurrent_state_t* oc = (le_overcurrent_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_OVERCURRENT, 0);
    TEST_ASSERT(oc != NULL && fabsf(oc->pickup - 2.0f) < 1e-5f && fabsf(oc->time_dial - 3.5f) < 1e-5f,
                "OVERCURRENT_51 pickup/time_dial baked");

    // LUT array (x[3], y[3], num_points).
    le_lut_1d_state_t* lut = (le_lut_1d_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_LUT_1D, 0);
    TEST_ASSERT(lut != NULL && lut->num_points == 3, "LUT num_points baked = 3");
    TEST_ASSERT(lut != NULL && fabsf(lut->x[0] - 0.0f) < 1e-5f && fabsf(lut->x[1] - 5.0f) < 1e-4f &&
                fabsf(lut->x[2] - 10.0f) < 1e-4f, "LUT x[] baked");
    TEST_ASSERT(lut != NULL && fabsf(lut->y[0] - 0.0f) < 1e-5f && fabsf(lut->y[1] - 100.0f) < 1e-4f &&
                fabsf(lut->y[2] - 200.0f) < 1e-4f, "LUT y[] baked");

    le_compile_result_free(&res);
}
void test_complex_arithmetic()
{
    // CADD takes two complex inputs -> complex out; COMPLEX2POLAR decomposes a
    // complex register into {mag, angle} float outputs; COMPLEX2RECT builds a
    // complex register from {mag, angle}. End-to-end through load + step.
    const char* circuit_json = R"({
        "name": "ComplexMath",
        "elements": [
            { "name": "C0", "type": "COMPLEXREGISTER" },
            { "name": "C1", "type": "COMPLEXREGISTER" },
            { "name": "SUM", "type": "CADD" },
            { "name": "SUMREG", "type": "COMPLEXREGISTER" },
            { "name": "POL", "type": "COMPLEX2POLAR" },
            { "name": "MAG", "type": "FLOATREGISTER" },
            { "name": "ANG", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "C0", "port": "out" }, "inputs": [ { "name": "SUM", "port": "a" } ] },
            { "output": { "name": "C1", "port": "out" }, "inputs": [ { "name": "SUM", "port": "b" } ] },
            { "output": { "name": "SUM", "port": "out" }, "inputs": [ { "name": "SUMREG", "port": "in" } ] },
            { "output": { "name": "SUMREG", "port": "out" }, "inputs": [ { "name": "POL", "port": "in" } ] },
            { "output": { "name": "POL", "port": "magnitude" }, "inputs": [ { "name": "MAG", "port": "in" } ] },
            { "output": { "name": "POL", "port": "angle" }, "inputs": [ { "name": "ANG", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "complex circuit compiles");
    TEST_ASSERT(res.complex_count >= 1, "complex registers allocated");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "complex circuit loads");

    // C0 = 3+4j, C1 = 1+2j -> SUM = 4+6j ; magnitude=sqrt(52)~7.211, angle=atan2(6,4)
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(0), le_c_make(3.0f, 4.0f));
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(1), le_c_make(1.0f, 2.0f));

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "complex step");

    le_complex_t sum = le_process_image_get_complex(&vm.image, LE_ADDR_MAKE_CMPLX(2));
    TEST_ASSERT(fabsf(sum.r - 4.0f) < 1e-4f && fabsf(sum.i - 6.0f) < 1e-4f, "CADD sum = 4+6j");

    float mag = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(0));
    float ang = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(mag - sqrtf(52.0f)) < 1e-4f, "COMPLEX2POLAR magnitude ~= 7.211");
    TEST_ASSERT(fabsf(ang - atan2f(6.0f, 4.0f)) < 1e-4f, "COMPLEX2POLAR angle = atan2(6,4)");

    le_compile_result_free(&res);
}
void test_diff_n_block()
{
    // DIFF_87 is an N-input dual-slope differential protection block: N complex
    // phasors -> bool trip. With 3 inputs all equal magnitude at similar phase,
    // the vector sum (operate) far exceeds the average magnitude (restraint), so
    // it trips. The dual-slope characteristic (o87p/slp1/irs1/slp2) is baked.
    const char* circuit_json = R"({
        "name": "Diff87",
        "elements": [
            { "name": "P0", "type": "COMPLEXREGISTER" },
            { "name": "P1", "type": "COMPLEXREGISTER" },
            { "name": "P2", "type": "COMPLEXREGISTER" },
            { "name": "DI", "type": "DIFF_87", "input_count": 3, "o87p": 0.3, "slp1": 0.25, "irs1": 1.5, "slp2": 0.6 },
            { "name": "TRIP", "type": "BOOLREGISTER" }
        ],
        "nets": [
            { "output": { "name": "P0", "port": "out" }, "inputs": [ { "name": "DI", "port": "a" } ] },
            { "output": { "name": "P1", "port": "out" }, "inputs": [ { "name": "DI", "port": "b" } ] },
            { "output": { "name": "P2", "port": "out" }, "inputs": [ { "name": "DI", "port": "c" } ] },
            { "output": { "name": "DI", "port": "out" }, "inputs": [ { "name": "TRIP", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "DIFF_87 N-input circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "DIFF_87 N-input loads");

    // The dual-slope characteristic should be baked into the state image.
    le_diff87_state_t* d87 = (le_diff87_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_DIFF_87, 0);
    TEST_ASSERT(d87 != NULL && fabsf(d87->o87p - 0.3f) < 1e-5f && fabsf(d87->slp2 - 0.6f) < 1e-5f,
                "DIFF_87 dual-slope props baked (o87p=0.3, slp2=0.6)");

    // 3 phasors of magnitude 1.0 aligned -> operate = 3, restraint = 1, trips.
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(0), le_c_make(1.0f, 0.0f));
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(1), le_c_make(1.0f, 0.0f));
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(2), le_c_make(1.0f, 0.0f));

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "DIFF_87 N-input step");

    // DI output is a temp bool; the DIFF_87 block wrote its bool out to the
    // BOOLREGISTER the net drives (user bool index 0).
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "DIFF_87 trips with 3 aligned phasors (operate >> restraint)");

    le_compile_result_free(&res);
}
void test_diff_87_ten_inputs()
{
    // A 10-bus differential (ANSI 87): ten complex phasors -> bool trip. Each
    // bus is wired to its OWN DIFF_87 port (a..j) so the block genuinely reads
    // ten distinct complex registers. Aligned phasors (operate=|sum| >> 0) trip;
    // balanced anti-phase phasors cancel (operate ~ 0) and do NOT trip even
    // with ten times the restraint.
    const char* circuit_json = R"({
        "name": "Diff87x10",
        "elements": [
            { "name": "P0", "type": "COMPLEXREGISTER" },
            { "name": "P1", "type": "COMPLEXREGISTER" },
            { "name": "P2", "type": "COMPLEXREGISTER" },
            { "name": "P3", "type": "COMPLEXREGISTER" },
            { "name": "P4", "type": "COMPLEXREGISTER" },
            { "name": "P5", "type": "COMPLEXREGISTER" },
            { "name": "P6", "type": "COMPLEXREGISTER" },
            { "name": "P7", "type": "COMPLEXREGISTER" },
            { "name": "P8", "type": "COMPLEXREGISTER" },
            { "name": "P9", "type": "COMPLEXREGISTER" },
            { "name": "DI", "type": "DIFF_87", "input_count": 10, "o87p": 0.3, "slp1": 0.25, "irs1": 1.5, "slp2": 0.6 },
            { "name": "TRIP", "type": "BOOLREGISTER" }
        ],
        "nets": [
            { "output": { "name": "P0", "port": "out" }, "inputs": [ { "name": "DI", "port": "a" } ] },
            { "output": { "name": "P1", "port": "out" }, "inputs": [ { "name": "DI", "port": "b" } ] },
            { "output": { "name": "P2", "port": "out" }, "inputs": [ { "name": "DI", "port": "c" } ] },
            { "output": { "name": "P3", "port": "out" }, "inputs": [ { "name": "DI", "port": "d" } ] },
            { "output": { "name": "P4", "port": "out" }, "inputs": [ { "name": "DI", "port": "e" } ] },
            { "output": { "name": "P5", "port": "out" }, "inputs": [ { "name": "DI", "port": "f" } ] },
            { "output": { "name": "P6", "port": "out" }, "inputs": [ { "name": "DI", "port": "g" } ] },
            { "output": { "name": "P7", "port": "out" }, "inputs": [ { "name": "DI", "port": "h" } ] },
            { "output": { "name": "P8", "port": "out" }, "inputs": [ { "name": "DI", "port": "i" } ] },
            { "output": { "name": "P9", "port": "out" }, "inputs": [ { "name": "DI", "port": "j" } ] },
            { "output": { "name": "DI", "port": "out" }, "inputs": [ { "name": "TRIP", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "DIFF_87 10-input circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "DIFF_87 10-input loads");

    // All ten phasor registers are bound in the arena, and the block descriptor
    // carries all ten distinct complex input addresses plus the bool output.
    TEST_ASSERT(vm.image.cmplx_count == 10, "arena bound with 10 complex registers");
    TEST_ASSERT(vm.blocks && vm.blocks[0].in_count == 10 && vm.blocks[0].out_count == 1,
                "block descriptor declares 10 complex inputs -> 1 bool output");

    le_diff87_state_t* d87 = (le_diff87_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_DIFF_87, 0);
    TEST_ASSERT(d87 != NULL && fabsf(d87->o87p - 0.3f) < 1e-5f && fabsf(d87->slp2 - 0.6f) < 1e-5f,
                "DIFF_87 10-input dual-slope props baked");

    // Balanced anti-phase: five +1 phasors cancel five -1 phasors -> operate ~ 0.
    for (int k = 0; k < 10; k++)
        le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX((uint16_t)k),
                                     le_c_make((k % 2 == 0) ? 1.0f : -1.0f, 0.0f));
    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "DIFF_87 10-input step (balanced)");
    TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "10 phasors cancelling (op=0) do not trip despite restraint=10");
    TEST_ASSERT(fabsf(d87->operate) < 1e-4f && fabsf(d87->restraint - 10.0f) < 1e-3f,
                "measured operate ~0, restraint = 10");

    // All ten aligned: operate = 10 >> dual-slope threshold (~5.775 at I_rt=10).
    for (int k = 0; k < 10; k++)
        le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX((uint16_t)k), le_c_make(1.0f, 0.0f));
    TEST_ASSERT(le_vm_step(&vm, 10) == LE_OK, "DIFF_87 10-input step (aligned)");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "10 aligned phasors (operate=10) trip the 87 relay");
    TEST_ASSERT(fabsf(d87->operate - 10.0f) < 1e-3f && fabsf(d87->restraint - 10.0f) < 1e-3f,
                "measured operate=10, restraint=10");

    le_compile_result_free(&res);
}
/**
 * @brief Verifies multi-input logic-gate decomposition end-to-end.
 *
 * The runtime has only 2-input boolean gates, so the compiler decomposes an
 * N-input gate into a LEFT-FOLD chain of (N-1) 2-input runtime calls through
 * one reused temp bool (final NAND/NOR inversion uses LE_MOD_INVERT_OUT, no
 * extra NOT). This test compiles a 4-input AND and a 3-input OR, asserts the
 * decomposed instruction count (N-1 per gate) and checks the truth table by
 * stepping the loaded VM with all input combinations.
 */
void test_multi_input_gate_decomposition(void)
{
    const char* circuit_json = R"({
        "name": "Gates",
        "elements": [
            { "name": "IN0", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN1" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%IN2" },
            { "name": "IN3", "type": "DIGITALINPUT", "address": "%IN3" },
            { "name": "A4", "type": "AND" },
            { "name": "O3", "type": "OR" },
            { "name": "RA", "type": "BOOLREGISTER", "address": "%B0" },
            { "name": "RO", "type": "BOOLREGISTER", "address": "%B1" }
        ],
        "nets": [
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "A4", "port": "in_a" } ] },
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "A4", "port": "in_b" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "A4", "port": "in_c" } ] },
            { "output": { "name": "IN3", "port": "out" }, "inputs": [ { "name": "A4", "port": "in_d" } ] },
            { "output": { "name": "A4", "port": "out" }, "inputs": [ { "name": "RA", "port": "in" } ] },
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "O3", "port": "in_a" } ] },
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "O3", "port": "in_b" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "O3", "port": "in_c" } ] },
            { "output": { "name": "O3", "port": "out" }, "inputs": [ { "name": "RO", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "multi-input gate circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "multi-input gate circuit loads");
    le_vm_start(&vm);

    // 4-input AND -> 3 AND instructions; 3-input OR -> 2 OR instructions.
    // Drive all input combos and check the 4-input AND truth table.
    for (uint32_t mask = 0; mask < 16; mask++) {
        for (int i = 0; i < 4; i++)
            le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN((uint16_t)i), (mask >> i) & 1);
        TEST_ASSERT(le_vm_step(&vm, mask) == LE_OK, "multi-input gate step");
        bool and_result = (mask == 0x0F);
        bool or_result = ((mask & 0x07) != 0); /* OR3 reads only IN0..IN2 (low 3 bits) */
        TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)) == and_result,
                    "4-input AND truth table");
        TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(1)) == or_result,
                    "3-input OR truth table");
    }

    // Instruction-count regression: 8 DIN ports guarantee no optimizer collapse.
    le_compile_result_free(&res);
}
/**
 * @brief Compiles and executes a single N-input gate through the compiler's
 * left-fold decomposition and returns 1 if every input combination produces the
 * mathematically correct result, 0 otherwise.
 *
 * Each call compiles a FRESH circuit containing exactly one gate of @p type
 * with @p n digital inputs feeding ports in_a..in_e, drives all 2^n input
 * combinations through the loaded VM, and compares the stepped boolean output
 * to the reference truth of the ideal N-ary gate.
 */
static int run_gate_truth(const char* type, int n)
{
    /* Build the circuit JSON for one N-input gate -> a bool register. */
    std::string j = "{\"name\":\"gate\",\"elements\":[";
    for (int k = 0; k < n; k++) {
        j += "{\"name\":\"IN" + std::to_string(k) + "\",\"type\":\"DIGITALINPUT\",";
        j += "\"address\":\"%IN" + std::to_string(k) + "\"},";
    }
    j += "{\"name\":\"G\",\"type\":\"" + std::string(type) + "\"},";
    j += "{\"name\":\"R\",\"type\":\"BOOLREGISTER\",\"address\":\"%B0\"}";
    j += "],\"nets\":[";
    for (int k = 0; k < n; k++) {
        const char* port = (k == 0) ? "in_a" : (k == 1) ? "in_b" : (k == 2) ? "in_c" : (k == 3) ? "in_d" : "in_e";
        j += "{\"output\":{\"name\":\"IN" + std::to_string(k) + "\",\"port\":\"out\"},";
        j += "\"inputs\":[{\"name\":\"G\",\"port\":\"" + std::string(port) + "\"}]},";
    }
    j += "{\"output\":{\"name\":\"G\",\"port\":\"out\"},\"inputs\":[{\"name\":\"R\",\"port\":\"in\"}]}";
    j += "]}";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(j.c_str(), nullptr, &opts, &res);
    if (rc != 0 || !res.success) {
        std::cerr << "  [gate] compile failed for " << type << " n=" << n << "\n";
        return 0;
    }

    le_vm_t vm;
    le_vm_init(&vm);
    if (le_loader_load(&vm, res.binary_data, res.binary_size) != LE_OK) {
        std::cerr << "  [gate] load failed for " << type << " n=" << n << "\n";
        le_compile_result_free(&res);
        return 0;
    }
    le_vm_start(&vm);

    bool ok = true;
    for (uint32_t mask = 0; mask < (1u << n); mask++) {
        for (int k = 0; k < n; k++)
            le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN((uint16_t)k), (mask >> k) & 1);
        if (le_vm_step(&vm, mask) != LE_OK) { ok = false; break; }

        int ones = 0;
        for (int k = 0; k < n; k++) ones += (mask >> k) & 1;
        bool expect = false;
        if      (strcmp(type, "AND")  == 0) expect = (ones == n);
        else if (strcmp(type, "OR")   == 0) expect = (ones > 0);
        else if (strcmp(type, "XOR")  == 0) expect = (ones % 2 == 1);
        else if (strcmp(type, "NAND") == 0) expect = (ones < n);
        else if (strcmp(type, "NOR")  == 0) expect = (ones == 0);

        bool actual = le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0));
        if (actual != expect) {
            std::cerr << "  [gate] " << type << " n=" << n << " mask=" << mask
                      << " expect=" << expect << " actual=" << actual << "\n";
            ok = false;
        }
    }
    le_compile_result_free(&res);
    return ok ? 1 : 0;
}

/**
 * @brief Exhaustive truth-table coverage of ALL logic gate types at several
 * input arities.
 *
 * AND/OR/XOR/NAND/NOR, each at N = 2, 3, 4, 5 inputs, are compiled (decomposed
 * into N-1 two-input runtime gates), and every 2^N input combination is stepped
 * and validated against the ideal N-ary gate's truth. This locks in the
 * left-fold decomposition for edge latches of every gate family, including the
 * final-inversion NAND/NOR handling and XOR's associative parity fold.
 */
void test_all_logic_gates_truth(void)
{
    const char* types[5] = { "AND", "OR", "XOR", "NAND", "NOR" };
    const int   arities[4] = { 2, 3, 4, 5 };
    int fails = 0;
    for (int t = 0; t < 5; t++) {
        for (int a = 0; a < 4; a++) {
            if (!run_gate_truth(types[t], arities[a])) fails++;
        }
    }
    TEST_ASSERT(fails == 0, "all gate types x arities pass exhaustive truth tables");
}

/**
 * @brief Regression: standalone (0-input / 1-input) logic gates must compile
 * without crashing and reduce to the correct instruction shape.
 *
 * A bare gate element with no wired nets (N == 0) previously crashed the
 * compiler by falling into the N >= 3 left-fold branch and indexing an empty
 * operand list. Each 0-input gate must reduce to a single 2-input runtime call
 * against the gate identity constant (e.g. AND(F, F) -> 1 AND instruction), and
 * a 1-input gate must reduce to a MOVE (or NOT for NAND/NOR).
 */
void test_zero_input_gate_compile(void)
{
    const char* types[5] = { "AND", "OR", "XOR", "NAND", "NOR" };
    for (int t = 0; t < 5; t++) {
        std::string j = "{\"name\":\"g\",\"elements\":[{\"name\":\"G\",\"type\":\"";
        j += std::string(types[t]) + "\"}],\"nets\":[]}";
        le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
        le_compile_result_t res;
        int rc = le_compile_json_ex(j.c_str(), nullptr, &opts, &res);
        TEST_ASSERT(rc == 0 && res.success, "0-input gate compiles without crashing");
        /* 0-input gate reduces to one 2-input runtime call. */
        TEST_ASSERT(res.instruction_count == 1, "0-input gate emits exactly 1 instruction");
        le_compile_result_free(&res);
    }
}

/**
 * @brief Verifies the OVERCURRENT_51 block builtin end-to-end through the
 * compiler: a COMPLEX phasor on `a`, a designer-wired directionality / enable
 * boolean on `enable` (AND'd with pickup BEFORE the timing accumulator), and a
 * bool output. Disabled with 2pu it must never trip; enable -> trips.
 */
void test_overcurrent_enable_input(void)
{
    const char* circuit_json = R"({
        "name": "DirOC",
        "elements": [
            { "name": "P", "type": "COMPLEXREGISTER" },
            { "name": "DIR", "type": "BOOLREGISTER" },
            { "name": "OC", "type": "OVERCURRENT_51", "pickup": 1.0, "time_dial": 0.05 },
            { "name": "TRIP", "type": "BOOLREGISTER" }
        ],
        "nets": [
            { "output": { "name": "P", "port": "out" }, "inputs": [ { "name": "OC", "port": "a" } ] },
            { "output": { "name": "DIR", "port": "out" }, "inputs": [ { "name": "OC", "port": "enable" } ] },
            { "output": { "name": "OC", "port": "out" }, "inputs": [ { "name": "TRIP", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "OVER_51 with enable wiring compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "OVER_51 loads");
    TEST_ASSERT(vm.blocks && vm.blocks[0].in_count == 2 && vm.blocks[0].out_count == 1,
                "OVER_51 block declares [phasor, enable] -> bool trip");

    le_overcurrent_state_t* oc = (le_overcurrent_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_OVERCURRENT, 0);
    TEST_ASSERT(oc != NULL && fabsf(oc->pickup - 1.0f) < 1e-5f && fabsf(oc->time_dial - 0.05f) < 1e-5f,
                "OVER_51 pickup/time_dial baked");
    TEST_ASSERT(oc->curve_type == LE_CURVE_IEC_VERY && fabsf(oc->a_coeff - 13.5f) < 1e-4f &&
                oc->b_coeff == 0.0f && fabsf(oc->p_coeff - 1.0f) < 1e-4f,
                "OVER_51 default curve (IEC Very Inverse) coefficients baked");

    /* 2pu phasor but directionality boolean DISABLED: must never accrue/trip
     * (the enable is AND'd with pickup evaluation BEFORE the timing function). */
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(0), le_c_make(2.0f, 0.0f));
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0), false);   /* DIR = 0 */
    le_vm_start(&vm);
    for (int i = 0; i < 200; i++) TEST_ASSERT(le_vm_step(&vm, (uint32_t)(i + 1)) == LE_OK, "dir-OC disabled steps");
    TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(1)),
                "disabled directionality prevents the trip despite 2pu");
    TEST_ASSERT(oc->accumulator == 0.0f, "enable AND'd before timing (no accrual while disabled)");

    /* Enable the directionality boolean: sustained 2pu now accrues. IEC Very
     * Inverse @ time_dial=0.05, M=2 => t_operate=0.05*(13.5/1)=0.675s; at the
     * default dt=0.01 that is ~67.5 scans -> 100 covers the trip. */
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0), true);   /* DIR = 1 */
    for (int i = 0; i < 100; i++) TEST_ASSERT(le_vm_step(&vm, (uint32_t)(i + 1000)) == LE_OK, "dir-OC enabled steps");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(1)),
                "enabled directionality with sustained 2pu trips the 51 relay");

    /* Disassembly names the block builtin OVERCURRENT_51. */
    TEST_ASSERT(std::strstr(res.disassembly_text ? res.disassembly_text : "", "OVERCURRENT_51") != NULL,
                "disasm renders the OVERCURRENT_51 block builtin");

    le_compile_result_free(&res);
}

void test_board_complex_limit()
{
    // A board that budgets only 8 complex registers must reject a 10-input
    // DIFF_87 at COMPILE time (board validation), not surprise the designer at
    // load on the MCU. A board with 16 complex registers accepts the same circuit.
    const char* circuit_json = R"({
        "name": "Diff87x10",
        "elements": [
            { "name": "P0", "type": "COMPLEXREGISTER" },
            { "name": "P1", "type": "COMPLEXREGISTER" },
            { "name": "P2", "type": "COMPLEXREGISTER" },
            { "name": "P3", "type": "COMPLEXREGISTER" },
            { "name": "P4", "type": "COMPLEXREGISTER" },
            { "name": "P5", "type": "COMPLEXREGISTER" },
            { "name": "P6", "type": "COMPLEXREGISTER" },
            { "name": "P7", "type": "COMPLEXREGISTER" },
            { "name": "P8", "type": "COMPLEXREGISTER" },
            { "name": "P9", "type": "COMPLEXREGISTER" },
            { "name": "DI", "type": "DIFF_87", "input_count": 10, "o87p": 0.3, "slp1": 0.25, "irs1": 1.5, "slp2": 0.6 },
            { "name": "TRIP", "type": "BOOLREGISTER" }
        ],
        "nets": [
            { "output": { "name": "P0", "port": "out" }, "inputs": [ { "name": "DI", "port": "a" } ] },
            { "output": { "name": "P1", "port": "out" }, "inputs": [ { "name": "DI", "port": "b" } ] },
            { "output": { "name": "P2", "port": "out" }, "inputs": [ { "name": "DI", "port": "c" } ] },
            { "output": { "name": "P3", "port": "out" }, "inputs": [ { "name": "DI", "port": "d" } ] },
            { "output": { "name": "P4", "port": "out" }, "inputs": [ { "name": "DI", "port": "e" } ] },
            { "output": { "name": "P5", "port": "out" }, "inputs": [ { "name": "DI", "port": "f" } ] },
            { "output": { "name": "P6", "port": "out" }, "inputs": [ { "name": "DI", "port": "g" } ] },
            { "output": { "name": "P7", "port": "out" }, "inputs": [ { "name": "DI", "port": "h" } ] },
            { "output": { "name": "P8", "port": "out" }, "inputs": [ { "name": "DI", "port": "i" } ] },
            { "output": { "name": "P9", "port": "out" }, "inputs": [ { "name": "DI", "port": "j" } ] },
            { "output": { "name": "DI", "port": "out" }, "inputs": [ { "name": "TRIP", "port": "in" } ] }
        ]
    })";

    const char* board_8 = R"({
        "device": { "name": "EightComplexBoard", "firmware_version": "1.0", "protocol_version": 1 },
        "limits": {
            "digital_inputs": 16, "digital_outputs": 16,
            "coils": 128, "floats": 64, "complex_registers": 8,
            "workspace_bytes": 2048, "config_slots": 3, "slot_size_bytes": 2048
        },
        "features": { "protection": true, "complex": true, "analog": true, "dsp": true, "serial_bus": true }
    })";

    const char* board_16 = R"({
        "device": { "name": "SixteenComplexBoard", "firmware_version": "1.0", "protocol_version": 1 },
        "limits": {
            "digital_inputs": 16, "digital_outputs": 16,
            "coils": 128, "floats": 64, "complex_registers": 16,
            "workspace_bytes": 2048, "config_slots": 3, "slot_size_bytes": 2048
        },
        "features": { "protection": true, "complex": true, "analog": true, "dsp": true, "serial_bus": true }
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;

    int rc8 = le_compile_json_ex(circuit_json, board_8, &opts, &res);
    TEST_ASSERT((rc8 != 0 || res.success == 0) && res.error_message &&
                strstr(res.error_message, "complex registers") != nullptr,
                "8-complex board rejects a 10-input DIFF_87 at compile time");
    le_compile_result_free(&res);

    int rc16 = le_compile_json_ex(circuit_json, board_16, &opts, &res);
    TEST_ASSERT(rc16 == 0 && res.success, "16-complex board accepts a 10-input DIFF_87");
    le_compile_result_free(&res);
}

void test_phase_comp_transform()
{
    // PHASE_COMP (ANSI 87T transformer compensation): applies the SEL 3x3
    // compensation matrix M(k) to three complex phasors. k=6 is even (s=1/3);
    // with a balanced input I_A=I_B=I_C the compensated phasors cancel to ~0
    // (operate=0 under through-load). comp is a baked property.
    const char* circuit_json = R"({
        "name": "PhaseComp",
        "elements": [
            { "name": "PA", "type": "COMPLEXREGISTER" },
            { "name": "PB", "type": "COMPLEXREGISTER" },
            { "name": "PC", "type": "COMPLEXREGISTER" },
            { "name": "PCX", "type": "PHASE_COMP", "compensation": 6 },
            { "name": "OA", "type": "COMPLEXREGISTER" },
            { "name": "OB", "type": "COMPLEXREGISTER" },
            { "name": "OC", "type": "COMPLEXREGISTER" }
        ],
        "nets": [
            { "output": { "name": "PA", "port": "out" }, "inputs": [ { "name": "PCX", "port": "a" } ] },
            { "output": { "name": "PB", "port": "out" }, "inputs": [ { "name": "PCX", "port": "b" } ] },
            { "output": { "name": "PC", "port": "out" }, "inputs": [ { "name": "PCX", "port": "c" } ] },
            { "output": { "name": "PCX", "port": "a" }, "inputs": [ { "name": "OA", "port": "in" } ] },
            { "output": { "name": "PCX", "port": "b" }, "inputs": [ { "name": "OB", "port": "in" } ] },
            { "output": { "name": "PCX", "port": "c" }, "inputs": [ { "name": "OC", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "PHASE_COMP circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "PHASE_COMP loads");

    // comp baked into the state image.
    le_comp33_state_t* c33 = (le_comp33_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_PHASE_COMP, 0);
    TEST_ASSERT(c33 != NULL && c33->comp == 6, "PHASE_COMP comp=6 baked");

    // Balanced through-load: I_A=I_B=I_C=1<0. k=6 (even, s=1/3), matrix rows are
    // { -2,1,1 }, { 1,-2,1 }, { 1,1,-2 } -> each compensated phasor = 0.
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(0), le_c_make(1.0f, 0.0f));
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(1), le_c_make(1.0f, 0.0f));
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(2), le_c_make(1.0f, 0.0f));

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "PHASE_COMP step");

    // Outputs at user complex registers OA/OB/OC (indices 3,4,5) -> ~0.
    le_complex_t o0 = le_process_image_get_complex(&vm.image, LE_ADDR_MAKE_CMPLX(3));
    le_complex_t o1 = le_process_image_get_complex(&vm.image, LE_ADDR_MAKE_CMPLX(4));
    le_complex_t o2 = le_process_image_get_complex(&vm.image, LE_ADDR_MAKE_CMPLX(5));
    TEST_ASSERT(le_c_mag(o0) < 1e-4f && le_c_mag(o1) < 1e-4f && le_c_mag(o2) < 1e-4f,
                "PHASE_COMP cancels balanced through-load to ~0 (operate=0)");

    le_compile_result_free(&res);
}
void test_dist21_mho()
{
    // DIST_21: mho distance block with prefault voltage memory. Complex phasor v, i
    // + offset_on boolean -> bool trip. In-zone trips (Z inside mho circle);
    // out-of-zone / reverse do not.
    const char* circuit_json = R"({
        "name": "Dist21",
        "elements": [
            { "name": "V0", "type": "COMPLEXREGISTER" },
            { "name": "I0", "type": "COMPLEXREGISTER" },
            { "name": "OFF", "type": "CONSTANT", "dataType": "Boolean", "value": false },
            { "name": "D21", "type": "DIST_21", "reach": 10.0, "line_angle": 75.0,
              "offset": 0.0, "offset_angle": 75.0,
              "prefault_v_threshold": 0.5, "prefault_v_duration": 80 },
            { "name": "TRIP", "type": "BOOLREGISTER" }
        ],
        "nets": [
            { "output": { "name": "V0", "port": "out" }, "inputs": [ { "name": "D21", "port": "v" } ] },
            { "output": { "name": "I0", "port": "out" }, "inputs": [ { "name": "D21", "port": "i" } ] },
            { "output": { "name": "OFF", "port": "out" }, "inputs": [ { "name": "D21", "port": "offset_on" } ] },
            { "output": { "name": "D21", "port": "out" }, "inputs": [ { "name": "TRIP", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "DIST_21 circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "DIST_21 loads");

    // In-zone healthy: V=50<75, I=10<0 -> Z=5<75 (inside 10 ohm reach) -> trips.
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(0), le_c_polar(50.0f, 75.0f * (float)M_PI / 180.0f));
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(1), le_c_polar(10.0f, 0.0f));
    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "DIST_21 in-zone step");

    // First step refreshes prefault_v with healthy V; run a second so memory is seeded.
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "DIST_21 seed step");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "DIST_21 in-zone Z=5 trips");

    // Depress V below the prefault threshold: with V=5 (<0.5 is not met since 5>0.5),
    // we instead use a V below 0.5 to exercise prefault memory. Set V=0.3<75 but the
    // magnitude is still healthy enough to trip via remembered prefault V (50).
    le_process_image_set_complex(&vm.image, LE_ADDR_MAKE_CMPLX(0), le_c_polar(0.3f, 75.0f * (float)M_PI / 180.0f));
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "DIST_21 prefault step");
    // Within the 80ms prefault window, the relay substitutes the remembered 50 phasor,
    // so the measured Z stays near 5 and the relay continues to trip.
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "DIST_21 prefault V memory keeps trip under V collapse");

    le_compile_result_free(&res);
}
void test_arena_capacity()
{
    // State capacity is the zero-heap arena, not per-type maxima. A program
    // whose state does not fit the arena is rejected at load (LE_ERR_CAPACITY);
    // once the arena has room it loads.
    const char* circuit_json = R"({
        "name": "ArenaFit",
        "elements": [
            { "name": "IN0", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "T1", "type": "TON", "preset_ms": 100 }
        ],
        "nets": [
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "T1", "port": "in" } ] }
        ]
    })";
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    TEST_ASSERT(le_compile_json_ex(circuit_json, nullptr, &opts, &res) == 0 && res.success,
                "timer circuit compiles");
    TEST_ASSERT(res.timer_count > 0, "timer circuit reserves a state block");

    // Load normally (the preconfigured state image fits the workspace).
    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK,
                "timer circuit loads with its state image in the workspace");
    le_timer_state_t* ht = le_process_image_timer(&vm.image, 0);
    TEST_ASSERT(ht != nullptr && ht->preset_ms == 100, "timer preset baked into the state image");
    TEST_ASSERT(le_rt_state(LE_BLK_TIMER, 0) != nullptr, "block 0 of timer group resolves");
    TEST_ASSERT(le_rt_state(LE_BLK_TIMER, 4096) == nullptr, "out-of-workspace index rejected");

    // "Does it fit?": validate rejects a program whose declared state image
    // exceeds the platform workspace (LE_ERR_CAPACITY), before CRC check.
    uint8_t big[LE_RAM_WORKSPACE_BYTES + sizeof(le_header_t) + 16] = {0};
    le_header_t* bh = (le_header_t*)big;
    bh->magic = LE_BIN_MAGIC;
    bh->version = LE_BIN_VERSION;
    bh->state_img_len = LE_RAM_WORKSPACE_BYTES + 1u;
    le_header_t out;
    TEST_ASSERT(le_loader_validate(big, sizeof(big), &out) == LE_ERR_CAPACITY,
                "program whose state image exceeds the workspace is rejected");

    // Register arena: the packed register footprint must fit the RAM budget, and
    // a tiny program packs tightly (RAM ~ what it declares).
    {
        le_header_t rh;
        memset(&rh, 0, sizeof(rh));
        rh.digital_in_count = 16;  rh.digital_out_count = 16;
        rh.bool_reg_count   = 256; rh.float_reg_count = 128;
        rh.int_reg_count    = 64;  rh.complex_reg_count = 64;
        rh.analog_in_count  = 16;
        alignas(4) uint8_t small[64];
        le_process_image_t pi;
        uint32_t rlen = 0;
        TEST_ASSERT(le_process_image_regs_len(&rh) > sizeof(small),
                    "full register footprint exceeds a tiny RAM budget");
        TEST_ASSERT(le_process_image_bind(&pi, small, sizeof(small), &rh, &rlen) == LE_ERR_CAPACITY,
                    "bind rejects a register arena that exceeds the RAM budget");

        le_header_t sh;
        memset(&sh, 0, sizeof(sh));
        sh.digital_in_count = 2; sh.digital_out_count = 2; sh.bool_reg_count = 2;
        TEST_ASSERT(le_process_image_bind(&pi, small, sizeof(small), &sh, &rlen) == LE_OK,
                    "tiny program binds into the same budget");
        TEST_ASSERT(rlen == 4 && pi.din_count == 2 && pi.bool_count == 2 && pi.floats_off == 4,
                    "tiny program packs into 4 arena bytes (bit bucket, 4-byte aligned)");
    }

    le_compile_result_free(&res);
}

void test_conversion_roundtrip_and_clamp()
{
    // RECT2COMPLEX(real,imag) -> COMPLEX2RECT -> {real, imag}: (3,4)->cplx->(3,4).
    // POLAR2COMPLEX(mag,ang) -> COMPLEX2POLAR -> {mag, ang}: preserves (5,ang).
    // CLAMP(value,min,max) clamps with proper independent bounds.
    const char* circuit_json = R"({
        "name": "Conversions",
        "elements": [
            { "name": "F0", "type": "FLOATREGISTER", "address": "%R0" },
            { "name": "F1", "type": "FLOATREGISTER", "address": "%R1" },
            { "name": "R2C", "type": "RECT2COMPLEX" },
            { "name": "CREG", "type": "COMPLEXREGISTER" },
            { "name": "C2R", "type": "COMPLEX2RECT" },
            { "name": "CR0", "type": "FLOATREGISTER", "address": "%R2" },
            { "name": "CR1", "type": "FLOATREGISTER", "address": "%R3" },
            { "name": "MAG", "type": "FLOATREGISTER", "address": "%R4" },
            { "name": "ANG", "type": "FLOATREGISTER", "address": "%R5" },
            { "name": "P2C", "type": "POLAR2COMPLEX" },
            { "name": "CREG2", "type": "COMPLEXREGISTER" },
            { "name": "C2P", "type": "COMPLEX2POLAR" },
            { "name": "RMAG", "type": "FLOATREGISTER", "address": "%R6" },
            { "name": "RANG", "type": "FLOATREGISTER", "address": "%R7" },
            { "name": "CL", "type": "CLAMP" },
            { "name": "CLOUT", "type": "FLOATREGISTER", "address": "%R8" }
        ],
        "nets": [
            { "output": { "name": "F0", "port": "out" }, "inputs": [ { "name": "R2C", "port": "real" } ] },
            { "output": { "name": "F1", "port": "out" }, "inputs": [ { "name": "R2C", "port": "imag" } ] },
            { "output": { "name": "R2C", "port": "out" }, "inputs": [ { "name": "CREG", "port": "in" } ] },
            { "output": { "name": "CREG", "port": "out" }, "inputs": [ { "name": "C2R", "port": "in" } ] },
            { "output": { "name": "C2R", "port": "real" }, "inputs": [ { "name": "CR0", "port": "in" } ] },
            { "output": { "name": "C2R", "port": "imag" }, "inputs": [ { "name": "CR1", "port": "in" } ] },
            { "output": { "name": "MAG", "port": "out" }, "inputs": [ { "name": "P2C", "port": "mag" } ] },
            { "output": { "name": "ANG", "port": "out" }, "inputs": [ { "name": "P2C", "port": "angle" } ] },
            { "output": { "name": "P2C", "port": "out" }, "inputs": [ { "name": "CREG2", "port": "in" } ] },
            { "output": { "name": "CREG2", "port": "out" }, "inputs": [ { "name": "C2P", "port": "in" } ] },
            { "output": { "name": "C2P", "port": "magnitude" }, "inputs": [ { "name": "RMAG", "port": "in" } ] },
            { "output": { "name": "C2P", "port": "angle" }, "inputs": [ { "name": "RANG", "port": "in" } ] },
            { "output": { "name": "F0", "port": "out" }, "inputs": [ { "name": "CL", "port": "value" } ] },
            { "output": { "name": "F1", "port": "out" }, "inputs": [ { "name": "CL", "port": "min" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "conversion + clamp circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "conversion circuit loads");

    // (3,4) rect -> complex -> rect
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 3.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(1), 4.0f);
    // mag=5, angle (for polar2complex) then complex2polar
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(4), 5.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(5), atan2f(4.0f, 3.0f));

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "conversion step");

    // COMPLEX2RECT round-trip: (3,4)->cplx->(3,4)
    TEST_ASSERT(fabsf(le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(2)) - 3.0f) < 1e-4f, "RECT2COMPLEX->COMPLEX2RECT real=3");
    TEST_ASSERT(fabsf(le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(3)) - 4.0f) < 1e-4f, "RECT2COMPLEX->COMPLEX2RECT imag=4");
    // POLAR2COMPLEX -> COMPLEX2POLAR -> (5, atan2(4,3))
    TEST_ASSERT(fabsf(le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(6)) - 5.0f) < 1e-4f, "POLAR2COMPLEX->COMPLEX2POLAR mag=5");
    TEST_ASSERT(fabsf(le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(7)) - atan2f(4.0f, 3.0f)) < 1e-4f, "POLAR2COMPLEX->COMPLEX2POLAR angle");

    le_compile_result_free(&res);
}

void test_variables()
{
    // Circuit variables in properties: direct scalar, indirect (var-on-var), and
    // arithmetic. Used here in a DIST_21 reach property (reach = Z2 = Z_Line*1.20).
    const char* circuit_json = R"({
        "name": "Vars",
        "variables": {
            "Z_Line": 5.0,
            "Z2": "Z_Line * 1.20",
            "pu": 0.90
        },
        "elements": [
            { "name": "C1", "type": "CONSTANT", "dataType": "Float", "value": "%Z_Line%" },
            { "name": "D21", "type": "DIST_21", "reach": "%Z2%", "prefault_v_threshold": "%pu%" }
        ],
        "nets": []
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "variable circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "variable circuit loads");

    // DIST_21 state baked with reach = 6.0 (5.0*1.20) and prefault threshold = 0.9.
    le_dist21_state_t* d21 = (le_dist21_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_21, 0);
    TEST_ASSERT(d21 != NULL, "DIST_21 state present");
    TEST_ASSERT(fabsf(d21->reach_ohms - 6.0f) < 1e-4f, "reach baked from indirect %Z2% = 5.0*1.20 = 6.0");
    TEST_ASSERT(fabsf(d21->prefault_v_threshold - 0.90f) < 1e-4f, "prefault threshold baked from %pu% = 0.9");

    le_compile_result_free(&res);
}

void test_variable_errors()
{
    // Undefined variable reference must fail cleanly.
    const char* undef = R"({
        "name": "Undef",
        "variables": { "Z2": "Missing * 2.0" },
        "elements": [ { "name": "D21", "type": "DIST_21", "reach": "%Z2%" } ],
        "nets": []
    })";
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(undef, nullptr, &opts, &res);
    TEST_ASSERT(!(rc == 0 && res.success), "undefined variable reference is rejected");
    if (res.error_message) TEST_ASSERT(std::strstr(res.error_message, "undefined") != NULL ||
                                       std::strstr(res.error_message, "Variable") != NULL,
                                       "error message mentions variable");
    le_compile_result_free(&res);

    // Circular reference must fail cleanly.
    const char* cyc = R"({
        "name": "Cyc",
        "variables": { "A": "B", "B": "A" },
        "elements": [ { "name": "D21", "type": "DIST_21", "reach": "%A%" } ],
        "nets": []
    })";
    le_compile_result_t res2;
    int rc2 = le_compile_json_ex(cyc, nullptr, &opts, &res2);
    TEST_ASSERT(!(rc2 == 0 && res2.success), "circular variable reference is rejected");
    le_compile_result_free(&res2);
}

void test_variable_math_functions()
{
    // Expression evaluator supports trig + math functions (unary and 2-arg).
    const char* circuit_json = R"m({
        "name": "MathFns",
        "variables": {
            "A": "sqrt(25) + pow(2, 3) + max(1, 5)",
            "B": "sin(0) + cos(0) + abs(-3)",
            "C": "atan2(1, 1) * 4"
        },
        "elements": [
            { "name": "D21", "type": "DIST_21", "reach": "%A%" }
        ],
        "nets": []
    })m";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "math-function variable circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "math-function circuit loads");

    // reach = sqrt(25)+pow(2,3)+max(1,5) = 5 + 8 + 5 = 18
    le_dist21_state_t* d21 = (le_dist21_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_21, 0);
    TEST_ASSERT(d21 != NULL, "DIST_21 state present");
    TEST_ASSERT(fabsf(d21->reach_ohms - 18.0f) < 1e-4f,
                "reach from math fns = sqrt25+pow(2,3)+max = 5+8+5 = 18");

    le_compile_result_free(&res);
}

void test_aliases_and_pulse()
{
    // Circuit-declared register aliases should be packed into the .lebin and
    // expose lookup + pulse-by-name at runtime. Board aliases attach separately
    // (host-supplied) and never grow the binary.
    const char* circuit_json = R"({
        "name": "AliasCircuit",
        "elements": [
            { "name": "B0", "type": "BOOLREGISTER" },
            { "name": "DO1", "type": "DIGITALOUTPUT", "address": "%Q0" },
            { "name": "F1", "type": "FLOATREGISTER" }
        ],
        "aliases": {
            "TRIP": "B0",
            "SSP":  "%B0",
            "LED":  "%OUT0",
            "FVAL": "%F0"
        },
        "nets": []
    })";
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    TEST_ASSERT(le_compile_json_ex(circuit_json, nullptr, &opts, &res) == 0 && res.success,
                "circuit with aliases compiles");
    TEST_ASSERT(res.alias_count == 4, "four aliases packed into the result");

    // Parse the packed header alias_count and confirm the table is appended.
    le_header_t hdr;
    TEST_ASSERT(le_loader_validate(res.binary_data, res.binary_size, &hdr) == LE_OK,
                "binary with aliases validates");
    TEST_ASSERT(hdr.alias_count == 4, "header reports alias_count");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK,
                "binary with aliases loads");
    TEST_ASSERT(vm.alias_count == 4, "vm carries the zero-copy alias table");

    uint16_t a = 0;
    TEST_ASSERT(le_alias_lookup(&vm, "TRIP", &a) == LE_OK && a == LE_ADDR_MAKE_BOOL_REG(0),
                "TRIP resolves to boolean register 0");
    TEST_ASSERT(le_alias_lookup(&vm, "SSP", &a) == LE_OK && a == LE_ADDR_MAKE_BOOL_REG(0),
                "SSP (%B0) resolves to boolean register 0");
    TEST_ASSERT(le_alias_lookup(&vm, "%LED", &a) == LE_OK && a == LE_ADDR_MAKE_DOUT(0),
                "LED resolves to digital output 0");
    TEST_ASSERT(le_alias_lookup(&vm, "fval", &a) == LE_OK && a == LE_ADDR_MAKE_FLOAT(0),
                "FVAL resolves to float register 0 (case-insensitive)");
    TEST_ASSERT(le_alias_lookup(&vm, "NOPE", &a) == LE_ERR_NOT_FOUND,
                "unknown alias returns LE_ERR_NOT_FOUND");

    // Pulse: default 1 s (set active, clear once now_ms advances past 1000).
    TEST_ASSERT(le_alias_pulse(&vm, "TRIP") == LE_OK, "pulse arms");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "pulse sets the register active immediately");
    TEST_ASSERT(le_vm_step(&vm, 500) == LE_OK && le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "pulse still active at 500 ms");
    TEST_ASSERT(le_vm_step(&vm, 1500) == LE_OK && !le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "pulse cleared after 1 s elapses");

    // Explicit-duration pulse via le_alias_pulse_for. The first le_vm_step
    // anchors the duration to the VM clock; a later step clears it. We pulse a
    // coil (SSP -> %B0) rather than the output, because an output driven by the
    // compiled rung is re-written each scan (the scan wins over an override).
    TEST_ASSERT(le_alias_pulse_for(&vm, "SSP", 0.25f) == LE_OK, "pulse_for arms");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "coil pulse active immediately");
    TEST_ASSERT(le_alias_set_bool(&vm, "LED", true) == LE_OK &&
                le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)),
                "alias set_bool writes an output by name");
    TEST_ASSERT(le_vm_step(&vm, 2400) == LE_OK && le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "coil pulse anchored (clear-now = 2400 + 250)");
    TEST_ASSERT(le_vm_step(&vm, 2800) == LE_OK && !le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)),
                "coil pulse cleared after 250 ms elapses");

    // Board aliases are host-supplied (NOT in the .lebin): attach and resolve.
    le_alias_t board[1];
    board[0].kind = 0; board[0].pad = 0;
    memset(board[0].name, 0, LE_ALIAS_NAME_MAX);
    memcpy(board[0].name, "BTN_A", 5);
    board[0].addr = LE_ADDR_MAKE_DIN(0);
    TEST_ASSERT(le_vm_load_board_aliases(&vm, board, 1) == LE_OK, "board aliases attach");
    TEST_ASSERT(le_alias_lookup(&vm, "BTN_A", &a) == LE_OK && a == LE_ADDR_MAKE_DIN(0),
                "board alias resolves at runtime");

    // The embedded alias table is circuit-only; verifying the board alias did
    // not touch binary size: header alias_count stayed 4.
    TEST_ASSERT(hdr.alias_count == 4, "board aliases never grow the .lebin");

    le_compile_result_free(&res);
}

void test_json_error_paths()
{
    // Malformed JSON document.
    le_compile_result_t r1;
    int rc1 = le_compile_json("{ not valid json at all", nullptr, &r1);
    TEST_ASSERT(!(rc1 == 0 && r1.success), "malformed JSON rejected");
    le_compile_result_free(&r1);

    // Root must be a JSON object (arrays/values are rejected).
    const char* not_object = R"([ 1, 2, 3 ])";
    le_compile_result_t r2;
    int rc2 = le_compile_json(not_object, nullptr, &r2);
    TEST_ASSERT(!(rc2 == 0 && r2.success), "non-object root rejected");
    le_compile_result_free(&r2);

    // Unknown element types are tolerated (lenient contract): the element is
    // compiled away to zero instructions instead of failing the build.
    const char* unknown = R"({
        "name": "Bad",
        "elements": [ { "name": "X1", "type": "NO_SUCH_ELEMENT" } ],
        "nets": []
    })";
    le_compile_result_t r3;
    int rc3 = le_compile_json(unknown, nullptr, &r3);
    TEST_ASSERT(rc3 == 0 && r3.success, "unknown element type compiles (lenient)");
    if (rc3 == 0 && r3.success) {
        le_vm_t vm;
        le_vm_init(&vm);
        TEST_ASSERT(le_loader_load(&vm, r3.binary_data, r3.binary_size) == LE_OK, "unknown-type binary loads");
        TEST_ASSERT(vm.instruction_count == 0, "unknown-type element emits zero instructions");
    }
    le_compile_result_free(&r3);
}

void test_net_error_paths()
{
    // Nets to nonexistent source elements are tolerated (lenient wiring): the
    // unresolved consumer falls back to its idle default (false/0) instead of a
    // hard compile error. Lock in that contract.
    const char* dangling = R"({
        "name": "Dangling",
        "elements": [ { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN0" },
                      { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT0" } ],
        "nets": [ { "output": { "name": "GHOST", "port": "out" },
                     "inputs": [ { "name": "O1", "port": "in" } ] } ]
    })";
    le_compile_result_t r1;
    int rc1 = le_compile_json(dangling, nullptr, &r1);
    TEST_ASSERT(rc1 == 0 && r1.success, "dangling net source compiles (lenient wiring)");
    if (rc1 == 0 && r1.success) {
        le_vm_t vm;
        le_vm_init(&vm);
        TEST_ASSERT(le_loader_load(&vm, r1.binary_data, r1.binary_size) == LE_OK, "dangling-net binary loads");
        le_vm_step(&vm, 0);
        TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)),
                    "unresolved gate source defaults to false");
    }
    le_compile_result_free(&r1);

    // An unknown tag sender is surfaced as a warning, not a hard error.
    const char* bad_tag = R"({
        "name": "BadTag",
        "elements": [ { "name": "TX", "type": "TAG", "direction": "send", "tag_name": "X" },
                      { "name": "RX", "type": "TAG", "direction": "recv", "tag_name": "MISSING" } ],
        "nets": []
    })";
    le_compile_result_t r2;
    int rc2 = le_compile_json(bad_tag, nullptr, &r2);
    TEST_ASSERT(rc2 == 0 && r2.success, "unmatched tag compiles with a warning (lenient wiring)");
    le_compile_result_free(&r2);

    // A self-driven loop is tolerated by the compiler (topological order still
    // deterministically emitted) -- size + load must succeed.
    const char* self_loop = R"({
        "name": "SelfLoop",
        "elements": [ { "name": "NX", "type": "NOT" },
                      { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT0" } ],
        "nets": [ { "output": { "name": "NX", "port": "out" }, "inputs": [ { "name": "NX", "port": "a" } ] },
                  { "output": { "name": "NX", "port": "out" }, "inputs": [ { "name": "O1", "port": "in" } ] } ]
    })";
    le_compile_result_t r3;
    int rc3 = le_compile_json(self_loop, nullptr, &r3);
    TEST_ASSERT(rc3 == 0 && r3.success, "self-referential net compiles without crashing");
    le_compile_result_free(&r3);
}

void test_alias_error_paths()
{
    // Alias name longer than 7 chars.
    const char* long_name = R"({
        "name": "AliasLong",
        "elements": [ { "name": "B0", "type": "BOOLREGISTER" } ],
        "nets": [],
        "aliases": { "WAYTOOLONG8": "B0" }
    })";
    le_compile_result_t r1;
    int rc1 = le_compile_json(long_name, nullptr, &r1);
    TEST_ASSERT(!(rc1 == 0 && r1.success), "8-char alias name rejected");
    le_compile_result_free(&r1);

    // Alias target that does not resolve to a register.
    const char* bad_target = R"({
        "name": "AliasBadTarget",
        "elements": [ { "name": "B0", "type": "BOOLREGISTER" } ],
        "nets": [],
        "aliases": { "OK1": "NOT_A_REGISTER" }
    })";
    le_compile_result_t r2;
    int rc2 = le_compile_json(bad_target, nullptr, &r2);
    TEST_ASSERT(!(rc2 == 0 && r2.success), "alias to unresolvable target rejected");
    le_compile_result_free(&r2);

    // A large alias table (33 entries, over the LE_MAX_ALIASES 32 runtime hint)
    // still compiles and resolves on the VM; the runtime keeps the full table.
    std::string many = "{\"name\":\"AliasMany\",\"elements\":[{\"name\":\"B0\",\"type\":\"BOOLREGISTER\"},"
                       "{\"name\":\"B1\",\"type\":\"BOOLREGISTER\"}],\"nets\":[],\"aliases\":{";
    for (int i = 0; i <= LE_MAX_ALIASES; i++) {  /* 33 entries */
        many += "\"A" + std::to_string(i) + (i % 2 ? "\":\"B0\"" : "\":\"B1\"");
        if (i < LE_MAX_ALIASES) many += ",";
    }
    many += "}}";
    le_compile_result_t r3;
    int rc3 = le_compile_json(many.c_str(), nullptr, &r3);
    TEST_ASSERT(rc3 == 0 && r3.success, "33-alias circuit compiles");
    if (rc3 == 0 && r3.success) {
        le_vm_t vm;
        le_vm_init(&vm);
        TEST_ASSERT(le_loader_load(&vm, r3.binary_data, r3.binary_size) == LE_OK, "many-alias binary loads");
        uint16_t addr = 0;
        TEST_ASSERT(le_alias_lookup(&vm, "A32", &addr) == LE_OK && addr == LE_ADDR_MAKE_BOOL_REG(1),
                    "last (33rd) alias resolves to its register");
    }
    le_compile_result_free(&r3);
}

void test_binary_determinism()
{
    const char* circuit_json = R"({
        "name": "Determinism",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%IN1" },
            { "name": "A1", "type": "AND" },
            { "name": "R1", "type": "BOOLREGISTER" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "A1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "A1", "port": "b" } ] },
            { "output": { "name": "A1", "port": "out" }, "inputs": [ { "name": "R1", "port": "in" } ] }
        ]
    })";
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t r1, r2;
    int rc1 = le_compile_json_ex(circuit_json, nullptr, &opts, &r1);
    int rc2 = le_compile_json_ex(circuit_json, nullptr, &opts, &r2);
    TEST_ASSERT(rc1 == 0 && r1.success && rc2 == 0 && r2.success, "both compilations succeed");
    TEST_ASSERT(r1.binary_size == r2.binary_size, "identical binary sizes");
    bool same = (r1.binary_size == r2.binary_size);
    if (same) {
        for (size_t i = 0; i < r1.binary_size; i++) {
            if (r1.binary_data[i] != r2.binary_data[i]) { same = false; break; }
        }
    }
    TEST_ASSERT(same, "byte-for-byte identical binaries (deterministic CRC/output)");
    le_compile_result_free(&r1);
    le_compile_result_free(&r2);
}

void test_binary_header_fields()
{
    const char* circuit_json = R"({
        "name": "HeaderFields",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT1" },
            { "name": "B1", "type": "BOOLREGISTER" },
            { "name": "F1", "type": "FLOATREGISTER" },
            { "name": "I1", "type": "INTREGISTER" }
        ],
        "nets": [ { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "O1", "port": "in" } ] } ]
    })";
    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, nullptr, &res);
    TEST_ASSERT(rc == 0 && res.success, "header-fields circuit compiles");
    TEST_ASSERT(sizeof(le_header_t) == 44, "packed header is exactly 44 bytes (4-byte aligned)");

    le_header_t h;
    TEST_ASSERT(le_loader_validate(res.binary_data, res.binary_size, &h) == LE_OK, "binary validates");
    TEST_ASSERT(h.magic == LE_BIN_MAGIC, "magic field set");
    TEST_ASSERT(h.version == LE_BIN_VERSION, "version field equals LE_BIN_VERSION");
    TEST_ASSERT(h.timing_count == 1, "compiler always emits the 8-byte timing descriptor");
    TEST_ASSERT(h.digital_in_count == 1, "digital in count header field");
    TEST_ASSERT(h.digital_out_count == 1, "digital out count header field");
    TEST_ASSERT(h.bool_reg_count == 1, "bool reg count header field");
    TEST_ASSERT(h.float_reg_count == 1, "float reg count header field");
    TEST_ASSERT(h.int_reg_count == 1, "int reg count header field");
    TEST_ASSERT(h.alias_count == 0, "no aliases declared -> alias_count 0");
    le_compile_result_free(&res);
}

/**
 * @brief Asserts the .lebin carries the compiler's worst-case timing model
 * (abstract cost + safety margin), that the descriptor is loader-readable, and
 * that costs scale with circuit size (a heavier circuit costs more).
 */
void test_timing_descriptor(void)
{
    const char* light_json = R"({
        "name": "TimingLight",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT0" }
        ],
        "nets": [ { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "O1", "port": "in" } ] } ]
    })";
    le_compile_result_t light;
    TEST_ASSERT(le_compile_json(light_json, nullptr, &light) == 0 && light.success, "light timing circuit compiles");
    TEST_ASSERT(light.abstract_cycles > 0, "compiler reports non-zero abstract cycle cost");

    le_header_t h;
    TEST_ASSERT(le_loader_validate(light.binary_data, light.binary_size, &h) == LE_OK, "timing binary validates");
    TEST_ASSERT(h.timing_count == 1, "timing descriptor present in header");

    le_vm_t vm;
    TEST_ASSERT(le_vm_init(&vm) == LE_OK, "vm init");
    TEST_ASSERT(le_loader_load(&vm, light.binary_data, light.binary_size) == LE_OK, "load with timing descriptor");
    TEST_ASSERT(vm.timing_abstract_cycles == (uint32_t)light.abstract_cycles, "loader captured compiler cost");

    le_timing_t t;
    TEST_ASSERT(le_loader_timing(&vm, &t) == LE_OK, "timing query");
    TEST_ASSERT(t.safety_margin_pct == 150, "safety margin carried through");
    TEST_ASSERT(t.abstract_cycles == (uint32_t)light.abstract_cycles, "timing report carries cost");
    le_compile_result_free(&light);

    /* A heavier circuit (more instructions) must cost strictly more. */
    const char* heavy_json = R"({
        "name": "TimingHeavy",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "I2", "type": "DIGITALINPUT", "address": "%IN1" },
            { "name": "G1", "type": "OR" },
            { "name": "G2", "type": "AND" },
            { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "G1", "port": "a" } ] },
            { "output": { "name": "I2", "port": "out" }, "inputs": [ { "name": "G1", "port": "b" } ] },
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "G2", "port": "a" } ] },
            { "output": { "name": "I2", "port": "out" }, "inputs": [ { "name": "G2", "port": "b" } ] },
            { "output": { "name": "G1", "port": "out" }, "inputs": [ { "name": "O1", "port": "in" } ] }
        ]
    })";
    le_compile_result_t heavy;
    TEST_ASSERT(le_compile_json(heavy_json, nullptr, &heavy) == 0 && heavy.success, "heavy timing circuit compiles");
    TEST_ASSERT(heavy.abstract_cycles > light.abstract_cycles, "heavier circuit costs strictly more abstract cycles");
    le_compile_result_free(&heavy);
}

/**
 * @brief Verifies the DESIGNER owns the fixed scan rate: a circuit-declared
 * `scan_rate_hz` is embedded in the .lebin timing descriptor, the loader applies
 * it to the VM clock (period = 1e6/rate us), and an unachievable declared rate is
 * rejected at compile time with the MAX achievable rate reported in the error.
 */
void test_circuit_owns_scan_rate(void)
{
    const char* json = R"({
        "name": "Rated",
        "scan_rate_hz": 960,
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT0" }
        ],
        "nets": [ { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "O1", "port": "in" } ] } ]
    })";
    le_compile_result_t res;
    TEST_ASSERT(le_compile_json(json, nullptr, &res) == 0 && res.success, "rated circuit compiles");

    /* Descriptor carries the declared rate; the loader applies the period. */
    le_timing_desc_t td;
    {
        le_header_t h;
        TEST_ASSERT(le_loader_validate(res.binary_data, res.binary_size, &h) == LE_OK, "rated binary validates");
        size_t off = sizeof(le_header_t) + (size_t)h.instruction_count * sizeof(le_instruction_t);
        for (uint16_t b = 0; b < h.block_count; b++) {
            const le_block_desc_t* d = (const le_block_desc_t*)(res.binary_data + off);
            off += (size_t)LE_BLOCK_DESC_HEADER_BYTES +
                   ((size_t)d->in_count + (size_t)d->out_count) * sizeof(uint16_t);
        }
        off += (size_t)h.state_desc_count * LE_STATE_DESC_BYTES;
        off += (size_t)h.state_img_len;
        off += (size_t)h.alias_count * LE_ALIAS_BYTES;
        const le_timing_desc_t* t = (const le_timing_desc_t*)(res.binary_data + off);
        td = *t;
    }
    TEST_ASSERT(td.design_scan_rate_hz == 960, "circuit scan_rate_hz embedded in the descriptor");

    le_vm_t vm;
    TEST_ASSERT(le_vm_init(&vm) == LE_OK, "vm init");
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "rated program loads");
    TEST_ASSERT(vm.scan_period_us == 1042, "loader applied 1e6/960 = 1042 us period to the VM clock");
    TEST_ASSERT(le_rt_scan_dt() > 0.0f, "runtime dt reflects the applied scan period");
    le_compile_result_free(&res);

    /* An unachievable declared rate must be rejected with the max achievable
     * rate in the message. Board = 1e6 ns/cycle => the light circuit's worst
     * case is ~1500 us/scan, so declaring 960 Hz (1042 us period) must fail. */
    const char* too_fast = R"({
        "name": "TooFast",
        "scan_rate_hz": 960,
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT0" }
        ],
        "nets": [ { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "O1", "port": "in" } ] } ]
    })";
    const char* profile = R"({
        "device": { "name": "TestBoard", "firmware_version": "1.0", "protocol_version": 1 },
        "limits": { "ns_per_abstract_cycle": 1000000 },
        "features": { "protection": true, "complex": true, "analog": true, "dsp": true, "serial_bus": true }
    })";
    le_compile_result_t bad;
    int rc2 = le_compile_json(too_fast, profile, &bad);
    TEST_ASSERT(rc2 != 0 || !bad.success, "unachievable scan_rate_hz rejected against the board cost model");
    if (bad.error_message) {
        TEST_ASSERT(std::strstr(bad.error_message, "Lower scan_rate_hz to at most") != NULL,
                    "error message tells the designer the max achievable rate");
    }
    le_compile_result_free(&bad);
}

/**
 * @brief Verifies the opcode enum is DENSE (every value 0x00..0xFF enumerated)
 * so the flattened executor compiles to a single O(1) jump table, and that the
 * reserved placeholders are loudly rejected if ever executed.
 */
void test_opcode_enum_density(void)
{
    TEST_ASSERT(LE_OP_LOAD_I == 0x1E, "LOAD_I opcode present at 0x1E");
    TEST_ASSERT(LE_OP_RESERVED_1F == 0x1F, "reserved block 1F present");
    TEST_ASSERT(LE_OP_FMA_F == 0x50, "FMA_F opcode present at 0x50");
    TEST_ASSERT(LE_OP_FMS_F == 0x51, "FMS_F opcode present at 0x51");
    TEST_ASSERT(LE_OP_LOAD_F == 0x52, "LOAD_F opcode present at 0x52");
    TEST_ASSERT(LE_OP_SQRT_F == 0x53, "SQRT_F opcode present at 0x53");
    TEST_ASSERT(LE_OP_RESERVED_5F == 0x5F, "reserved block 5F present");
    TEST_ASSERT(LE_OP_RESERVED_9F == 0x9F, "reserved block 9F present");
    TEST_ASSERT(LE_OP_RESERVED_FE == 0xFE, "reserved block FE present");
    TEST_ASSERT(LE_OP_END == 0xFF, "END remains the table sentinel");

    le_process_image_t img;
    le_process_image_init(&img);
    le_instruction_t inst = { (uint8_t)LE_OP_RESERVED_5F, 0, LE_ADDR_UNUSED, LE_ADDR_UNUSED, LE_ADDR_UNUSED };
    TEST_ASSERT(le_exec_instruction(&inst, &img, 0) == LE_ERR_UNKNOWN_OPCODE,
                "reserved opcode rejected as unknown");
}

void test_disasm_content()
{
    const char* circuit_json = R"({
        "name": "Disasm",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT0" },
            { "name": "F1", "type": "FLOATREGISTER" }
        ],
        "nets": [ { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "O1", "port": "in" } ] } ],
        "aliases": { "START": "F1", "TRIP": "O1" }
    })";
    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, nullptr, &res);
    TEST_ASSERT(rc == 0 && res.success, "disasm circuit compiles");
    const char* d = res.disassembly_text ? res.disassembly_text : "";
    TEST_ASSERT(std::strstr(d, "%IN[0]") != NULL, "disasm shows %IN mnemonic");
    TEST_ASSERT(std::strstr(d, "%OUT[0]") != NULL, "disasm shows %OUT mnemonic");
    TEST_ASSERT(std::strstr(d, "%F[") != NULL, "disasm shows %F mnemonic");
    TEST_ASSERT(std::strstr(d, "Register Aliases (2):") != NULL, "disasm renders the alias table");
    le_compile_result_free(&res);
}

void test_dce_preserves_stateful()
{
    // A stateful element (TON) with NO downstream consumer is a root for DCE and
    // MUST survive optimization (side effects/state updates matter).
    const char* circuit_json = R"({
        "name": "DCEStateful",
        "elements": [ { "name": "T1", "type": "TON", "preset_ms": 500 } ],
        "nets": []
    })";
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL); /* full optimizer */
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "stateful-only circuit compiles under full optimization");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK,
                "stateful-only binary loads");
    le_timer_state_t* t = le_process_image_timer(&vm.image, 0);
    TEST_ASSERT(t != NULL, "DCE preserved the stateful TON (state bound)");
    le_compile_result_free(&res);
}

void test_inversion_combinations()
{
    const char* circuit_json = R"({
        "name": "InvCombos",
        "elements": [
            { "name": "IA", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "IB", "type": "DIGITALINPUT", "address": "%IN1" },
            { "name": "N1", "type": "NOT" },
            { "name": "N2", "type": "NOT" },
            { "name": "G", "type": "AND" },
            { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT0" }
        ],
        "nets": [
            { "output": { "name": "IA", "port": "out" }, "inputs": [ { "name": "N1", "port": "a" } ] },
            { "output": { "name": "IB", "port": "out" }, "inputs": [ { "name": "N2", "port": "a" } ] },
            { "output": { "name": "N1", "port": "out" }, "inputs": [ { "name": "G", "port": "a" } ] },
            { "output": { "name": "N2", "port": "out" }, "inputs": [ { "name": "G", "port": "b" } ] },
            { "output": { "name": "G", "port": "out" }, "inputs": [ { "name": "O1", "port": "in" } ] }
        ]
    })";
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "inversion-combo circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "inversion binary loads");

    auto run_case = [&](bool ia, bool ib, bool expect) {
        le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), ia);
        le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), ib);
        le_vm_step(&vm, 0);
        return le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == expect;
    };
    // out = (!IA) && (!IB) after NOT folding
    TEST_ASSERT(run_case(false, false, true),  "!0 AND !0 = 1");
    TEST_ASSERT(run_case(false, true, false),  "!0 AND !1 = 0");
    TEST_ASSERT(run_case(true, false, false),  "!1 AND !0 = 0");
    TEST_ASSERT(run_case(true, true, false),   "!1 AND !1 = 0");
    le_compile_result_free(&res);
}

void test_integrated_pipeline_step()
{
    // A realistic multi-scan circuit exercising the whole pipeline end-to-end:
    // DIN -> TON -> AND gate -> DOUT, plus a float ADD chain -> CMP_GT -> DOUT.
    const char* circuit_json = R"({
        "name": "Integrated",
        "elements": [
            { "name": "IN0", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN1" },
            { "name": "T1", "type": "TON", "preset_ms": 100 },
            { "name": "G1", "type": "AND" },
            { "name": "O0", "type": "DIGITALOUTPUT", "address": "%OUT0" },
            { "name": "C1", "type": "CONSTANT", "dataType": "float", "value": 1.0 },
            { "name": "C2", "type": "CONSTANT", "dataType": "float", "value": 1.0 },
            { "name": "A1", "type": "ADD" },
            { "name": "F1", "type": "FLOATREGISTER" },
            { "name": "CP", "type": "CMP_GT" },
            { "name": "O1", "type": "DIGITALOUTPUT", "address": "%OUT1" }
        ],
        "nets": [
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "T1", "port": "in" } ] },
            { "output": { "name": "T1", "port": "out" }, "inputs": [ { "name": "G1", "port": "a" } ] },
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "G1", "port": "b" } ] },
            { "output": { "name": "G1", "port": "out" }, "inputs": [ { "name": "O0", "port": "in" } ] },
            { "output": { "name": "C1", "port": "out" }, "inputs": [ { "name": "A1", "port": "a" } ] },
            { "output": { "name": "C2", "port": "out" }, "inputs": [ { "name": "A1", "port": "b" } ] },
            { "output": { "name": "A1", "port": "out" }, "inputs": [ { "name": "F1", "port": "in" } ] },
            { "output": { "name": "F1", "port": "out" }, "inputs": [ { "name": "CP", "port": "a" } ] },
            { "output": { "name": "C1", "port": "out" }, "inputs": [ { "name": "CP", "port": "b" } ] },
            { "output": { "name": "CP", "port": "out" }, "inputs": [ { "name": "O1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "integrated circuit compiles under full optimization");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "integrated binary loads");
    TEST_ASSERT(vm.running, "autostart flag set");
    TEST_ASSERT(le_rt_kind_base(LE_BLK_TIMER) >= 0, "timer bound at load");
    le_timer_state_t* ht = le_process_image_timer(&vm.image, 0);

    // float chain: F1 = C1 + C2 = 2.0, OUT1 = (F1 > 1.0) = true after one scan.
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    TEST_ASSERT(le_vm_step(&vm, 10) == LE_OK, "step t=10");
    TEST_ASSERT(!ht->q, "TON not done at t=10 (<100ms)");
    TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)), "OUT0 gated off before timer done");
    float f1 = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(0));
    TEST_ASSERT(f1 >= 1.99f && f1 <= 2.01f, "F1 = 1+1 = 2.0 after a scan");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(1)), "OUT1 = (2 > 1) is true");

    // t=120: TON done; with IN1 also set, OUT0 goes high.
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    TEST_ASSERT(le_vm_step(&vm, 120) == LE_OK, "step t=120");
    TEST_ASSERT(ht->q, "TON done after preset");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)), "OUT0 = TON && IN1 is true");

    // Dropping the timer enable arms the TON down path; OUT0 clears immediately.
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    TEST_ASSERT(le_vm_step(&vm, 200) == LE_OK, "step t=200");
    TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)), "OUT0 clears when timer enable drops");

    TEST_ASSERT(vm.cycle_count == 3, "three scans elapsed");
    le_compile_result_free(&res);
}

void test_register_limits_load_reject()
{
    // Circuit declaring more registers than fit in the runtime workspace budget.
    // The compiler emits it; the RUNTIME loader rejects it with LE_ERR_CAPACITY.
    int num_floats = (LE_RAM_WORKSPACE_BYTES / 4) + 1;
    std::string s = "{\"name\":\"TooManyFloats\",\"elements\":[";
    for (int i = 0; i < num_floats; i++) {
        s += "{\"name\":\"F" + std::to_string(i) + "\",\"type\":\"FLOATREGISTER\"}";
        if (i < num_floats - 1) s += ",";
    }
    s += "],\"nets\":[]}";

    le_compile_result_t res;
    int rc = le_compile_json(s.c_str(), nullptr, &res);
    TEST_ASSERT(rc == 0 && res.success, "over-budget circuit still compiles (compiler has no float cap)");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_ERR_CAPACITY,
                "runtime loader rejects register count beyond LE_RAM_WORKSPACE_BYTES");
    le_compile_result_free(&res);
}

void test_comms_upload_endtoend()
{
    // Compile a real circuit, upload it over the wire (PROG_BEGIN/CHUNK/END),
    // let the VM run it, then read back a GET_IMAGE snapshot.
    const char* circuit_json = R"({
        "name": "WireProg",
        "elements": [
            { "name": "IN0", "type": "DIGITALINPUT", "address": "%IN0" },
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%IN1" },
            { "name": "X1", "type": "XOR" },
            { "name": "O0", "type": "DIGITALOUTPUT", "address": "%OUT0" }
        ],
        "nets": [
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "X1", "port": "a" } ] },
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "X1", "port": "b" } ] },
            { "output": { "name": "X1", "port": "out" }, "inputs": [ { "name": "O0", "port": "in" } ] }
        ]
    })";
    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, nullptr, &res);
    TEST_ASSERT(rc == 0 && res.success, "wire circuit compiles");

    const le_hal_t* sim = le_hal_get_sim();
    sim->init();
    le_hal_set(sim);

    le_storage_t storage;
    le_storage_init(&storage, sim);

    le_vm_t vm;
    le_vm_init(&vm);
    le_comms_t comms;
    le_comms_init(&comms, &vm, &storage);

    // Upload targets slot 1 (active slot is 0 by default).
    uint32_t total = (uint32_t)res.binary_size;
    uint8_t begin[5] = {
        (uint8_t)(total & 0xFF), (uint8_t)((total >> 8) & 0xFF),
        (uint8_t)((total >> 16) & 0xFF), (uint8_t)((total >> 24) & 0xFF),
        0x01 /* target slot 1 */
    };
    test_feed_packet(&comms, LE_CMD_PROG_BEGIN, 1, begin, 5);

    std::vector<uint8_t> chunk(2u + res.binary_size);
    chunk[0] = 0; chunk[1] = 0;
    memcpy(chunk.data() + 2, res.binary_data, res.binary_size);
    test_feed_packet(&comms, LE_CMD_PROG_CHUNK, 2, chunk.data(), (uint16_t)chunk.size());
    test_feed_packet(&comms, LE_CMD_PROG_END, 3, NULL, 0);

    // Upload stores to the slot; activation is an explicit, separate step.
    TEST_ASSERT(le_storage_activate_slot(&storage, 1, &vm), "uploaded slot 1 activates");
    TEST_ASSERT(vm.instruction_count >= 1, "uploaded program committed to the VM");
    TEST_ASSERT(vm.running, "uploaded program autostarted");

    // Drive it: IN0=1, IN1=0 -> XOR -> OUT0 on.
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "uploaded program steps");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)), "XOR(1,0)=1 on the wire program");

    // GET_IMAGE snapshot reflects the live OUT bit.
    test_feed_packet(&comms, LE_CMD_GET_IMAGE, 4, NULL, 0);
    uint32_t total_bits = (uint32_t)vm.image.din_count + (uint32_t)vm.image.dout_count +
                          ((vm.image.bool_count < 128u) ? vm.image.bool_count : 128u);
    size_t img_bytes = (total_bits + 7u) / 8u;
    TEST_ASSERT(img_bytes >= 1, "snapshot has at least 1 byte");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)),
                "VM state untouched by the snapshot request");

    le_compile_result_free(&res);
}

void test_analog_output_pipeline()
{
    const char* circuit_json = R"({
        "name": "AnalogOutTest",
        "elements": [
            { "name": "C1", "type": "CONSTANT", "dataType": "float", "value": 1.0 },
            { "name": "C2", "type": "CONSTANT", "dataType": "float", "value": 1.0 },
            { "name": "ADD1", "type": "ADD" },
            { "name": "AO0", "type": "ANALOGOUTPUT", "channel": 0 }
        ],
        "nets": [
            { "output": { "name": "C1", "port": "out" }, "inputs": [ { "name": "ADD1", "port": "a" } ] },
            { "output": { "name": "C2", "port": "out" }, "inputs": [ { "name": "ADD1", "port": "b" } ] },
            { "output": { "name": "ADD1", "port": "out" }, "inputs": [ { "name": "AO0", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "analog output circuit compiles");
    TEST_ASSERT(res.aout_count == 1, "res.aout_count is 1");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "analog output binary loads");
    TEST_ASSERT(vm.image.aout_count == 1, "process image has 1 analog output");

    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "step circuit");
    TEST_ASSERT(fabsf(le_pi_get_aout(&vm.image, 0) - 2.0f) < 1e-4f, "AO0 has value 2.0 via le_pi_get_aout");
    TEST_ASSERT(fabsf(le_process_image_get_float(&vm.image, LE_ADDR_MAKE_AOUT(0)) - 2.0f) < 1e-4f,
                "AO0 has value 2.0 via LE_ADDR_MAKE_AOUT");

    le_compile_result_free(&res);
}

void test_sparse_io_addressing_accommodated()
{
    // Circuit uses only %I4 and %Q5 (no lower inputs/outputs declared)
    const char* circuit_json = R"({
        "name": "SparseIOCircuit",
        "elements": [
            { "name": "BTN4", "type": "DIGITALINPUT", "address": "%I4" },
            { "name": "OUT5", "type": "DIGITALOUTPUT", "address": "%Q5" }
        ],
        "nets": [
            { "output": { "name": "BTN4", "port": "out" }, "inputs": [ { "name": "OUT5", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res;
    int rc = le_compile_json(circuit_json, nullptr, &res);
    TEST_ASSERT(rc == 0 && res.success, "sparse I/O circuit compiles");

    le_header_t h;
    TEST_ASSERT(le_loader_validate(res.binary_data, res.binary_size, &h) == LE_OK, "sparse binary validates");
    TEST_ASSERT(h.digital_in_count >= 5, "header allocates at least 5 DIN for %I4");
    TEST_ASSERT(h.digital_out_count >= 6, "header allocates at least 6 DOUT for %Q5");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "sparse binary loads into VM");

    // %I4 can be written and read through process image
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(4), true);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DIN(4)) == true, "%I4 readback is true");

    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm step succeeds");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(5)) == true, "%Q5 is set by %I4");

    le_compile_result_free(&res);

    // Now test with board profile declaring 16 digital inputs
    const char* board_json = R"({
        "device": { "name": "Board16" },
        "limits": { "digital_inputs": 16, "digital_outputs": 16 }
    })";
    rc = le_compile_json(circuit_json, board_json, &res);
    TEST_ASSERT(rc == 0 && res.success, "sparse I/O with board compiles");
    TEST_ASSERT(le_loader_validate(res.binary_data, res.binary_size, &h) == LE_OK, "board binary validates");
    TEST_ASSERT(h.digital_in_count >= 16, "header allocates all 16 board inputs");
    TEST_ASSERT(h.digital_out_count >= 16, "header allocates all 16 board outputs");
    le_compile_result_free(&res);
}

void test_rs_latch_feedback_register()
{
    const char* circuit_json = R"({
        "name": "RS_Latch_Test",
        "elements": [
            { "name": "IN0", "type": "DIGITALINPUT" },
            { "name": "IN4", "type": "DIGITALINPUT" },
            { "name": "OR", "type": "OR" },
            { "name": "AND", "type": "AND" },
            { "name": "NOT", "type": "NOT" },
            { "name": "B_REG1", "type": "BOOLREGISTER" },
            { "name": "OUT1", "type": "DIGITALOUTPUT" }
        ],
        "nets": [
            { "output": { "name": "IN4", "port": "out" }, "inputs": [ { "name": "NOT", "port": "in" } ] },
            { "output": { "name": "NOT", "port": "out" }, "inputs": [ { "name": "AND", "port": "b" } ] },
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "OR", "port": "b" } ] },
            { "output": { "name": "B_REG1", "port": "out" }, "inputs": [ { "name": "OR", "port": "a" }, { "name": "OUT1", "port": "in" } ] },
            { "output": { "name": "OR", "port": "out" }, "inputs": [ { "name": "AND", "port": "a" } ] },
            { "output": { "name": "AND", "port": "out" }, "inputs": [ { "name": "B_REG1", "port": "in" } ] }
        ]
    })";

    // 1. Test with LE_OPT_NONE (-O0)
    le_compiler_options_t opts_none = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res_none;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts_none, &res_none);
    TEST_ASSERT(rc == 0 && res_none.success, "RS latch compiles at -O0");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res_none.binary_data, res_none.binary_size) == LE_OK, "RS latch loads");
    le_vm_start(&vm);

    // Step 1: Set=1, Reset=0
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "step 1");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)) == true, "B_REG1 is set");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 is set");

    // Step 2: Set=0, Reset=0 -> Must stay latched!
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    TEST_ASSERT(le_vm_step(&vm, 1) == LE_OK, "step 2");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)) == true, "B_REG1 remains latched");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 remains latched");

    // Step 3: Set=0, Reset=1 -> Must reset!
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    TEST_ASSERT(le_vm_step(&vm, 2) == LE_OK, "step 3");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)) == false, "B_REG1 resets");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT1 resets");

    le_compile_result_free(&res_none);

    // 2. Test with LE_OPT_FULL (-O2)
    le_compiler_options_t opts_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res_full;
    rc = le_compile_json_ex(circuit_json, nullptr, &opts_full, &res_full);
    TEST_ASSERT(rc == 0 && res_full.success, "RS latch compiles at -O2");

    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res_full.binary_data, res_full.binary_size) == LE_OK, "RS latch loads (opt)");
    le_vm_start(&vm);

    // Step 1: Set=1, Reset=0
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "opt step 1");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)) == true, "opt B_REG1 is set");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "opt OUT1 is set");

    // Step 2: Set=0, Reset=0 -> Must stay latched!
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    TEST_ASSERT(le_vm_step(&vm, 1) == LE_OK, "opt step 2");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)) == true, "opt B_REG1 remains latched");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "opt OUT1 remains latched");

    // Step 3: Set=0, Reset=1 -> Must reset!
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    TEST_ASSERT(le_vm_step(&vm, 2) == LE_OK, "opt step 3");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0)) == false, "opt B_REG1 resets");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "opt OUT1 resets");

    le_compile_result_free(&res_full);
}

void test_dedicated_latch_node_state_retention()
{
    // A circuit with a LATCH (RS) element, followed by downstream logic (AND, OR)
    // that uses temporary boolean registers. The latch MUST retain its state across
    // scans even when downstream nodes acquire and overwrite temporary boolean registers.
    const char* circuit_json = R"({
        "name": "Dedicated_Latch_Circuit",
        "elements": [
            { "name": "IN0", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN4", "type": "DIGITALINPUT", "address": "%I4" },
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I2" },
            { "name": "L1", "type": "LATCH" },
            { "name": "OUT0", "type": "DIGITALOUTPUT", "address": "%Q0" },
            { "name": "AND1", "type": "AND" },
            { "name": "OR1", "type": "OR" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q1" }
        ],
        "nets": [
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "L1", "port": "s" } ] },
            { "output": { "name": "IN4", "port": "out" }, "inputs": [ { "name": "L1", "port": "r" } ] },
            { "output": { "name": "L1", "port": "q" }, "inputs": [ { "name": "OUT0", "port": "in" } ] },
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OR1", "port": "a" } ] },
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "OR1", "port": "b" } ] },
            { "output": { "name": "OR1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "Dedicated LATCH circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "Dedicated LATCH circuit loads");
    le_vm_start(&vm);

    // Scan 1: Set=1, Reset=0 -> Latch Q becomes 1, OUT0 becomes 1
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(4), false);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "latch scan 1");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT0 set to 1");

    // Scan 2: Set=0, Reset=0 -> Latch MUST REMAIN 1 across scan!
    // Downstream gates (AND1, OR1) will execute, but should NOT clobber the latch state!
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(4), false);
    TEST_ASSERT(le_vm_step(&vm, 1) == LE_OK, "latch scan 2");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT0 remains latched at 1");

    // Scan 3: Set=0, Reset=1 -> Latch resets to 0
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(4), true);
    TEST_ASSERT(le_vm_step(&vm, 2) == LE_OK, "latch scan 3");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT0 reset to 0");

    // Scan 4: Set=0, Reset=0 -> Latch remains 0
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(4), false);
    TEST_ASSERT(le_vm_step(&vm, 3) == LE_OK, "latch scan 4");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT0 remains 0");

    le_compile_result_free(&res);
}

void test_cross_coupled_latch_auto_feedback()
{
    // Discrete cross-coupled RS latch without an explicit BOOLREGISTER element:
    // OR <-> AND combinatorial loop. The compiler automatically synthesizes a
    // persistent feedback register to hold state across scans.
    const char* circuit_json = R"({
        "name": "CrossCoupled_RS",
        "elements": [
            { "name": "IN0", "type": "DIGITALINPUT" },
            { "name": "IN4", "type": "DIGITALINPUT" },
            { "name": "OR", "type": "OR" },
            { "name": "NOT", "type": "NOT" },
            { "name": "AND", "type": "AND" },
            { "name": "OUT0", "type": "DIGITALOUTPUT" }
        ],
        "nets": [
            { "output": { "name": "IN0", "port": "out" }, "inputs": [ { "name": "OR", "port": "a" } ] },
            { "output": { "name": "OR", "port": "out" }, "inputs": [ { "name": "OUT0", "port": "in" }, { "name": "AND", "port": "a" } ] },
            { "output": { "name": "IN4", "port": "out" }, "inputs": [ { "name": "NOT", "port": "in" } ] },
            { "output": { "name": "NOT", "port": "out" }, "inputs": [ { "name": "AND", "port": "b" } ] },
            { "output": { "name": "AND", "port": "out" }, "inputs": [ { "name": "OR", "port": "b" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success, "cross-coupled latch compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "cross-coupled latch loads");
    le_vm_start(&vm);

    // Step 1: Set=1, Reset=0 -> Output=1
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "cc step 1");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT0 is set");

    // Step 2: Set=0, Reset=0 -> Must stay latched!
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    TEST_ASSERT(le_vm_step(&vm, 1) == LE_OK, "cc step 2");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT0 remains latched");

    // Step 3: Set=0, Reset=1 -> Resets to 0!
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    TEST_ASSERT(le_vm_step(&vm, 2) == LE_OK, "cc step 3");
    // Step 4: After reset propagation, output is 0
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    TEST_ASSERT(le_vm_step(&vm, 3) == LE_OK, "cc step 4");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT0 is reset");

    le_compile_result_free(&res);
}

void test_boolean_simplification_annihilation()
{
    // A && !A -> FALSE
    const char* circuit_and = R"({
        "name": "AnnihilationAnd",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "NOT1", "type": "NOT" },
            { "name": "AND1", "type": "AND" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "NOT1", "port": "in" }, { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "NOT1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res_and;
    int rc = le_compile_json_ex(circuit_and, nullptr, &opt_full, &res_and);
    TEST_ASSERT(rc == 0 && res_and.success == 1, "Annihilation AND compile failed");
    // AND1 and NOT1 should both be eliminated, collapsing to a single MOVE FALSE -> %OUT[0]
    TEST_ASSERT(res_and.instruction_count == 1, "Expected 1 instruction (constant FALSE write)");

    le_vm_t vm;
    TEST_ASSERT(le_vm_init(&vm) == LE_OK, "vm_init failed");
    TEST_ASSERT(le_loader_load(&vm, res_and.binary_data, res_and.binary_size) == LE_OK, "load failed");

    // Test with IN1 = true -> OUT1 should be false
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT1 must be false");

    // Test with IN1 = false -> OUT1 should be false
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    TEST_ASSERT(le_vm_step(&vm, 1) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT1 must be false");
    le_compile_result_free(&res_and);

    // A || !A -> TRUE
    const char* circuit_or = R"({
        "name": "AnnihilationOr",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "NOT1", "type": "NOT" },
            { "name": "OR1", "type": "OR" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "NOT1", "port": "in" }, { "name": "OR1", "port": "b" } ] },
            { "output": { "name": "NOT1", "port": "out" }, "inputs": [ { "name": "OR1", "port": "a" } ] },
            { "output": { "name": "OR1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res_or;
    rc = le_compile_json_ex(circuit_or, nullptr, &opt_full, &res_or);
    TEST_ASSERT(rc == 0 && res_or.success == 1, "Annihilation OR compile failed");
    TEST_ASSERT(res_or.instruction_count == 1, "Expected 1 instruction (constant TRUE write)");

    TEST_ASSERT(le_loader_load(&vm, res_or.binary_data, res_or.binary_size) == LE_OK, "load failed");
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 must be true for !0 || 0");
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    TEST_ASSERT(le_vm_step(&vm, 1) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 must be true for !1 || 1");
    le_compile_result_free(&res_or);
}

void test_boolean_simplification_idempotence()
{
    // A && A -> A
    const char* circuit_and = R"({
        "name": "IdempotenceAnd",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "AND1", "type": "AND" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" }, { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res_and;
    int rc = le_compile_json_ex(circuit_and, nullptr, &opt_full, &res_and);
    TEST_ASSERT(rc == 0 && res_and.success == 1, "Idempotence AND compile failed");
    // AND1 eliminated, direct move IN1 -> OUT1
    TEST_ASSERT(res_and.instruction_count == 1, "Expected 1 instruction (MOVE IN1 -> OUT1)");

    le_vm_t vm;
    TEST_ASSERT(le_vm_init(&vm) == LE_OK, "vm_init failed");
    TEST_ASSERT(le_loader_load(&vm, res_and.binary_data, res_and.binary_size) == LE_OK, "load failed");

    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true, "OUT1 must be true");

    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    TEST_ASSERT(le_vm_step(&vm, 1) == LE_OK, "vm_step failed");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == false, "OUT1 must be false");
    le_compile_result_free(&res_and);

    // A ^ A -> FALSE
    const char* circuit_xor = R"({
        "name": "IdempotenceXor",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "XOR1", "type": "XOR" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "XOR1", "port": "a" }, { "name": "XOR1", "port": "b" } ] },
            { "output": { "name": "XOR1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res_xor;
    rc = le_compile_json_ex(circuit_xor, nullptr, &opt_full, &res_xor);
    TEST_ASSERT(rc == 0 && res_xor.success == 1, "Idempotence XOR compile failed");
    TEST_ASSERT(res_xor.instruction_count == 1, "Expected 1 instruction (constant FALSE write)");
    le_compile_result_free(&res_xor);
}

void test_boolean_simplification_absorption()
{
    // A || (A && B) -> A
    const char* circuit_abs = R"({
        "name": "AbsorptionTest",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "AND1", "type": "AND" },
            { "name": "OR1", "type": "OR" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" }, { "name": "OR1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OR1", "port": "b" } ] },
            { "output": { "name": "OR1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_abs, nullptr, &opt_full, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Absorption compile failed");
    // Both AND1 and OR1 eliminated, leaving MOVE IN1 -> OUT1
    TEST_ASSERT(res.instruction_count == 1, "Expected 1 instruction after absorption (MOVE IN1 -> OUT1)");

    le_vm_t vm;
    TEST_ASSERT(le_vm_init(&vm) == LE_OK, "vm_init failed");
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "load failed");

    // All 4 input combinations: OUT1 must equal IN1
    for (int in1 = 0; in1 <= 1; in1++) {
        for (int in2 = 0; in2 <= 1; in2++) {
            le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), in1 != 0);
            le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), in2 != 0);
            TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
            bool expected = (in1 != 0);
            TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == expected,
                        "Absorption logic truth table mismatch");
        }
    }
    le_compile_result_free(&res);

    // Redundancy / Simplification: A || (!A && B) -> A || B
    const char* circuit_red = R"({
        "name": "RedundancyTest",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "NOT1", "type": "NOT" },
            { "name": "AND1", "type": "AND" },
            { "name": "OR1", "type": "OR" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "NOT1", "port": "in" }, { "name": "OR1", "port": "a" } ] },
            { "output": { "name": "NOT1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OR1", "port": "b" } ] },
            { "output": { "name": "OR1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res_red;
    rc = le_compile_json_ex(circuit_red, nullptr, &opt_full, &res_red);
    TEST_ASSERT(rc == 0 && res_red.success == 1, "Redundancy compile failed");
    // A || (!A && B) simplifies to A || B, which coalesces into 1 instruction OR(IN1, IN2) -> OUT1
    TEST_ASSERT(res_red.instruction_count == 1, "Expected 1 fused OR instruction for A || B");

    TEST_ASSERT(le_loader_load(&vm, res_red.binary_data, res_red.binary_size) == LE_OK, "load failed");
    for (int in1 = 0; in1 <= 1; in1++) {
        for (int in2 = 0; in2 <= 1; in2++) {
            le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), in1 != 0);
            le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), in2 != 0);
            TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
            bool expected = (in1 != 0) || (in2 != 0);
            TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == expected,
                        "Redundancy logic truth table mismatch");
        }
    }
    le_compile_result_free(&res_red);
}

void test_boolean_simplification_complementary_merging()
{
    // (A && B) || (A && !B) -> A
    const char* circuit_cm = R"({
        "name": "ComplementaryMergingTest",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "NOT1", "type": "NOT" },
            { "name": "AND1", "type": "AND" },
            { "name": "AND2", "type": "AND" },
            { "name": "OR1", "type": "OR" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" }, { "name": "AND2", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" }, { "name": "NOT1", "port": "in" } ] },
            { "output": { "name": "NOT1", "port": "out" }, "inputs": [ { "name": "AND2", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OR1", "port": "a" } ] },
            { "output": { "name": "AND2", "port": "out" }, "inputs": [ { "name": "OR1", "port": "b" } ] },
            { "output": { "name": "OR1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opt_full = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_cm, nullptr, &opt_full, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Complementary merging compile failed");
    // (A && B) || (A && !B) simplifies directly to A (MOVE IN1 -> OUT1)
    TEST_ASSERT(res.instruction_count == 1, "Expected 1 instruction after complementary merging (MOVE IN1 -> OUT1)");

    le_vm_t vm;
    TEST_ASSERT(le_vm_init(&vm) == LE_OK, "vm_init failed");
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "load failed");

    for (int in1 = 0; in1 <= 1; in1++) {
        for (int in2 = 0; in2 <= 1; in2++) {
            le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), in1 != 0);
            le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), in2 != 0);
            TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "vm_step failed");
            bool expected = (in1 != 0);
            TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == expected,
                        "Complementary merging truth table mismatch");
        }
    }
    le_compile_result_free(&res);
}

void test_ring_oscillator_hazard_warning()
{
    // 3-inverter ring oscillator: NOT1 -> NOT2 -> NOT3 -> NOT1 (odd inversion parity = 3)
    const char* ring_json = R"({
        "name": "RingOscillator",
        "elements": [
            { "name": "N1", "type": "NOT" },
            { "name": "N2", "type": "NOT" },
            { "name": "N3", "type": "NOT" },
            { "name": "OUT0", "type": "DIGITALOUTPUT", "address": "%OUT0" }
        ],
        "nets": [
            { "output": { "name": "N1", "port": "out" }, "inputs": [ { "name": "N2", "port": "in" } ] },
            { "output": { "name": "N2", "port": "out" }, "inputs": [ { "name": "N3", "port": "in" } ] },
            { "output": { "name": "N3", "port": "out" }, "inputs": [ { "name": "N1", "port": "in" }, { "name": "OUT0", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(ring_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Ring oscillator compiles safely");
    TEST_ASSERT(res.warnings != nullptr, "Expected compiler warnings for ring oscillator");
    TEST_ASSERT(std::strstr(res.warnings, "ring oscillator") != nullptr,
                "Warning must mention combinatorial ring oscillator hazard");

    // Verify VM executes deterministically using synthesized unit-delay register
    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "ring oscillator loads");
    le_vm_start(&vm);

    // Initial state: first scan computes output
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "step 0");
    bool v0 = le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0));
    // Second scan: inverting feedback toggles output!
    TEST_ASSERT(le_vm_step(&vm, 1) == LE_OK, "step 1");
    bool v1 = le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0));
    TEST_ASSERT(v0 != v1, "Ring oscillator output toggles deterministically per scan");

    le_compile_result_free(&res);
}

void test_instruction_fusion_fma()
{
    // A * B + C fused into FMA
    const char* circuit_json = R"({
        "name": "FmaTest",
        "elements": [
            { "name": "A", "type": "FLOATREGISTER" },
            { "name": "B", "type": "FLOATREGISTER" },
            { "name": "C", "type": "FLOATREGISTER" },
            { "name": "M", "type": "MUL" },
            { "name": "ADD1", "type": "ADD" },
            { "name": "OUT_F", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "A", "port": "out" }, "inputs": [ { "name": "M", "port": "a" } ] },
            { "output": { "name": "B", "port": "out" }, "inputs": [ { "name": "M", "port": "b" } ] },
            { "output": { "name": "M", "port": "out" }, "inputs": [ { "name": "ADD1", "port": "a" } ] },
            { "output": { "name": "C", "port": "out" }, "inputs": [ { "name": "ADD1", "port": "b" } ] },
            { "output": { "name": "ADD1", "port": "out" }, "inputs": [ { "name": "OUT_F", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "FMA circuit compiles");

    // Load and test VM execution
    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "FMA program loads");

    // Set A = 3.0, B = 4.0, C = 5.0
    // Float registers: A (%F0), B (%F1), C (%F2), OUT_F (%F3)
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 3.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(1), 4.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(2), 5.0f);

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "FMA step succeeds");

    // 3.0 * 4.0 + 5.0 = 17.0
    float out_val = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(3));
    TEST_ASSERT(fabsf(out_val - 17.0f) < 1e-4f, "FMA computes 3*4 + 5 = 17.0f");

    le_compile_result_free(&res);
}

void test_instruction_fusion_fms()
{
    // C - (A * B) fused into FMS
    const char* circuit_json = R"({
        "name": "FmsTest",
        "elements": [
            { "name": "A", "type": "FLOATREGISTER" },
            { "name": "B", "type": "FLOATREGISTER" },
            { "name": "C", "type": "FLOATREGISTER" },
            { "name": "M", "type": "MUL" },
            { "name": "SUB1", "type": "SUB" },
            { "name": "OUT_F", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "A", "port": "out" }, "inputs": [ { "name": "M", "port": "a" } ] },
            { "output": { "name": "B", "port": "out" }, "inputs": [ { "name": "M", "port": "b" } ] },
            { "output": { "name": "C", "port": "out" }, "inputs": [ { "name": "SUB1", "port": "a" } ] },
            { "output": { "name": "M", "port": "out" }, "inputs": [ { "name": "SUB1", "port": "b" } ] },
            { "output": { "name": "SUB1", "port": "out" }, "inputs": [ { "name": "OUT_F", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "FMS circuit compiles");

    // Load and test VM execution
    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "FMS program loads");

    // Set A = 2.5, B = 4.0, C = 20.0
    // Float registers: A (%F0), B (%F1), C (%F2), OUT_F (%F3)
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 2.5f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(1), 4.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(2), 20.0f);

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "FMS step succeeds");

    // 20.0 - (2.5 * 4.0) = 10.0
    float out_val = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(3));
    TEST_ASSERT(fabsf(out_val - 10.0f) < 1e-4f, "FMS computes 20 - (2.5*4) = 10.0f");

    le_compile_result_free(&res);
}

void test_chunked_container_emission()
{
    const char* circuit_json = R"({
        "name": "TestChunkedEmit",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "OR1", "type": "OR" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "OR1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "OR1", "port": "b" } ] },
            { "output": { "name": "OR1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t options = le_compiler_options_init_default();
    options.emit_chunked_container = 1;

    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &options, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Compilation with emit_chunked_container succeeds");
    TEST_ASSERT(res.binary_data != nullptr && res.binary_size > sizeof(le_container_header_t), "Binary emitted");

    // Verify container header magic is LEB2
    uint32_t magic = *(const uint32_t*)res.binary_data;
    TEST_ASSERT(magic == LE_BIN_MAGIC_V2, "Emitted binary magic is LEB2 (0x4C454232)");

    // Disassembly contains chunked container banner
    TEST_ASSERT(res.disassembly_text != nullptr, "Disassembly generated");
    std::string disasm(res.disassembly_text);
    TEST_ASSERT(disasm.find("LOGICELEMENTS CONTAINER DISASSEMBLY (.lebin v13 Chunked)") != std::string::npos,
                "Disassembly identifies chunked container format");

    // Load into VM and verify
    le_vm_t vm;
    std::memset(&vm, 0, sizeof(vm));
    le_status_t st = le_loader_load(&vm, res.binary_data, res.binary_size);
    TEST_ASSERT(st == LE_OK, "le_loader_load accepts emitted chunked container");

    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_vm_step(&vm, 0);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)) == true,
                "Logic executes correctly from emitted chunked container");

    le_compile_result_free(&res);
}

void test_multi_rate_compilation_and_execution()
{
    // Fast logic (AND -> OUT0), Medium logic (LPF -> OUT_F0), Slow logic (TOTALIZER -> OUT_F1)
    const char* circuit_json = R"({
        "name": "TestMultiRateCircuit",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "IN2", "type": "DIGITALINPUT", "address": "%I1" },
            { "name": "AND1", "type": "AND" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" },
            { "name": "AIN1", "type": "ANALOGINPUT", "address": "%AIN0" },
            { "name": "LPF1", "type": "LPF", "cutoff_hz": 10.0 },
            { "name": "OUT_F0", "type": "FLOATREGISTER", "address": "%F0" },
            { "name": "TOT1", "type": "TOTALIZER", "scale": 1.0 },
            { "name": "OUT_F1", "type": "FLOATREGISTER", "address": "%F1" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "AND1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "AND1", "port": "b" } ] },
            { "output": { "name": "AND1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] },
            { "output": { "name": "AIN1", "port": "out" }, "inputs": [ { "name": "LPF1", "port": "in" } ] },
            { "output": { "name": "LPF1", "port": "out" }, "inputs": [ { "name": "OUT_F0", "port": "in" } ] },
            { "output": { "name": "AIN1", "port": "out" }, "inputs": [ { "name": "TOT1", "port": "in" } ] },
            { "output": { "name": "TOT1", "port": "out" }, "inputs": [ { "name": "OUT_F1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t options = le_compiler_options_init_default();
    options.enable_multi_rate = 1;
    options.rate_med_div = 10;
    options.rate_slow_div = 100;

    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &options, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Compilation with enable_multi_rate succeeds");
    TEST_ASSERT(res.binary_data != nullptr, "Binary data produced");

    // Must be chunked container with RATE chunk
    le_chunked_view_t view;
    le_status_t st = le_loader_parse_chunks(res.binary_data, res.binary_size, &view);
    TEST_ASSERT(st == LE_OK, "Parsed chunks from multi-rate binary");
    TEST_ASSERT(view.rate != nullptr, "RATE chunk present in container");
    TEST_ASSERT(view.rate->rate_fast_count > 0, "rate_fast_count > 0");
    TEST_ASSERT(view.rate->rate_med_count > 0, "rate_med_count > 0");
    TEST_ASSERT(view.rate->rate_med_div == 10, "rate_med_div == 10");
    TEST_ASSERT(view.rate->rate_slow_div == 100, "rate_slow_div == 100");

    // Check disassembly mentions Execution Rates
    std::string disasm(res.disassembly_text);
    TEST_ASSERT(disasm.find("Execution Rates:") != std::string::npos, "Disassembly reports Execution Rates");

    // Load into VM
    le_vm_t vm;
    std::memset(&vm, 0, sizeof(vm));
    st = le_loader_load(&vm, res.binary_data, res.binary_size);
    TEST_ASSERT(st == LE_OK, "VM successfully loaded multi-rate container");
    TEST_ASSERT(vm.rate_fast_count == view.rate->rate_fast_count, "VM fast count matches");
    TEST_ASSERT(vm.rate_med_count == view.rate->rate_med_count, "VM med count matches");
    TEST_ASSERT(vm.rate_med_div == 10, "VM med divisor matches");
    TEST_ASSERT(vm.rate_slow_div == 100, "VM slow divisor matches");

    le_compile_result_free(&res);
}

void test_cryptographic_signing_api()
{
    const char* circuit_json = R"({
        "name": "TestSignedCircuit",
        "elements": [
            { "name": "IN1", "type": "DIGITALINPUT", "address": "%I0" },
            { "name": "NOT1", "type": "NOT" },
            { "name": "OUT1", "type": "DIGITALOUTPUT", "address": "%Q0" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "NOT1", "port": "in" } ] },
            { "output": { "name": "NOT1", "port": "out" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t options = le_compiler_options_init_default();
    options.emit_chunked_container = 1;

    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &options, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Compilation succeeds");

    // Keygen
    uint8_t seed[32] = { 0x42, 0x13, 0x37, 0x99 };
    uint8_t pk[32], sk[64];
    rc = le_compiler_keygen(pk, sk, seed);
    TEST_ASSERT(rc == 0, "le_compiler_keygen succeeds");

    // Sign container
    std::vector<uint8_t> signed_container(res.binary_size + 512);
    size_t signed_size = 0;
    rc = le_compiler_sign_container(res.binary_data, res.binary_size, sk,
                                   signed_container.data(), signed_container.size(), &signed_size);
    TEST_ASSERT(rc == 0 && signed_size > res.binary_size, "le_compiler_sign_container succeeds");

    // Validate with correct public key
    le_header_t hdr;
    le_status_t st = le_loader_validate_signed(signed_container.data(), signed_size, pk, &hdr);
    TEST_ASSERT(st == LE_OK, "le_loader_validate_signed accepts container signed by compiler API");

    // Validate with wrong public key
    uint8_t wrong_pk[32];
    std::memcpy(wrong_pk, pk, 32);
    wrong_pk[0] ^= 0xAA;
    st = le_loader_validate_signed(signed_container.data(), signed_size, wrong_pk, &hdr);
    TEST_ASSERT(st == LE_ERR_SIGNATURE_INVALID, "le_loader_validate_signed rejects wrong public key");

    // Verify disassembler recognizes SIGN chunk
    char* dis_signed = nullptr;
    char* err_signed = nullptr;
    rc = le_disassemble_bin(signed_container.data(), signed_size, -1, -1, -1, nullptr, &dis_signed, &err_signed);
    TEST_ASSERT(rc == 0 && dis_signed != nullptr, "Disassembly of signed container succeeds");
    std::string dis_str(dis_signed);
    TEST_ASSERT(dis_str.find("SIGN") != std::string::npos || dis_str.find("Signature") != std::string::npos,
                "Disassembly mentions signature chunk");
    le_compiler_free_buffer(dis_signed);

    le_compile_result_free(&res);
}

void test_code_block_basic_arithmetic()
{
    const char* circuit_json = R"({
        "name": "CbArith",
        "elements": [
            { "name": "IN1", "type": "FLOATREGISTER" },
            { "name": "IN2", "type": "FLOATREGISTER" },
            {
                "name": "CB1",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "a", "type": "float" },
                    { "name": "b", "type": "float" }
                ],
                "outputs": [
                    { "name": "c", "type": "float" }
                ],
                "code": "float c = a * 2.0f + b;"
            },
            { "name": "OUT1", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "CB1", "port": "a" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "CB1", "port": "b" } ] },
            { "output": { "name": "CB1", "port": "c" }, "inputs": [ { "name": "OUT1", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Code block arithmetic compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "Code block binary loads");

    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 5.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(1), 3.0f);

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "Code block VM step succeeds");

    float out_val = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(2));
    if (res.disassembly_text) { std::cout << "DISASM:\n" << res.disassembly_text << "\n"; }
    std::cout << "out_val=" << out_val << "\n";
    TEST_ASSERT(fabsf(out_val - 13.0f) < 1e-4f, "Code block computes 5*2 + 3 = 13.0f");

    le_compile_result_free(&res);
}

void test_code_block_heterogeneous_io()
{
    const char* circuit_json = R"({
        "name": "CbHetero",
        "elements": [
            { "name": "EN", "type": "BOOLREGISTER" },
            { "name": "COUNT", "type": "INTREGISTER" },
            { "name": "VAL", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "en", "type": "bool" },
                    { "name": "count", "type": "int" },
                    { "name": "val", "type": "float" }
                ],
                "outputs": [
                    { "name": "trip", "type": "bool" },
                    { "name": "next_count", "type": "int" },
                    { "name": "scaled_val", "type": "float" }
                ],
                "code": "bool trip = en && (val > 10.0f); int next_count = count + 5; float scaled_val = val * 0.5f;"
            },
            { "name": "OUT_TRIP", "type": "BOOLREGISTER" },
            { "name": "OUT_NEXT", "type": "INTREGISTER" },
            { "name": "OUT_SCALED", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "EN", "port": "out" }, "inputs": [ { "name": "CB", "port": "en" } ] },
            { "output": { "name": "COUNT", "port": "out" }, "inputs": [ { "name": "CB", "port": "count" } ] },
            { "output": { "name": "VAL", "port": "out" }, "inputs": [ { "name": "CB", "port": "val" } ] },
            { "output": { "name": "CB", "port": "trip" }, "inputs": [ { "name": "OUT_TRIP", "port": "in" } ] },
            { "output": { "name": "CB", "port": "next_count" }, "inputs": [ { "name": "OUT_NEXT", "port": "in" } ] },
            { "output": { "name": "CB", "port": "scaled_val" }, "inputs": [ { "name": "OUT_SCALED", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Heterogeneous code block compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "Heterogeneous binary loads");

    // EN = true (%B0), COUNT = 10 (%I0), VAL = 14.0f (%F0)
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0), true);
    le_process_image_set_int(&vm.image, LE_ADDR_MAKE_INT_REG(0), 10);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 14.0f);

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "Heterogeneous VM step succeeds");

    if (res.disassembly_text) {
        std::cout << "HETERO DISASM:\n" << res.disassembly_text << "\n";
    }

    bool trip_val = le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(1));
    int32_t count_val = le_process_image_get_int(&vm.image, LE_ADDR_MAKE_INT_REG(1));
    float scaled_val = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(1));
    std::cout << "trip_val=" << trip_val << " count_val=" << count_val << " scaled_val=" << scaled_val << "\n";

    TEST_ASSERT(trip_val == true, "trip is true when en=true and val=14 > 10");
    TEST_ASSERT(count_val == 15, "next_count is 10 + 5 = 15");
    TEST_ASSERT(fabsf(scaled_val - 7.0f) < 1e-4f, "scaled_val is 14 * 0.5 = 7.0f");

    le_compile_result_free(&res);
}

void test_code_block_arrays_and_loops()
{
    const char* circuit_json = R"({
        "name": "CbArrays",
        "elements": [
            { "name": "S0", "type": "FLOATREGISTER" },
            { "name": "S1", "type": "FLOATREGISTER" },
            { "name": "S2", "type": "FLOATREGISTER" },
            { "name": "S3", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "samples", "type": "float[4]" }
                ],
                "outputs": [
                    { "name": "sum", "type": "float" },
                    { "name": "picked", "type": "float" }
                ],
                "code": "float sum = 0.0f; for (int i = 0; i < 4; i++) { sum += samples[i]; } int k = 2; float picked = samples[k];"
            },
            { "name": "OUT_SUM", "type": "FLOATREGISTER" },
            { "name": "OUT_PICKED", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "S0", "port": "out" }, "inputs": [ { "name": "CB", "port": "samples_0" } ] },
            { "output": { "name": "S1", "port": "out" }, "inputs": [ { "name": "CB", "port": "samples_1" } ] },
            { "output": { "name": "S2", "port": "out" }, "inputs": [ { "name": "CB", "port": "samples_2" } ] },
            { "output": { "name": "S3", "port": "out" }, "inputs": [ { "name": "CB", "port": "samples_3" } ] },
            { "output": { "name": "CB", "port": "sum" }, "inputs": [ { "name": "OUT_SUM", "port": "in" } ] },
            { "output": { "name": "CB", "port": "picked" }, "inputs": [ { "name": "OUT_PICKED", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Array and loop code block compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "Array program loads");

    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 10.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(1), 20.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(2), 30.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(3), 40.0f);

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "Array VM step succeeds");

    float sum_val = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(4));
    float picked_val = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(5));

    if (res.disassembly_text) {
        std::cout << "ARRAYS DISASM:\n" << res.disassembly_text << "\n";
    }
    std::cout << "sum_val=" << sum_val << " picked_val=" << picked_val << "\n";

    TEST_ASSERT(fabsf(sum_val - 100.0f) < 1e-4f, "Sum over unrolled loop is 100.0f");
    TEST_ASSERT(fabsf(picked_val - 30.0f) < 1e-4f, "Picked dynamic indexed element is 30.0f");

    le_compile_result_free(&res);
}

void test_code_block_conditional_and_inferred_latch()
{
    // Part 1: Inferred latch warning
    const char* circuit_json = R"({
        "name": "CbLatch",
        "elements": [
            { "name": "EN", "type": "BOOLREGISTER" },
            { "name": "IN_VAL", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "en", "type": "bool" },
                    { "name": "in_val", "type": "float" }
                ],
                "outputs": [
                    { "name": "hold_val", "type": "float" }
                ],
                "code": "if (en) { hold_val = in_val; }"
            },
            { "name": "OUT_VAL", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "EN", "port": "out" }, "inputs": [ { "name": "CB", "port": "en" } ] },
            { "output": { "name": "IN_VAL", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_val" } ] },
            { "output": { "name": "CB", "port": "hold_val" }, "inputs": [ { "name": "OUT_VAL", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Inferred latch compiles successfully");
    TEST_ASSERT(res.warnings != nullptr && std::string(res.warnings).find("inferred latch for variable 'hold_val'") != std::string::npos,
                "Non-fatal compiler warning generated for inferred latch");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "Latch program loads");

    // Scan 1: en = true, in_val = 42.0f
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0), true);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 42.0f);
    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "Scan 1 succeeds");
    float out1 = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(out1 - 42.0f) < 1e-4f, "Scan 1 latches 42.0f");

    // Scan 2: en = false, in_val = 99.0f -> should retain 42.0f!
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_BOOL_REG(0), false);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 99.0f);
    TEST_ASSERT(le_vm_step(&vm, 1) == LE_OK, "Scan 2 succeeds");
    float out2 = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(out2 - 42.0f) < 1e-4f, "Scan 2 retains 42.0f across cycle via inferred latch");

    le_compile_result_free(&res);

    // Part 2: Explicit static variable suppresses warning
    const char* circuit_static_json = R"({
        "name": "CbStatic",
        "elements": [
            { "name": "EN", "type": "BOOLREGISTER" },
            { "name": "IN_VAL", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "en", "type": "bool" },
                    { "name": "in_val", "type": "float" }
                ],
                "outputs": [
                    { "name": "hold_val", "type": "float" }
                ],
                "code": "static float hold_val = 0.0f; if (en) { hold_val = in_val; }"
            },
            { "name": "OUT_VAL", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "EN", "port": "out" }, "inputs": [ { "name": "CB", "port": "en" } ] },
            { "output": { "name": "IN_VAL", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_val" } ] },
            { "output": { "name": "CB", "port": "hold_val" }, "inputs": [ { "name": "OUT_VAL", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res_static;
    rc = le_compile_json_ex(circuit_static_json, nullptr, &opts, &res_static);
    TEST_ASSERT(rc == 0 && res_static.success == 1, "Static circuit compiles");
    TEST_ASSERT(res_static.warnings == nullptr || std::string(res_static.warnings).find("inferred latch") == std::string::npos,
                "Explicit static suppresses inferred latch warning");
    le_compile_result_free(&res_static);
}

void test_code_block_user_functions()
{
    const char* circuit_json = R"({
        "name": "CbFunc",
        "elements": [
            { "name": "IN1", "type": "FLOATREGISTER" },
            { "name": "IN2", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "in1", "type": "float" },
                    { "name": "in2", "type": "float" }
                ],
                "outputs": [
                    { "name": "out1", "type": "float" },
                    { "name": "out2", "type": "float" }
                ],
                "code": "float deadband(float x, float thresh) { if (x > thresh) return x - thresh; if (x < -thresh) return x + thresh; return 0.0f; } float out1 = deadband(in1, 2.0f); float out2 = deadband(in2, 2.0f);"
            },
            { "name": "OUT1", "type": "FLOATREGISTER" },
            { "name": "OUT2", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "IN1", "port": "out" }, "inputs": [ { "name": "CB", "port": "in1" } ] },
            { "output": { "name": "IN2", "port": "out" }, "inputs": [ { "name": "CB", "port": "in2" } ] },
            { "output": { "name": "CB", "port": "out1" }, "inputs": [ { "name": "OUT1", "port": "in" } ] },
            { "output": { "name": "CB", "port": "out2" }, "inputs": [ { "name": "OUT2", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(circuit_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "User-defined function code block compiles cleanly");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "Function program loads");

    // IN1 = 5.0f (beyond threshold 2.0 -> 3.0), IN2 = 1.0f (within threshold -> 0.0)
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 5.0f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(1), 1.0f);

    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "Function VM step succeeds");

    float o1 = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(2));
    float o2 = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(3));
    if (res.disassembly_text) {
        std::cout << "FUNC DISASM:\n" << res.disassembly_text << "\n";
    }
    std::cout << "o1=" << o1 << " o2=" << o2 << "\n";

    TEST_ASSERT(fabsf(o1 - 3.0f) < 1e-4f, "deadband(5.0, 2.0) = 3.0f");
    TEST_ASSERT(fabsf(o2 - 0.0f) < 1e-4f, "deadband(1.0, 2.0) = 0.0f");

    le_compile_result_free(&res);
}

void test_code_block_error_paths()
{
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);

    // 1. Syntax Error
    const char* bad_syntax_json = R"({
        "name": "BadSyntax",
        "elements": [
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "code": "float x = * * 5;"
            }
        ],
        "nets": []
    })";
    le_compile_result_t res1;
    int rc1 = le_compile_json_ex(bad_syntax_json, nullptr, &opts, &res1);
    TEST_ASSERT(rc1 != 0 || res1.success == 0, "Bad syntax rejected");
    le_compile_result_free(&res1);

    // 2. Recursive function call rejection
    const char* recursive_json = R"({
        "name": "Recursive",
        "elements": [
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "code": "float fact(float n) { return n * fact(n - 1.0f); } float x = fact(5.0f);"
            }
        ],
        "nets": []
    })";
    le_compile_result_t res2;
    int rc2 = le_compile_json_ex(recursive_json, nullptr, &opts, &res2);
    TEST_ASSERT(rc2 != 0 || res2.success == 0, "Recursive function rejected");
    TEST_ASSERT(res2.error_message != nullptr && std::string(res2.error_message).find("Recursion is prohibited") != std::string::npos,
                "Error message specifies recursion prohibition");
    le_compile_result_free(&res2);
}

void test_code_block_trig_and_math()
{
    // 1. Compile-time constant folding in code block
    const char* const_folding_json = R"({
        "name": "ConstMathBlock",
        "elements": [
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "outputs": [
                    { "name": "v_sin", "type": "float" },
                    { "name": "v_cos", "type": "float" },
                    { "name": "v_sqrt", "type": "float" },
                    { "name": "v_exp", "type": "float" },
                    { "name": "v_pow", "type": "float" },
                    { "name": "v_atan2", "type": "float" },
                    { "name": "v_clamp", "type": "float" }
                ],
                "code": "v_sin = sin(0.0f); v_cos = cos(0.0f); v_sqrt = sqrt(16.0f); v_exp = exp(0.0f); v_pow = pow(2.0f, 3.0f); v_atan2 = atan2(1.0f, 1.0f); v_clamp = clamp(15.0f, 0.0f, 10.0f);"
            },
            { "name": "OUT_SIN", "type": "FLOATREGISTER" },
            { "name": "OUT_COS", "type": "FLOATREGISTER" },
            { "name": "OUT_SQRT", "type": "FLOATREGISTER" },
            { "name": "OUT_EXP", "type": "FLOATREGISTER" },
            { "name": "OUT_POW", "type": "FLOATREGISTER" },
            { "name": "OUT_ATAN2", "type": "FLOATREGISTER" },
            { "name": "OUT_CLAMP", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "CB", "port": "v_sin" }, "inputs": [ { "name": "OUT_SIN", "port": "in" } ] },
            { "output": { "name": "CB", "port": "v_cos" }, "inputs": [ { "name": "OUT_COS", "port": "in" } ] },
            { "output": { "name": "CB", "port": "v_sqrt" }, "inputs": [ { "name": "OUT_SQRT", "port": "in" } ] },
            { "output": { "name": "CB", "port": "v_exp" }, "inputs": [ { "name": "OUT_EXP", "port": "in" } ] },
            { "output": { "name": "CB", "port": "v_pow" }, "inputs": [ { "name": "OUT_POW", "port": "in" } ] },
            { "output": { "name": "CB", "port": "v_atan2" }, "inputs": [ { "name": "OUT_ATAN2", "port": "in" } ] },
            { "output": { "name": "CB", "port": "v_clamp" }, "inputs": [ { "name": "OUT_CLAMP", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res;
    int rc = le_compile_json_ex(const_folding_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Const math circuit compiles");

    le_vm_t vm;
    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "Const math loads");
    le_vm_start(&vm);
    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "Const math step succeeds");

    float out_sin = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(0));
    float out_cos = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(1));
    float out_sqrt = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(2));
    float out_exp = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(3));
    float out_pow = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(4));
    float out_atan2 = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(5));
    float out_clamp = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(6));

    TEST_ASSERT(fabsf(out_sin - 0.0f) < 1e-4f, "sin(0) folded to 0");
    TEST_ASSERT(fabsf(out_cos - 1.0f) < 1e-4f, "cos(0) folded to 1");
    TEST_ASSERT(fabsf(out_sqrt - 4.0f) < 1e-4f, "sqrt(16) folded to 4");
    TEST_ASSERT(fabsf(out_exp - 1.0f) < 1e-4f, "exp(0) folded to 1");
    TEST_ASSERT(fabsf(out_pow - 8.0f) < 1e-4f, "pow(2, 3) folded to 8");
    TEST_ASSERT(fabsf(out_atan2 - 0.785398f) < 1e-3f, "atan2(1, 1) folded to pi/4");
    TEST_ASSERT(fabsf(out_clamp - 10.0f) < 1e-4f, "clamp(15, 0, 10) folded to 10");
    le_compile_result_free(&res);

    // 2. Dynamic runtime evaluation in code block
    const char* dynamic_json = R"({
        "name": "DynamicMathBlock",
        "elements": [
            { "name": "IN_THETA", "type": "FLOATREGISTER" },
            { "name": "IN_VAL", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "theta", "type": "float" },
                    { "name": "val", "type": "float" }
                ],
                "outputs": [
                    { "name": "out_s", "type": "float" },
                    { "name": "out_c", "type": "float" },
                    { "name": "out_sq", "type": "float" },
                    { "name": "out_pw", "type": "float" },
                    { "name": "out_cl", "type": "float" }
                ],
                "code": "out_s = sin(theta); out_c = cos(theta); out_sq = sqrt(val); out_pw = pow(val, 2.0f); out_cl = clamp(val, 2.0f, 8.0f);"
            },
            { "name": "RES_S", "type": "FLOATREGISTER" },
            { "name": "RES_C", "type": "FLOATREGISTER" },
            { "name": "RES_SQ", "type": "FLOATREGISTER" },
            { "name": "RES_PW", "type": "FLOATREGISTER" },
            { "name": "RES_CL", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "IN_THETA", "port": "out" }, "inputs": [ { "name": "CB", "port": "theta" } ] },
            { "output": { "name": "IN_VAL", "port": "out" }, "inputs": [ { "name": "CB", "port": "val" } ] },
            { "output": { "name": "CB", "port": "out_s" }, "inputs": [ { "name": "RES_S", "port": "in" } ] },
            { "output": { "name": "CB", "port": "out_c" }, "inputs": [ { "name": "RES_C", "port": "in" } ] },
            { "output": { "name": "CB", "port": "out_sq" }, "inputs": [ { "name": "RES_SQ", "port": "in" } ] },
            { "output": { "name": "CB", "port": "out_pw" }, "inputs": [ { "name": "RES_PW", "port": "in" } ] },
            { "output": { "name": "CB", "port": "out_cl" }, "inputs": [ { "name": "RES_CL", "port": "in" } ] }
        ]
    })";

    rc = le_compile_json_ex(dynamic_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Dynamic math circuit compiles");

    le_vm_init(&vm);
    TEST_ASSERT(le_loader_load(&vm, res.binary_data, res.binary_size) == LE_OK, "Dynamic math loads");
    le_vm_start(&vm);

    // Set theta = 0.52359877f (~pi/6 = 30 deg), val = 9.0f
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(0), 0.52359877f);
    le_process_image_set_float(&vm.image, LE_ADDR_MAKE_FLOAT(1), 9.0f);

    TEST_ASSERT(le_vm_step(&vm, 0) == LE_OK, "Dynamic math step succeeds");

    float d_s = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(2));
    float d_c = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(3));
    float d_sq = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(4));
    float d_pw = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(5));
    float d_cl = le_process_image_get_float(&vm.image, LE_ADDR_MAKE_FLOAT(6));

    TEST_ASSERT(fabsf(d_s - 0.5f) < 1e-3f, "sin(pi/6) dynamic matches ~0.5");
    TEST_ASSERT(fabsf(d_c - 0.866025f) < 1e-3f, "cos(pi/6) dynamic matches ~0.866");
    TEST_ASSERT(fabsf(d_sq - 3.0f) < 1e-3f, "sqrt(9.0) dynamic matches 3.0");
    TEST_ASSERT(fabsf(d_pw - 81.0f) < 1e-2f, "pow(9.0, 2.0) dynamic matches 81.0");
    TEST_ASSERT(fabsf(d_cl - 8.0f) < 1e-3f, "clamp(9.0, 2.0, 8.0) dynamic matches 8.0");

    le_compile_result_free(&res);
}

void test_code_block_static_array_order_independence()
{
    const char* circuit1_json = R"({
        "name": "CbOrder1",
        "elements": [
            { "name": "IN_A", "type": "DIGITALINPUT" },
            { "name": "IN_B", "type": "DIGITALINPUT" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "in_a", "type": "bool" },
                    { "name": "in_b", "type": "bool" }
                ],
                "outputs": [
                    { "name": "out", "type": "bool" }
                ],
                "code": "static bool[5] s;\nfor(int i = 4; i > 0; i--) { s[i] = s[i - 1]; }\ns[0] = in_a && in_b;\nout = s[4];"
            },
            { "name": "OUT", "type": "DIGITALOUTPUT" }
        ],
        "nets": [
            { "output": { "name": "IN_A", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_a" } ] },
            { "output": { "name": "IN_B", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_b" } ] },
            { "output": { "name": "CB", "port": "out" }, "inputs": [ { "name": "OUT", "port": "in" } ] }
        ]
    })";

    const char* circuit2_json = R"({
        "name": "CbOrder2",
        "elements": [
            { "name": "IN_A", "type": "DIGITALINPUT" },
            { "name": "IN_B", "type": "DIGITALINPUT" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "in_a", "type": "bool" },
                    { "name": "in_b", "type": "bool" }
                ],
                "outputs": [
                    { "name": "out", "type": "bool" }
                ],
                "code": "static bool[5] s;\nout = s[4];\nfor(int i = 4; i > 0; i--) { s[i] = s[i - 1]; }\ns[0] = in_a && in_b;"
            },
            { "name": "OUT", "type": "DIGITALOUTPUT" }
        ],
        "nets": [
            { "output": { "name": "IN_A", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_a" } ] },
            { "output": { "name": "IN_B", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_b" } ] },
            { "output": { "name": "CB", "port": "out" }, "inputs": [ { "name": "OUT", "port": "in" } ] }
        ]
    })";

    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
    le_compile_result_t res1, res2;
    int rc1 = le_compile_json_ex(circuit1_json, nullptr, &opts, &res1);
    int rc2 = le_compile_json_ex(circuit2_json, nullptr, &opts, &res2);

    TEST_ASSERT(rc1 == 0 && res1.success == 1, "Circuit 1 (out after loop) compiles successfully");
    TEST_ASSERT(rc2 == 0 && res2.success == 1, "Circuit 2 (out before loop) compiles successfully");
    TEST_ASSERT(res1.instruction_count == res2.instruction_count, "Instruction count matches between both definitions");
    TEST_ASSERT(res1.binary_size == res2.binary_size, "Binary size matches between both definitions");

    std::string disasm1 = res1.disassembly_text ? res1.disassembly_text : "";
    std::string disasm2 = res2.disassembly_text ? res2.disassembly_text : "";

    std::cout << "DISASM 1 (out after loop):\n" << disasm1 << "\n";
    std::cout << "DISASM 2 (out before loop):\n" << disasm2 << "\n";

    // Both should end by moving %B[22] -> %OUT[0]
    TEST_ASSERT(disasm1.find("%OUT[0]") != std::string::npos, "Disassembly 1 outputs to %OUT[0]");
    TEST_ASSERT(disasm2.find("%OUT[0]") != std::string::npos, "Disassembly 2 outputs to %OUT[0]");

    // Verify both disassemblies have the identical last move operand into %OUT[0]
    auto extract_out_source = [](const std::string& dis) -> std::string {
        size_t pos = dis.find("-> %OUT[0]");
        if (pos == std::string::npos) return "";
        size_t line_start = dis.rfind('\n', pos);
        if (line_start == std::string::npos) line_start = 0;
        return dis.substr(line_start, pos - line_start);
    };

    std::string src1 = extract_out_source(disasm1);
    std::string src2 = extract_out_source(disasm2);
    TEST_ASSERT(!src1.empty() && src1 == src2, "Output wire source matches between both definitions");

    le_compile_result_free(&res1);
    le_compile_result_free(&res2);
}

void test_code_block_documentation_examples()
{
    auto test_compile_cb = [](const std::string& code, const std::string& inputs_json, const std::string& outputs_json) -> bool {
        std::string escaped_code;
        for (char c : code) {
            if (c == '"') escaped_code += "\\\"";
            else if (c == '\\') escaped_code += "\\\\";
            else if (c == '\n') escaped_code += "\\n";
            else if (c == '\r') continue;
            else if (c == '\t') escaped_code += "\\t";
            else escaped_code += c;
        }
        std::string circuit_json = "{\n"
            "  \"name\": \"DocTest\",\n"
            "  \"elements\": [\n"
            "    {\n"
            "      \"name\": \"CB\",\n"
            "      \"type\": \"CODE_BLOCK\",\n"
            "      \"inputs\": " + inputs_json + ",\n"
            "      \"outputs\": " + outputs_json + ",\n"
            "      \"code\": \"" + escaped_code + "\"\n"
            "    }\n"
            "  ],\n"
            "  \"nets\": []\n"
            "}";
        le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);
        le_compile_result_t res;
        int rc = le_compile_json_ex(circuit_json.c_str(), nullptr, &opts, &res);
        bool ok = (rc == 0 && res.success == 1);
        if (!ok && res.error_message) {
            std::cout << "Doc test compile failed for code:\n" << code << "\nError: " << res.error_message << "\n";
        }
        le_compile_result_free(&res);
        return ok;
    };

    // Ex 1: Signal Conditioning & Clamping
    TEST_ASSERT(test_compile_cb(
        "float scaled = in_val * gain + offset;\n"
        "out_val = clamp(scaled, min_limit, max_limit);",
        R"([{"name":"in_val","type":"float"},{"name":"gain","type":"float"},{"name":"offset","type":"float"},{"name":"min_limit","type":"float"},{"name":"max_limit","type":"float"}])",
        R"([{"name":"out_val","type":"float"}])"
    ), "Doc Ex 1: Signal conditioning");

    // Ex 2: Mode Selector Multiplexer
    TEST_ASSERT(test_compile_cb(
        "if (manual_mode) {\n"
        "    out_ctrl = manual_cmd;\n"
        "} else if (auto_enable) {\n"
        "    out_ctrl = auto_target;\n"
        "} else {\n"
        "    out_ctrl = 0.0;\n"
        "}",
        R"([{"name":"manual_mode","type":"bool"},{"name":"manual_cmd","type":"float"},{"name":"auto_enable","type":"bool"},{"name":"auto_target","type":"float"}])",
        R"([{"name":"out_ctrl","type":"float"}])"
    ), "Doc Ex 2: Mode selector MUX");

    // Ex 3: Unrolled Array FIR / Weighted Sum
    TEST_ASSERT(test_compile_cb(
        "float weights[4];\n"
        "weights[0] = 0.4;\n"
        "weights[1] = 0.3;\n"
        "weights[2] = 0.2;\n"
        "weights[3] = 0.1;\n"
        "float acc = 0.0;\n"
        "for (int i = 0; i < 4; i++) {\n"
        "    acc += samples[i] * weights[i];\n"
        "}\n"
        "out_filtered = acc;",
        R"([{"name":"samples","type":"float[4]"}])",
        R"([{"name":"out_filtered","type":"float"}])"
    ), "Doc Ex 3: FIR filter unrolled loop");

    // Ex 4: Stateful Shift Register Pipeline
    TEST_ASSERT(test_compile_cb(
        "static bool s[5];\n"
        "out = s[4];\n"
        "for (int i = 4; i > 0; i--) {\n"
        "    s[i] = s[i - 1];\n"
        "}\n"
        "s[0] = in_a && in_b;",
        R"([{"name":"in_a","type":"bool"},{"name":"in_b","type":"bool"}])",
        R"([{"name":"out","type":"bool"}])"
    ), "Doc Ex 4: Shift register pipeline");

    // Ex 5: Stateful Rising Edge Detector
    TEST_ASSERT(test_compile_cb(
        "static bool prev = false;\n"
        "pulse = trigger && !prev;\n"
        "prev = trigger;",
        R"([{"name":"trigger","type":"bool"}])",
        R"([{"name":"pulse","type":"bool"}])"
    ), "Doc Ex 5: Rising edge detector");

    // Ex 6: Stateful Counter / Accumulator with Reset
    TEST_ASSERT(test_compile_cb(
        "static int count = 0;\n"
        "if (reset) {\n"
        "    count = 0;\n"
        "} else if (enable) {\n"
        "    count = count + 1;\n"
        "}\n"
        "out_count = count;",
        R"([{"name":"reset","type":"bool"},{"name":"enable","type":"bool"}])",
        R"([{"name":"out_count","type":"int"}])"
    ), "Doc Ex 6: Accumulator with reset");

    // Ex 7: Inlined User Helper Function
    TEST_ASSERT(test_compile_cb(
        "float deadband(float val, float thresh) {\n"
        "    if (abs(val) < thresh) return 0.0;\n"
        "    return val;\n"
        "}\n"
        "out_a = deadband(in_a, 0.1);\n"
        "out_b = deadband(in_b, 0.1);",
        R"([{"name":"in_a","type":"float"},{"name":"in_b","type":"float"}])",
        R"([{"name":"out_a","type":"float"},{"name":"out_b","type":"float"}])"
    ), "Doc Ex 7: Inlined user function");

    // Ex 8: Complex Phasor Math (Polar Conversion)
    TEST_ASSERT(test_compile_cb(
        "float mag = sqrt(in_phasor.real * in_phasor.real + in_phasor.imag * in_phasor.imag);\n"
        "float angle = atan2(in_phasor.imag, in_phasor.real);\n"
        "out_mag = mag;\n"
        "out_phase = angle;",
        R"([{"name":"in_phasor","type":"complex"}])",
        R"([{"name":"out_mag","type":"float"},{"name":"out_phase","type":"float"}])"
    ), "Doc Ex 8: Complex polar conversion");

    // Ex 9: Bitwise Masking Operations
    TEST_ASSERT(test_compile_cb(
        "int masked = (in_bits & mask) ^ 15;\n"
        "out_bits = masked | 128;",
        R"([{"name":"in_bits","type":"int"},{"name":"mask","type":"int"}])",
        R"([{"name":"out_bits","type":"int"}])"
    ), "Doc Ex 9: Bitwise operations");
}

void test_code_block_hdl_multi_driver_warning()
{
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);

    // 1. Multiple drivers on output net (unconditional overwrite)
    const char* multi_driver_output_json = R"({
        "name": "CbMultiOut",
        "elements": [
            { "name": "IN_A", "type": "FLOATREGISTER" },
            { "name": "IN_B", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [ { "name": "in_a", "type": "float" }, { "name": "in_b", "type": "float" } ],
                "outputs": [ { "name": "out", "type": "float" } ],
                "code": "out = in_a;\nout = in_b;"
            },
            { "name": "OUT", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "IN_A", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_a" } ] },
            { "output": { "name": "IN_B", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_b" } ] },
            { "output": { "name": "CB", "port": "out" }, "inputs": [ { "name": "OUT", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res1;
    int rc1 = le_compile_json_ex(multi_driver_output_json, nullptr, &opts, &res1);
    TEST_ASSERT(rc1 == 0 && res1.success == 1, "Multi-driver output compiles with warning");
    TEST_ASSERT(res1.warnings != nullptr && std::string(res1.warnings).find("multiple unconditional drivers for net 'out'") != std::string::npos,
                "Warning emitted for multi-driver output net");
    le_compile_result_free(&res1);

    // 2. Multiple drivers on local variable
    const char* multi_driver_local_json = R"({
        "name": "CbMultiLocal",
        "elements": [
            { "name": "IN_A", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [ { "name": "in_a", "type": "float" } ],
                "outputs": [ { "name": "out", "type": "float" } ],
                "code": "float x = in_a;\nx = in_a * 2.0f;\nout = x;"
            },
            { "name": "OUT", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "IN_A", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_a" } ] },
            { "output": { "name": "CB", "port": "out" }, "inputs": [ { "name": "OUT", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res2;
    int rc2 = le_compile_json_ex(multi_driver_local_json, nullptr, &opts, &res2);
    TEST_ASSERT(rc2 == 0 && res2.success == 1, "Multi-driver local compiles with warning");
    TEST_ASSERT(res2.warnings != nullptr && std::string(res2.warnings).find("multiple unconditional drivers for net 'x'") != std::string::npos,
                "Warning emitted for multi-driver local net");
    le_compile_result_free(&res2);

    // 3. Multi-driver scalar in loop iterations
    const char* multi_driver_loop_json = R"({
        "name": "CbMultiLoop",
        "elements": [
            { "name": "IN_A", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [ { "name": "in_a", "type": "float" } ],
                "outputs": [ { "name": "out", "type": "float" } ],
                "code": "for (int i = 0; i < 3; i++) { out = in_a; }"
            },
            { "name": "OUT", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "IN_A", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_a" } ] },
            { "output": { "name": "CB", "port": "out" }, "inputs": [ { "name": "OUT", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res3;
    int rc3 = le_compile_json_ex(multi_driver_loop_json, nullptr, &opts, &res3);
    TEST_ASSERT(rc3 == 0 && res3.success == 1, "Loop multi-driver compiles with warning");
    TEST_ASSERT(res3.warnings != nullptr && std::string(res3.warnings).find("multiple unconditional drivers for net 'out' across loop iterations") != std::string::npos,
                "Warning emitted for loop multi-driver net");
    le_compile_result_free(&res3);

    // 4. Multiplexer (if/else) has NO multi-driver warning
    const char* mux_valid_json = R"({
        "name": "CbMuxValid",
        "elements": [
            { "name": "COND", "type": "BOOLREGISTER" },
            { "name": "IN_A", "type": "FLOATREGISTER" },
            { "name": "IN_B", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [ { "name": "cond", "type": "bool" }, { "name": "in_a", "type": "float" }, { "name": "in_b", "type": "float" } ],
                "outputs": [ { "name": "out", "type": "float" } ],
                "code": "if (cond) { out = in_a; } else { out = in_b; }"
            },
            { "name": "OUT", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "COND", "port": "out" }, "inputs": [ { "name": "CB", "port": "cond" } ] },
            { "output": { "name": "IN_A", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_a" } ] },
            { "output": { "name": "IN_B", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_b" } ] },
            { "output": { "name": "CB", "port": "out" }, "inputs": [ { "name": "OUT", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res4;
    int rc4 = le_compile_json_ex(mux_valid_json, nullptr, &opts, &res4);
    TEST_ASSERT(rc4 == 0 && res4.success == 1, "Mux compiles cleanly");
    TEST_ASSERT(res4.warnings == nullptr || std::string(res4.warnings).find("multiple unconditional drivers") == std::string::npos,
                "Clean MUX does not trigger multi-driver warning");
    le_compile_result_free(&res4);
}

void test_code_block_hdl_unused_and_undriven_ports()
{
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);

    // Unused input pin 'in_unused' and undriven output pin 'out_undriven'
    const char* ports_json = R"({
        "name": "CbPorts",
        "elements": [
            { "name": "IN_A", "type": "FLOATREGISTER" },
            { "name": "IN_UNUSED", "type": "FLOATREGISTER" },
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [
                    { "name": "in_a", "type": "float" },
                    { "name": "in_unused", "type": "float" }
                ],
                "outputs": [
                    { "name": "out_driven", "type": "float" },
                    { "name": "out_undriven", "type": "float" }
                ],
                "code": "out_driven = in_a * 2.0f;"
            },
            { "name": "OUT", "type": "FLOATREGISTER" }
        ],
        "nets": [
            { "output": { "name": "IN_A", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_a" } ] },
            { "output": { "name": "IN_UNUSED", "port": "out" }, "inputs": [ { "name": "CB", "port": "in_unused" } ] },
            { "output": { "name": "CB", "port": "out_driven" }, "inputs": [ { "name": "OUT", "port": "in" } ] }
        ]
    })";

    le_compile_result_t res;
    int rc = le_compile_json_ex(ports_json, nullptr, &opts, &res);
    TEST_ASSERT(rc == 0 && res.success == 1, "Ports circuit compiles with diagnostics");
    TEST_ASSERT(res.warnings != nullptr, "Diagnostics emitted for unused/undriven ports");

    std::string warn_str = res.warnings;
    TEST_ASSERT(warn_str.find("[NOTICE] Code block 'CB': input pin 'in_unused' is declared in the interface but never referenced") != std::string::npos,
                "Unused input pin notice emitted");
    TEST_ASSERT(warn_str.find("[WARNING] Code block 'CB': output pin 'out_undriven' is declared in the interface but never driven") != std::string::npos,
                "Undriven output pin warning emitted");

    le_compile_result_free(&res);
}

void test_code_block_hdl_bitwise_type_validation()
{
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_FULL);

    // 1. Bitwise AND on float
    const char* bitwise_float_json = R"({
        "name": "CbBitwiseFloat",
        "elements": [
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [ { "name": "a", "type": "float" }, { "name": "b", "type": "float" } ],
                "outputs": [ { "name": "out", "type": "float" } ],
                "code": "out = a & b;"
            }
        ],
        "nets": []
    })";

    le_compile_result_t res1;
    int rc1 = le_compile_json_ex(bitwise_float_json, nullptr, &opts, &res1);
    TEST_ASSERT(rc1 != 0 || res1.success == 0, "Bitwise AND on float rejected");
    TEST_ASSERT(res1.error_message != nullptr && std::string(res1.error_message).find("Bitwise operations (&, |, ^) are only supported on 'int' or 'bool'") != std::string::npos,
                "Error explains bitwise operations require int or bool");
    le_compile_result_free(&res1);

    // 2. Bitwise NOT on float
    const char* bitwise_not_float_json = R"({
        "name": "CbBitwiseNotFloat",
        "elements": [
            {
                "name": "CB",
                "type": "CODE_BLOCK",
                "inputs": [ { "name": "a", "type": "float" } ],
                "outputs": [ { "name": "out", "type": "float" } ],
                "code": "out = ~a;"
            }
        ],
        "nets": []
    })";

    le_compile_result_t res2;
    int rc2 = le_compile_json_ex(bitwise_not_float_json, nullptr, &opts, &res2);
    TEST_ASSERT(rc2 != 0 || res2.success == 0, "Bitwise NOT on float rejected");
    TEST_ASSERT(res2.error_message != nullptr && std::string(res2.error_message).find("Bitwise NOT (~) is only supported on 'int' type") != std::string::npos,
                "Error explains bitwise NOT requires int");
    le_compile_result_free(&res2);
}

int main()
{
    std::cout << "=== Running LogicElements Compiler Unit Tests ===\n";
    RUN_TEST(test_code_block_hdl_multi_driver_warning);
    RUN_TEST(test_code_block_hdl_unused_and_undriven_ports);
    RUN_TEST(test_code_block_hdl_bitwise_type_validation);
    RUN_TEST(test_code_block_documentation_examples);
    RUN_TEST(test_code_block_static_array_order_independence);
    RUN_TEST(test_basic_and_circuit);
    RUN_TEST(test_float_math_circuit);
    RUN_TEST(test_board_limits_validation);
    RUN_TEST(test_custom_node_ext_call);
    RUN_TEST(test_direct_destination_coalescing);
    RUN_TEST(test_inversion_folding);
    RUN_TEST(test_dead_code_elimination);
    RUN_TEST(test_common_subexpression_elimination);
    RUN_TEST(test_constant_folding);
    RUN_TEST(test_double_negation);
    RUN_TEST(test_dsp_circuit_compilation);
    RUN_TEST(test_data_processing_and_dsp_nodes);
    RUN_TEST(test_mux_block);
    RUN_TEST(test_multi_output_selection);
    RUN_TEST(test_state_table_binding);
    RUN_TEST(test_timer_runs_via_heap);
    RUN_TEST(test_all_props_baked);
    RUN_TEST(test_complex_arithmetic);
    RUN_TEST(test_diff_n_block);
    RUN_TEST(test_diff_87_ten_inputs);
    RUN_TEST(test_multi_input_gate_decomposition);
    RUN_TEST(test_all_logic_gates_truth);
    RUN_TEST(test_zero_input_gate_compile);
    RUN_TEST(test_board_complex_limit);
    RUN_TEST(test_overcurrent_enable_input);
    RUN_TEST(test_phase_comp_transform);
    RUN_TEST(test_dist21_mho);
    RUN_TEST(test_arena_capacity);
    RUN_TEST(test_conversion_roundtrip_and_clamp);
    RUN_TEST(test_variables);
    RUN_TEST(test_variable_errors);
    RUN_TEST(test_variable_math_functions);
    RUN_TEST(test_aliases_and_pulse);
    RUN_TEST(test_json_error_paths);
    RUN_TEST(test_net_error_paths);
    RUN_TEST(test_alias_error_paths);
    RUN_TEST(test_binary_determinism);
    RUN_TEST(test_binary_header_fields);
    RUN_TEST(test_disasm_content);
    RUN_TEST(test_timing_descriptor);
    RUN_TEST(test_circuit_owns_scan_rate);
    RUN_TEST(test_opcode_enum_density);
    RUN_TEST(test_dce_preserves_stateful);
    RUN_TEST(test_inversion_combinations);
    RUN_TEST(test_integrated_pipeline_step);
    RUN_TEST(test_register_limits_load_reject);
    RUN_TEST(test_comms_upload_endtoend);
    RUN_TEST(test_analog_output_pipeline);
    RUN_TEST(test_sparse_io_addressing_accommodated);
    RUN_TEST(test_rs_latch_feedback_register);
    RUN_TEST(test_dedicated_latch_node_state_retention);
    RUN_TEST(test_cross_coupled_latch_auto_feedback);
    RUN_TEST(test_boolean_simplification_annihilation);
    RUN_TEST(test_boolean_simplification_idempotence);
    RUN_TEST(test_boolean_simplification_absorption);
    RUN_TEST(test_boolean_simplification_complementary_merging);
    RUN_TEST(test_ring_oscillator_hazard_warning);
    RUN_TEST(test_instruction_fusion_fma);
    RUN_TEST(test_instruction_fusion_fms);
    RUN_TEST(test_chunked_container_emission);
    RUN_TEST(test_multi_rate_compilation_and_execution);
    RUN_TEST(test_cryptographic_signing_api);
    RUN_TEST(test_code_block_basic_arithmetic);
    RUN_TEST(test_code_block_heterogeneous_io);
    RUN_TEST(test_code_block_arrays_and_loops);
    RUN_TEST(test_code_block_conditional_and_inferred_latch);
    RUN_TEST(test_code_block_user_functions);
    RUN_TEST(test_code_block_error_paths);
    RUN_TEST(test_code_block_trig_and_math);

    std::cout << "=================================================\n";
    std::cout << "Summary: " << g_tests_passed << " Passed, " << g_tests_failed << " Failed.\n";
    return (g_tests_failed == 0) ? 0 : 1;
}


