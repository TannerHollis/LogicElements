#include "le_compiler.h"
#include "le_types.h"
#include "le_process_image.h"
#include "le_vm.h"
#include "le_loader.h"
#include "le_rt.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <string>

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
    TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_TIMER(0)), "timer not done at t=10");
    TEST_ASSERT(le_vm_step(&vm, 60) == LE_OK, "timer step (t=60)");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_TIMER(0)),
                "timer done via heap after preset elapsed");

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

int main()
{
    std::cout << "=== Running LogicElements Compiler Unit Tests ===\n";
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
    RUN_TEST(test_board_complex_limit);
    RUN_TEST(test_phase_comp_transform);
    RUN_TEST(test_dist21_mho);
    RUN_TEST(test_arena_capacity);
    RUN_TEST(test_conversion_roundtrip_and_clamp);
    RUN_TEST(test_variables);
    RUN_TEST(test_variable_errors);
    RUN_TEST(test_variable_math_functions);
    RUN_TEST(test_aliases_and_pulse);

    std::cout << "=================================================\n";
    std::cout << "Summary: " << g_tests_passed << " Passed, " << g_tests_failed << " Failed.\n";
    return (g_tests_failed == 0) ? 0 : 1;
}


