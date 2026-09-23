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
            "timers": 2,
            "counters": 2,
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
    TEST_ASSERT(vm.block_count == 1, "VM exposes one block descriptor");
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
    TEST_ASSERT(vm.block_count == 1, "one block descriptor");
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

    int tbase = le_rt_group_base(LE_BLK_TIMER);
    int lbase = le_rt_group_base(LE_BLK_LPF);
    TEST_ASSERT(tbase >= 0, "timer group has a bound base row");
    TEST_ASSERT(lbase >= 0, "LPF group has a bound base row");
    TEST_ASSERT(le_rt_row_count() >= 2, "at least 2 state rows bound");

    le_rt_block_t* t = le_rt_get(tbase);
    TEST_ASSERT(t != NULL && t->state != NULL, "timer row has heap state pointer");
    TEST_ASSERT(t->kind == LE_BLK_TIMER, "timer row kind is LE_BLK_TIMER");
    const uint8_t* heap = le_rt_heap();
    const uint8_t* st = t->state;
    TEST_ASSERT(st >= heap && (size_t)(st - heap) + t->state_size <= le_rt_capacity(),
                "timer state pointer lies within the zero-heap arena");

    // Per-element config injection: the LPF "alpha":0.5 must land on the heap block.
    le_lpf_state_t* lpfh = (le_lpf_state_t*)le_process_image_kind_state(&vm.image, LE_BLK_LPF, 0);
    TEST_ASSERT(lpfh != NULL && fabsf(lpfh->alpha - 0.5f) < 1e-5f,
                "LPF alpha=0.5 injected into the bound heap block");

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

    int base = le_rt_group_base(LE_BLK_TIMER);
    TEST_ASSERT(base >= 0, "timer bound to heap at load");
    le_rt_block_t* rb = le_rt_get(base);
    TEST_ASSERT(rb != NULL && rb->state != NULL, "timer row has heap state");
    TEST_ASSERT(((uint8_t*)&vm.image.timers[0]) != rb->state,
                "heap state pointer differs from the fixed array");

    // Configure the (heap) timer preset through the resolver.
    le_timer_state_t* ht = le_process_image_timer(&vm.image, 0);
    TEST_ASSERT(ht != NULL && ht == (le_timer_state_t*)rb->state, "resolver returns the heap timer");
    ht->preset_ms = 50;

    le_vm_start(&vm);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    TEST_ASSERT(le_vm_step(&vm, 10) == LE_OK, "timer step (t=10)");
    TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_TIMER(0)), "timer not done at t=10");
    TEST_ASSERT(le_vm_step(&vm, 60) == LE_OK, "timer step (t=60)");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_TIMER(0)),
                "timer done via heap after preset elapsed");
    TEST_ASSERT(!vm.image.timers[0].q,
                "fixed-array timer untouched - execution happened in the heap");

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

    std::cout << "=================================================\n";
    std::cout << "Summary: " << g_tests_passed << " Passed, " << g_tests_failed << " Failed.\n";
    return (g_tests_failed == 0) ? 0 : 1;
}


