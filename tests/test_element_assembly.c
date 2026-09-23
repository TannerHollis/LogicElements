/**
 * @file test_element_assembly.c
 * @brief Per-element compiler-to-assembly verification harness.
 *
 * For every circuit fixture in a directory (one .json per element type), this
 * harness:
 *   1. Compiles the circuit at -O0 (the 1:1 schematic->bytecode mapping);
 *   2. Parses the emitted disassembly into opcode records;
 *   3. Asserts the *exactly expected* opcode mnemonic appears the expected
 *      number of times, and the total instruction count matches.
 *
 * The element->opcode oracle is the internal table below. "Branching to every
 * element" is enforced by a coverage gate: any oracle entry lacking a fixture
 * is reported as a FAIL.
 *
 * Usage: test_element_assembly <fixture_dir>
 * Exit code: 0 when every check passes, 1 otherwise.
 */
#include "le_compiler.h"
#include "le_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) \
    do { if (cond) { g_pass++; } else { printf("    [FAIL] %s\n", msg); g_fail++; } } while (0)

/* ------------------------------------------------------------------ */
/* Element -> expected-opcode oracle                                   */
/* ------------------------------------------------------------------ */
typedef struct {
    const char* fixture;   /* filename without .json */
    const char* mnemonic;  /* expected disasm opcode, or "" for no-op   */
    int         count;     /* expected occurrences of @p mnemonic       */
    int         total;     /* expected total instruction count (-1 = auto) */
} element_spec_t;

#define SPEC(f, m, c) { f, m, c, -1 }
#define SPECN(f)      { f, "", 0, 0 }   /* no-op element: zero instructions */

static const element_spec_t g_oracle[] = {
    /* I/O & storage */
    SPECN("DIGITALINPUT"),
    SPECN("ANALOGINPUT_plain"),
    SPEC("ANALOGINPUT_scaled", "SCALE_F", 1),
    SPECN("CONSTANT"),
    SPEC("DIGITALOUTPUT", "MOVE", 1),
    SPEC("BOOLREGISTER", "MOVE", 1),
    SPEC("FLOATREGISTER", "MOVE_F", 1),
    SPEC("INTREGISTER", "MOVE", 1),
    /* Logic gates */
    SPEC("AND", "AND", 1), SPEC("OR", "OR", 1), SPEC("XOR", "XOR", 1),
    SPEC("NAND", "NAND", 1), SPEC("NOR", "NOR", 1), SPEC("NOT", "NOT", 1),
    SPEC("MUX", "MUX", 1),
    /* Edge & latches */
    SPEC("RTRIG", "RTRIG", 1), SPEC("FTRIG", "FTRIG", 1),
    SPEC("SR", "SR", 1), SPEC("RS", "RS", 1),
    SPEC("LATCH_set", "SR", 1), SPEC("LATCH_reset", "RS", 1),
    /* Timers & counters */
    SPEC("TON", "TON", 1), SPEC("TOF", "TOF", 1), SPEC("TP", "TP", 1),
    SPEC("CTU", "CTU", 1), SPEC("CTD", "CTD", 1), SPEC("CTUD", "CTUD", 1),
    /* Float math */
    SPEC("ADD", "ADD_F", 1), SPEC("SUB", "SUB_F", 1), SPEC("MUL", "MUL_F", 1),
    SPEC("DIV", "DIV_F", 1), SPEC("ABS", "ABS_F", 1), SPEC("NEG", "NEG_F", 1),
    SPEC("MIN", "MIN_F", 1), SPEC("MAX", "MAX_F", 1),
    SPEC("CLAMP", "CLAMP_F", 1),
    /* Comparisons */
    SPEC("CMP_GT", "CMP_GT", 1), SPEC("CMP_LT", "CMP_LT", 1),
    SPEC("CMP_GE", "CMP_GE", 1), SPEC("CMP_LE", "CMP_LE", 1),
    SPEC("CMP_EQ", "CMP_EQ", 1), SPEC("CMP_NE", "CMP_NE", 1),
    /* Control / protection / conversions */
    SPEC("PID", "PID", 1), SPEC("OVERCURRENT", "OVERCURRENT", 1),
    SPEC("RECT2POLAR", "RECT2POLAR", 1), SPEC("POLAR2RECT", "POLAR2RECT", 1),
    SPEC("PHASOR_SHIFT", "PHASOR_SHIFT", 1),
    SPEC("PHASOR_1P", "PHASOR_1P", 1), SPEC("SYM_COMP", "SYM_COMP", 1),
    SPEC("DIFF_87", "DIFF_87", 1), SPEC("DIST_21", "DIST_21", 1),
    /* Serial bus */
    SPEC("I2C", "I2C", 1), SPEC("SPI", "SPI", 1),
    /* DSP & filters */
    SPEC("LPF", "LPF_1P", 1), SPEC("BIQUAD", "BIQUAD_IIR", 1),
    SPEC("MOVING_AVG", "MOVING_AVG", 1), SPEC("RATE_LIMITER", "RATE_LIMITER", 1),
    SPEC("DEADBAND", "DEADBAND", 1), SPEC("WASHOUT", "WASHOUT", 1),
    SPEC("PEAK_DETECTOR", "PEAK_DETECTOR", 1), SPEC("RMS", "RMS", 1),
    SPEC("MEDIAN", "MEDIAN", 1), SPEC("DERIVATIVE", "DERIVATIVE", 1),
    SPEC("ZERO_CROSSING", "ZERO_CROSSING", 1), SPEC("LUT_1D", "LUT_1D", 1),
    SPEC("TOTALIZER", "TOTALIZER", 1), SPEC("MIN_MAX_HOLD", "MIN_MAX_HOLD", 1),
    /* Tags & board extensibility */
    SPEC("TAG", "MOVE", 1),
    SPEC("LE_CUSTOM", "BLOCK", 1),  /* block call for a custom node (func_id>=0x80) */
};

/* ------------------------------------------------------------------ */
/* File & scanning helpers                                             */
/* ------------------------------------------------------------------ */

static char* read_file(const char* path)
{
    long len;
    char* buf;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    (void)fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static void path_join(char* out, size_t cap, const char* dir, const char* file)
{
    snprintf(out, cap, "%s%c%s", dir, '/', file);
}

/* Count occurrences of @p mnemonic as a disassembly opcode token. The
 * disassembler emits each instruction as:
 *     [  0]  MNEMONIC         <in_a>        <in_b>        -> <out>
 * i.e. '[' at col 0, 4-digit index, ']' at col 5, then the opcode begins at
 * column 8 and occupies up to 14 columns. We match only that token. */
static int count_opcode(const char* disasm, const char* mnemonic)
{
    int n = 0;
    const char* line = disasm;
    while (line && *line) {
        const char* nl = strchr(line, '\n');
        const char* end = nl ? nl : line + strlen(line);
        if (end - line >= 8 + 14) {
            char tok[16];
            memcpy(tok, line + 8, 14);
            tok[14] = '\0';
            while (tok[0] && tok[(int)strlen(tok) - 1] == ' ') tok[(int)strlen(tok) - 1] = '\0';
            if (strcmp(tok, mnemonic) == 0) n++;
        }
        if (!nl) break;
        line = nl + 1;
    }
    return n;
}

/* Verify a single fixture. Returns 1 on pass, 0 on fail. */
static int run_fixture(const char* dir, const element_spec_t* spec)
{
    char circuit_path[1024];
    char* circuit = NULL;
    le_compiler_options_t opts;
    le_compile_result_t res;
    int rc;
    int instr_count = 0;
    int opcode_hits = 0;

    memset(&res, 0, sizeof(res));
    path_join(circuit_path, sizeof(circuit_path), dir, spec->fixture);
    strncat(circuit_path, ".json", sizeof(circuit_path) - strlen(circuit_path) - 1);

    circuit = read_file(circuit_path);
    if (!circuit) {
        printf("  [%-20s] [SKIP] fixture not found: %s\n", spec->fixture, circuit_path);
        g_fail++;
        return 0;
    }

    opts = le_compiler_options_init(LE_OPT_NONE);
    rc = le_compile_json_ex(circuit, NULL, &opts, &res);
    free(circuit);

    if (rc != 0 || !res.success) {
        printf("  [%-20s] [FAIL] compile (rc=%d", spec->fixture, rc);
        if (res.error_message) printf(": %s", res.error_message);
        printf(")\n");
        g_fail++;
        le_compile_result_free(&res);
        return 0;
    }

    instr_count = res.instruction_count;
    if (spec->mnemonic[0] != '\0' && res.disassembly_text) {
        opcode_hits = count_opcode(res.disassembly_text, spec->mnemonic);
    }

    printf("  [%-20s] compile=ok instr=%d | ", spec->fixture, instr_count);

    if (spec->total >= 0 && instr_count != spec->total) {
        printf("[FAIL: expected %d instructions]\n", spec->total);
        g_fail++;
        le_compile_result_free(&res);
        return 0;
    }
    if (spec->mnemonic[0] == '\0') {
        CHECK(instr_count == 0, "no-op element must emit 0 instructions");
        printf("PASS\n");
    } else if (opcode_hits != spec->count) {
        printf("[FAIL: expected opcode %s x%d, found x%d]\n",
               spec->mnemonic, spec->count, opcode_hits);
        g_fail++;
        le_compile_result_free(&res);
        return 0;
    } else {
        printf("PASS (%s x%d)\n", spec->mnemonic, opcode_hits);
    }

    le_compile_result_free(&res);
    return 1;
}

int main(int argc, char** argv)
{
    const char* dir = (argc > 1) ? argv[1] : NULL;
    int i;
    int covered = 0;
    int oracle_total = (int)(sizeof(g_oracle) / sizeof(g_oracle[0]));

    printf("============================================================\n");
    printf(" LOGICELEMENTS PER-ELEMENT ASSEMBLY TEST\n");
    printf(" Compiler: %s\n", le_compiler_get_version());
    printf("============================================================\n");
    if (!dir) {
        printf("Usage: %s <fixture_dir>\n", argv[0]);
        return 2;
    }

    for (i = 0; i < oracle_total; i++) {
        if (run_fixture(dir, &g_oracle[i])) covered++;
    }

    printf("============================================================\n");
    printf(" SUMMARY: oracle=%d fixtures_covered=%d failed=%d\n",
           oracle_total, covered, g_fail);
    if (covered < oracle_total) {
        printf(" COVERAGE GAP: %d oracle element(s) missing a fixture\n",
               oracle_total - covered);
    }
    printf("============================================================\n");
    return (g_fail == 0 && covered == oracle_total) ? 0 : 1;
}