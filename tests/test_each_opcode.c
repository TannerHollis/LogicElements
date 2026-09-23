/**
 * @file test_each_opcode.c
 * @brief Per-element compiler verification harness for LogicElements.
 *
 * This test compiles one minimal circuit per runtime element type through the
 * canonical compiler C API at -O0 (the 1:1 schematic-to-bytecode mapping),
 * prints the compiler's disassembled circuit breakdown, loads the resulting
 * bytecode into the virtual machine and verifies the runtime behavior.
 *
 * The purpose is to expose every place where the compiler's
 * element-to-bytecode mapping does not match what the runtime actually
 * executes (missing runtime handlers, dropped element parameters, operands
 * left unconnected, ...).
 *
 * Every element receives a three-part verdict:
 *   COMPILE  - le_compile_json_ex succeeded
 *   DISASM   - the disassembly contains the expected opcode(s)
 *   RUNTIME  - the VM executed the bytecode and produced the documented output
 *
 * Exit code: 0 when every check passes, 1 otherwise.
 *
 * NOTE: protection / serial-bus / DSP checks rely on the runtime defaults
 * (LE_ENABLE_PROTECTION, LE_ENABLE_SERIAL_BUS, LE_ENABLE_DSP all = 1).
 */

#include "le_compiler.h"
#include "le_types.h"
#include "le_process_image.h"
#include "le_vm.h"
#include "le_loader.h"
#include "le_hal.h"
#include "le_complex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Provided by src/hal/le_hal_sim.c */
const le_hal_t* le_hal_get_sim(void);

/* ------------------------------------------------------------------ */
/* Test bookkeeping                                                    */
/* ------------------------------------------------------------------ */

int g_tests_passed = 0;
int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("    [FAIL] %s\n", msg); \
            g_tests_failed++; \
        } else { \
            g_tests_passed++; \
        } \
    } while (0)

#define TEST_ASSERT_NEAR(actual, expected, tol, msg) \
    TEST_ASSERT((fabsf((actual) - (expected)) <= (tol)), msg)

/* ------------------------------------------------------------------ */
/* Element verdict table                                               */
/* ------------------------------------------------------------------ */

#define VERDICT_SKIP (-1)
#define VERDICT_FAIL (0)
#define VERDICT_PASS (1)

typedef struct {
    const char* name;
    int compile;
    int disasm;
    int runtime;
} element_result_t;

static element_result_t g_results[128];
static int g_result_count = 0;
static int g_case_number = 0;

static void record_result(const char* name, int compile, int disasm, int runtime)
{
    if (g_result_count < (int)(sizeof(g_results) / sizeof(g_results[0]))) {
        g_results[g_result_count].name = name;
        g_results[g_result_count].compile = compile;
        g_results[g_result_count].disasm = disasm;
        g_results[g_result_count].runtime = runtime;
        g_result_count++;
    }
}

static const char* verdict_str(int v)
{
    if (v == VERDICT_PASS) return "ok";
    if (v == VERDICT_FAIL) return "FAIL";
    return "skipped";
}

static const char* status_name(le_status_t s)
{
    switch (s) {
        case LE_OK: return "LE_OK";
        case LE_ERR_NULL_PTR: return "LE_ERR_NULL_PTR";
        case LE_ERR_INVALID_MAGIC: return "LE_ERR_INVALID_MAGIC";
        case LE_ERR_INVALID_VERSION: return "LE_ERR_INVALID_VERSION";
        case LE_ERR_CRC_MISMATCH: return "LE_ERR_CRC_MISMATCH";
        case LE_ERR_CAPACITY: return "LE_ERR_CAPACITY";
        case LE_ERR_OUT_OF_BOUNDS: return "LE_ERR_OUT_OF_BOUNDS";
        case LE_ERR_UNKNOWN_OPCODE: return "LE_ERR_UNKNOWN_OPCODE";
        default: return "UNKNOWN";
    }
}

/* ------------------------------------------------------------------ */
/* Core per-element driver                                             */
/* ------------------------------------------------------------------ */

typedef void (*behavior_fn)(le_vm_t* vm);

typedef struct {
    const char* title;              /* Element type under test */
    const char* json;               /* Minimal circuit exercising the element */
    const char* const* mnemonics;   /* NULL-terminated list of expected opcodes */
    behavior_fn behavior;           /* VM behavior verification (NULL = none) */
    const char* note;               /* Optional context printed under the title */
    const char* board_json;         /* Optional board profile (NULL = none) */
} element_case_t;

/*
 * The disassembler emits instruction lines as:
 *   [   0]  MNEMONIC         <in_a>         <in_b>         -> <out>
 * i.e. the mnemonic starts at column 8 and occupies 14 columns.
 */
static int disasm_has_insn(const char* disasm, const char* mnemonic)
{
    const char* line;
    char tok[16];

    if (!disasm || !mnemonic) return 0;
    line = disasm;
    while ((line = strchr(line, '\n')) != NULL) {
        line++;
        if (line[0] == '[' && strlen(line) > (size_t)(8 + 14)) {
            memcpy(tok, line + 8, 14);
            tok[14] = '\0';
            while (strlen(tok) > 0 && tok[strlen(tok) - 1] == ' ') {
                tok[strlen(tok) - 1] = '\0';
            }
            if (strcmp(tok, mnemonic) == 0) return 1;
        }
    }
    return 0;
}

/*
 * Compiles one element circuit, prints the disassembled breakdown and
 * verifies compile / disassembly / runtime behavior.
 */
static void run_element(const element_case_t* tc)
{
    le_compiler_options_t opts = le_compiler_options_init(LE_OPT_NONE);
    le_compile_result_t res;
    int disasm_ok;
    int runtime_ok = VERDICT_SKIP;
    int overall_ok;

    g_case_number++;
    printf("\n------------------------------------------------------------------------------\n");
    printf("[%2d] ELEMENT: %s\n", g_case_number, tc->title);
    if (tc->note) {
        printf("     %s\n", tc->note);
    }
    printf("     JSON: %s\n", tc->json);

    memset(&res, 0, sizeof(res));
    {
        int rc = le_compile_json_ex(tc->json, tc->board_json, &opts, &res);
        int compile_ok = (rc == 0 && res.success == 1);

        if (!compile_ok) {
            printf("    [FAIL] Compilation failed (rc=%d)", rc);
            if (res.error_message) {
                printf(" -- %s", res.error_message);
            }
            printf("\n");
            g_tests_failed++;
            record_result(tc->title, VERDICT_FAIL, VERDICT_FAIL, VERDICT_SKIP);
            le_compile_result_free(&res);
            return;
        }
        g_tests_passed++;
        printf("     Compiled OK: %d instruction(s) | DIN:%d DOUT:%d BOOL:%d FLOAT:%d INT:%d TIMER:%d CTR:%d\n",
               res.instruction_count, res.din_count, res.dout_count, res.bool_reg_count,
               res.float_count, res.int_count, res.timer_count, res.counter_count);
    }

    printf("     --- Compiler breakdown (disassembly, -O0 1:1 mapping) ---\n");
    if (res.disassembly_text) {
        const char* line = res.disassembly_text;
        while (*line) {
            const char* eol = strchr(line, '\n');
            if (!eol) eol = line + strlen(line);
            printf("     | %.*s\n", (int)(eol - line), line);
            if (!*eol) break;
            line = eol + 1;
        }
    } else {
        printf("     | (no disassembly text)\n");
    }

    disasm_ok = 1;
    if (tc->mnemonics) {
        int i;
        for (i = 0; tc->mnemonics[i] != NULL; i++) {
            if (disasm_has_insn(res.disassembly_text, tc->mnemonics[i])) {
                g_tests_passed++;
            } else {
                disasm_ok = 0;
                g_tests_failed++;
                printf("    [FAIL] Disassembly is missing expected opcode: %s\n", tc->mnemonics[i]);
            }
        }
    }

    if (tc->behavior) {
        le_vm_t vm;
        if (le_vm_init(&vm) != LE_OK) {
            g_tests_failed++;
            printf("    [FAIL] le_vm_init failed\n");
            runtime_ok = VERDICT_FAIL;
        } else {
            le_status_t ls = le_loader_load(&vm, res.binary_data, res.binary_size);
            if (ls != LE_OK) {
                g_tests_failed++;
                printf("    [FAIL] le_loader_load failed: %s\n", status_name(ls));
                runtime_ok = VERDICT_FAIL;
            } else {
                int before = g_tests_failed;
                tc->behavior(&vm);
                runtime_ok = (g_tests_failed == before) ? VERDICT_PASS : VERDICT_FAIL;
            }
        }
    }

    overall_ok = (disasm_ok && runtime_ok != VERDICT_FAIL);
    printf("     => %s (compile=ok disasm=%s runtime=%s)\n",
           overall_ok ? "PASS" : "FAIL",
           verdict_str(disasm_ok ? VERDICT_PASS : VERDICT_FAIL),
           verdict_str(runtime_ok));

    record_result(tc->title, VERDICT_PASS,
                  disasm_ok ? VERDICT_PASS : VERDICT_FAIL, runtime_ok);
    le_compile_result_free(&res);
}

/* ------------------------------------------------------------------ */
/* Behavior checks - I/O                                               */
/* ------------------------------------------------------------------ */

static void behavior_din(le_vm_t* vm)
{
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), true);
    le_status_t st = le_vm_step(vm, 10);
    TEST_ASSERT(st == LE_OK, "DIGITALINPUT: le_vm_step failed (0 instructions expected)");
    if (st == LE_OK) {
        TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DIN(0)),
                    "DIGITALINPUT: DIN[0] readable after step");
    }
}

static void behavior_dout(le_vm_t* vm)
{
    uint32_t t = 10;
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), true);
    if (le_vm_step(vm, t) != LE_OK) { TEST_ASSERT(0, "DIGITALOUTPUT: le_vm_step failed"); return; }
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "DIGITALOUTPUT: DOUT[0] should follow DIN[0] (true)");
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), false);
    if (le_vm_step(vm, t += 10) != LE_OK) { TEST_ASSERT(0, "DIGITALOUTPUT: le_vm_step failed"); return; }
    TEST_ASSERT(!le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "DIGITALOUTPUT: DOUT[0] should follow DIN[0] (false)");
}

static void behavior_ain_raw(le_vm_t* vm)
{
    le_process_image_set_int(&vm->image, LE_ADDR_MAKE_AIN(0), 10);
    le_process_image_set_float(&vm->image, LE_ADDR_MAKE_FLOAT(0), 2.0f); /* %R1 multiplier */
    if (le_vm_step(vm, 10) != LE_OK) { TEST_ASSERT(0, "ANALOGINPUT(raw): le_vm_step failed"); return; }
    float out = le_process_image_get_float(&vm->image, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT_NEAR(out, 20.0f, 0.01f, "ANALOGINPUT(raw): AIN[0]=10 * %R1=2.0 should give 20.0");
}

static void behavior_ain_scaled(le_vm_t* vm)
{
    /* The compiler writes scaler parameters only into the generated C header,
       never into the .lebin binary - the host application must program the
       scaler state (as the generated header does). We do the same here. */
    le_process_image_set_scaler(&vm->image, 0, 0.0f, 4095.0f, 0.0f, 100.0f, true);
    le_process_image_set_int(&vm->image, LE_ADDR_MAKE_AIN(0), 2047);
    if (le_vm_step(vm, 10) != LE_OK) { TEST_ASSERT(0, "ANALOGINPUT(float): le_vm_step failed"); return; }
    float out = le_process_image_get_float(&vm->image, LE_ADDR_MAKE_FLOAT(0));
    TEST_ASSERT_NEAR(out, 49.99f, 0.05f, "ANALOGINPUT(float): raw 2047 of 0..4095 should scale to ~49.99");
}

static void behavior_tag(le_vm_t* vm)
{
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), true);
    if (le_vm_step(vm, 10) != LE_OK) { TEST_ASSERT(0, "TAG: le_vm_step failed"); return; }
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "TAG: receiver output should follow the sender's DIN[0]");
}

/* ------------------------------------------------------------------ */
/* Behavior checks - storage registers                                 */
/* ------------------------------------------------------------------ */

static void behavior_boolreg(le_vm_t* vm)
{
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), true);
    if (le_vm_step(vm, 10) != LE_OK) { TEST_ASSERT(0, "BOOLREGISTER: le_vm_step failed"); return; }
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_BOOL_REG(0)),
                "BOOLREGISTER: %M0 should hold DIN[0]");
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "BOOLREGISTER: DOUT[0] should follow %M0");
}

static void behavior_floatreg(le_vm_t* vm)
{
    if (le_vm_step(vm, 10) != LE_OK) { TEST_ASSERT(0, "FLOATREGISTER: le_vm_step failed"); return; }
    float v = le_process_image_get_float(&vm->image, LE_ADDR_MAKE_FLOAT(0));
    TEST_ASSERT_NEAR(v, 2.0f, 0.01f, "FLOATREGISTER: 1.0 + 1.0 should store 2.0 in %R0");
}

static void behavior_intreg(le_vm_t* vm)
{
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), true);
    le_status_t st = le_vm_step(vm, 10);
    TEST_ASSERT(st == LE_OK, "INTREGISTER: le_vm_step failed");
    if (st == LE_OK) {
        int32_t v = le_process_image_get_int(&vm->image, LE_ADDR_MAKE_INT_REG(0));
        TEST_ASSERT(v == 1,
            "INTREGISTER: compiler emits a bool MOVE into the INT region, but set_bool() is a no-op there - the register never updates");
    }
}

/* ------------------------------------------------------------------ */
/* Behavior checks - logic gates                                       */
/* ------------------------------------------------------------------ */

typedef bool (*gate_fn)(bool a, bool b);

static void behavior_gate2(le_vm_t* vm, gate_fn eval)
{
    static const struct { bool a; bool b; } cases[4] = {
        { false, false }, { false, true }, { true, false }, { true, true }
    };
    int i;
    uint32_t t = 10;
    for (i = 0; i < 4; i++) {
        le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), cases[i].a);
        le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(1), cases[i].b);
        t += 10;
        if (le_vm_step(vm, t) != LE_OK) {
            TEST_ASSERT(0, "GATE: le_vm_step failed");
            return;
        }
        bool got = le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0));
        char msg[96];
        snprintf(msg, sizeof(msg), "GATE: truth table case (%d,%d) expected %d got %d",
                 (int)cases[i].a, (int)cases[i].b, (int)eval(cases[i].a, cases[i].b), (int)got);
        TEST_ASSERT(got == eval(cases[i].a, cases[i].b), msg);
    }
}

static bool gate_eval_and (bool a, bool b) { return a && b; }
static bool gate_eval_or  (bool a, bool b) { return a || b; }
static bool gate_eval_xor (bool a, bool b) { return a != b; }
static bool gate_eval_nand(bool a, bool b) { return !(a && b); }
static bool gate_eval_nor (bool a, bool b) { return !(a || b); }
static void behavior_gate_and(le_vm_t* vm)  { behavior_gate2(vm, gate_eval_and); }
static void behavior_gate_or (le_vm_t* vm)  { behavior_gate2(vm, gate_eval_or);  }
static void behavior_gate_xor(le_vm_t* vm)  { behavior_gate2(vm, gate_eval_xor); }
static void behavior_gate_nand(le_vm_t* vm) { behavior_gate2(vm, gate_eval_nand);}
static void behavior_gate_nor(le_vm_t* vm)  { behavior_gate2(vm, gate_eval_nor); }

static void behavior_not(le_vm_t* vm)
{
    uint32_t t = 10;
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), true);
    if (le_vm_step(vm, t) != LE_OK) { TEST_ASSERT(0, "NOT: le_vm_step failed"); return; }
    TEST_ASSERT(!le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "NOT: true should become false");
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), false);
    if (le_vm_step(vm, t += 10) != LE_OK) { TEST_ASSERT(0, "NOT: le_vm_step failed"); return; }
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "NOT: false should become true");
}

static void behavior_mux(le_vm_t* vm)
{
    uint32_t t = 10;
    /* sel=0 -> out must be in0 */
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), true);   /* in0  */
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(1), false);  /* in1  */
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(2), false);  /* sel  */
    if (le_vm_step(vm, t) != LE_OK) { TEST_ASSERT(0, "MUX: le_vm_step failed"); return; }
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "MUX: sel=0 must select in0 (true)");
    /* sel=1 -> out must be in1 */
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(1), true);
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(2), true);
    if (le_vm_step(vm, t += 10) != LE_OK) { TEST_ASSERT(0, "MUX: le_vm_step failed"); return; }
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "MUX: sel=1 must select in1 - the compiler drops the documented sel/in0/in1 ports and the runtime returns in_a verbatim");
}

/* ------------------------------------------------------------------ */
/* Behavior checks - edge triggers                                     */
/* ------------------------------------------------------------------ */

static void behavior_rtrig(le_vm_t* vm)
{
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), true);
    le_status_t st = le_vm_step(vm, 10);
    TEST_ASSERT(st == LE_OK, "RTRIG: le_vm_step failed");
    if (st != LE_OK) return;
    bool pulse = le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0));
    TEST_ASSERT(pulse,
                "RTRIG: rising edge must pulse for one scan - the documented 'clk' port is not mapped by the compiler (in_a compiled to a constant)");
    if (!pulse) return;
    le_vm_step(vm, 20);
    TEST_ASSERT(!le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "RTRIG: pulse must be exactly one scan wide");
}

static void behavior_ftrig(le_vm_t* vm)
{
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), true);
    le_status_t st = le_vm_step(vm, 10);
    TEST_ASSERT(st == LE_OK, "FTRIG: le_vm_step failed");
    if (st != LE_OK) return;
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), false);
    st = le_vm_step(vm, 20);
    TEST_ASSERT(st == LE_OK, "FTRIG: le_vm_step failed");
    if (st != LE_OK) return;
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "FTRIG: falling edge must pulse for one scan - the documented 'clk' port is not mapped by the compiler (in_a compiled to a constant)");
}

/* ------------------------------------------------------------------ */
/* Behavior checks - latches                                           */
/* ------------------------------------------------------------------ */

static void step_latch(le_vm_t* vm, bool s, bool r, uint32_t* t)
{
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(0), s);
    le_process_image_set_bool(&vm->image, LE_ADDR_MAKE_DIN(1), r);
    *t += 10;
    le_vm_step(vm, *t);
}

static void behavior_latch_reset_dominant(le_vm_t* vm)
{
    uint32_t t = 0;
    step_latch(vm, true, false, &t);
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "LATCH: S=1 R=0 must set Q");
    step_latch(vm, false, false, &t);
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "LATCH: Q must hold");
    step_latch(vm, true, true, &t);
    TEST_ASSERT(!le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "LATCH: S=1 R=1 must reset (reset dominant)");
    step_latch(vm, false, false, &t);
    TEST_ASSERT(!le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "LATCH: Q=0 must hold");
}

static void behavior_latch_set_dominant(le_vm_t* vm)
{
    uint32_t t = 0;
    step_latch(vm, true, false, &t);
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "SR LATCH: S=1 R=0 must set Q");
    step_latch(vm, false, false, &t);
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "SR LATCH: Q must hold");
    step_latch(vm, true, true, &t);
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "SR LATCH: S=1 R=1 must set (set dominant)");
    step_latch(vm, false, false, &t);
    TEST_ASSERT(le_process_image_get_bool(&vm->image, LE_ADDR_MAKE_DOUT(0)),
                "SR LATCH: Q=1 must hold");
}

/* ------------------------------------------------------------------ */
/* Element case table                                                  */
/* ------------------------------------------------------------------ */
/* This table is grown two element types at a time. For each entry:
 *   json       - a minimal circuit exercising exactly one element type
 *   mnemonics  - opcodes that MUST appear in the disassembled breakdown
 *   behavior   - VM behavior verification (drives inputs, checks outputs)
 *   board_json - optional board profile (custom nodes only)
 */

static const char* const MNEM_MOVE[]   = { "MOVE", NULL };

static const char JSON_DIGITALINPUT[] =
    "{\"name\":\"E_DIN\","
    "\"elements\":[{\"name\":\"IN1\",\"type\":\"DIGITALINPUT\",\"address\":\"%I0\"}],"
    "\"nets\":[]}";

static const char JSON_DIGITALOUTPUT[] =
    "{\"name\":\"E_DOUT\","
    "\"elements\":["
    "  {\"name\":\"IN1\",\"type\":\"DIGITALINPUT\",\"address\":\"%I0\"},"
    "  {\"name\":\"OUT1\",\"type\":\"DIGITALOUTPUT\",\"address\":\"%Q0\"}"
    "],"
    "\"nets\":["
    "  {\"output\":{\"name\":\"IN1\",\"port\":\"out\"},\"inputs\":[{\"name\":\"OUT1\",\"port\":\"in\"}]}"
    "]}";

static const char JSON_TAG[] =
    "{\"name\":\"E_TAG\","
    "\"elements\":["
    "  {\"name\":\"IN1\",\"type\":\"DIGITALINPUT\",\"address\":\"%I0\"},"
    "  {\"name\":\"TS1\",\"type\":\"TAG_SEND\",\"direction\":\"send\",\"tag_name\":\"T1\"},"
    "  {\"name\":\"TR1\",\"type\":\"TAG_RECEIVE\",\"direction\":\"receive\",\"tag_name\":\"T1\"},"
    "  {\"name\":\"OUT1\",\"type\":\"DIGITALOUTPUT\",\"address\":\"%Q0\"}"
    "],"
    "\"nets\":["
    "  {\"output\":{\"name\":\"IN1\",\"port\":\"out\"},\"inputs\":[{\"name\":\"TS1\",\"port\":\"in\"}]},"
    "  {\"output\":{\"name\":\"TR1\",\"port\":\"out\"},\"inputs\":[{\"name\":\"OUT1\",\"port\":\"in\"}]}"
    "]}";


static const element_case_t g_cases[] = {
    /* ================= Batch 1: basic I/O ================= */
    {
        "DIGITALINPUT",
        JSON_DIGITALINPUT,
        NULL,                 /* no instructions expected */
        behavior_din,
        "Input element only: compiles to zero instructions; verifies the"
        "loader accepts an empty program and the VM steps cleanly.",
        NULL
    },
    {
        "DIGITALOUTPUT",
        JSON_DIGITALOUTPUT,
        MNEM_MOVE,
        behavior_dout,
        "Direct DIN[0] -> DOUT[0] path: exactly one MOVE instruction.",
        NULL
    },
    {
        "TAG_SEND",
        JSON_TAG,
        MNEM_MOVE,
        behavior_tag,
        "Tag pair: DIN[0] -> TAG_SEND(T1) -> TAG_RECEIVE(T1) -> DOUT[0]. The compiler"
        "resolves tags to the sender's source address at compile time (no runtime"
        "shared register); the breakdown must show exactly one MOVE DIN[0] -> DOUT[0].",
        NULL
    },
    {
        "TAG_RECEIVE",
        JSON_TAG,
        MNEM_MOVE,
        behavior_tag,
        "Tag pair (same circuit as TAG_SEND): verifies the receiver aliases the"
        "sender's source output address.",
        NULL
    },
};

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    int i;
    int issues = 0;

    printf("============================================================\n");
    printf(" LOGICELEMENTS PER-ELEMENT COMPILER TEST SUITE\n");
    printf(" Compiler: %s\n", le_compiler_get_version());
    printf("============================================================\n");
    printf(" Each element type is compiled (-O0, 1:1 schematic->bytecode\n");
    printf(" mapping), disassembled, loaded into the VM and\n");
    printf(" behavior-verified against the documented element spec.\n");

    le_hal_set(le_hal_get_sim());

    for (i = 0; i < (int)(sizeof(g_cases) / sizeof(g_cases[0])); i++) {
        run_element(&g_cases[i]);
    }

    printf("\n============================================================\n");
    printf(" SUMMARY (%d element types tested)\n", g_result_count);
    printf(" %-16s %-10s %-10s %-10s %s\n", "ELEMENT", "COMPILE", "DISASM", "RUNTIME", "VERDICT");
    printf(" ------------------------------------------------------------\n");
    for (i = 0; i < g_result_count; i++) {
        element_result_t* r = &g_results[i];
        int ok = (r->compile == VERDICT_PASS && r->disasm == VERDICT_PASS && r->runtime != VERDICT_FAIL);
        if (!ok) issues++;
        printf(" %-16s %-10s %-10s %-10s %s\n",
               r->name, verdict_str(r->compile), verdict_str(r->disasm),
               verdict_str(r->runtime), ok ? "PASS" : "FAIL");
    }
    printf("============================================================\n");
    printf(" CHECKS: %d passed, %d failed | ELEMENTS WITH ISSUES: %d/%d\n",
           g_tests_passed, g_tests_failed, issues, g_result_count);
    printf("============================================================\n");
    return (g_tests_failed == 0) ? 0 : 1;
}
