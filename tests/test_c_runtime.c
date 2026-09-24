/**
 * @file test_c_runtime.c
 * @brief Comprehensive automated unit tests for LogicElements C Runtime.
 */

#include "le_types.h"
#include "le_process_image.h"
#include "le_opcodes.h"
#include "le_vm.h"
#include "le_rt.h"
#include "le_loader.h"
#include "le_hal.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

const le_hal_t* le_hal_get_sim(void);

/* The self-contained 4-instruction example program (OR->%OUT0, AND->%OUT1)
 * previously shipped in example_configs/example_embedded.h. It is embedded here
 * so the runtime tests carry no external config dependency. */
static const uint8_t le_default_program[] = {
    0x31, 0x42, 0x45, 0x4c, 0x08, 0x00, 0x01, 0x00, 0x04, 0x00, 0x02, 0x00,
    0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xde, 0x68, 0xbf, 0xc5, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x20,
    0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x20, 0x01, 0x00, 0x01, 0x20,
    0xff, 0xff, 0x00, 0x10, 0x01, 0x00, 0x00, 0x20, 0xff, 0xff, 0x01, 0x10
}; /* 72 bytes, version 8 (40-byte 4-aligned header) */

int g_tests_passed = 0;
int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] Line %d: %s\n", __LINE__, msg); \
        g_tests_failed++; \
    } else { \
        g_tests_passed++; \
    } \
} while (0)

/* Manual-opcode tests bypass the loader, so they place state structs directly
 * into the state workspace and record the kind group's byte offset. They bind
 * a private state slice + register arena (le_rt_bind / le_process_image_bind)
 * instead of relying on a static runtime buffer. */
static uint32_t s_test_off = 0;
static uint8_t  s_test_ws[LE_RAM_WORKSPACE_BYTES];
/* The register arena is accessed through typed pointers (direct loads), so
 * it must be 4-byte aligned; the union forces that. */
static union {
    uint8_t   b[LE_RAM_WORKSPACE_BYTES];
    uint32_t  _align4[(LE_RAM_WORKSPACE_BYTES + 3) / 4];
} s_test_reg_arena;
static void test_rt_reset(void) { le_rt_bind(s_test_ws, sizeof(s_test_ws)); le_rt_reset(); s_test_off = 0; }

/* Binds a standalone process image at the FULL compile-time maxima so manual
 * opcode tests can exercise every register region without loading a program. */
static void test_img_init(le_process_image_t* img)
{
    le_header_t full;
    memset(&full, 0, sizeof(full));
    full.digital_in_count  = LE_MAX_DIGITAL_IN;
    full.digital_out_count = LE_MAX_DIGITAL_OUT;
    full.bool_reg_count    = LE_MAX_BOOL_REGS;
    full.float_reg_count   = LE_MAX_FLOATS;
    full.int_reg_count     = LE_MAX_INT_REGS;
#if LE_ENABLE_COMPLEX
    full.complex_reg_count = LE_MAX_COMPLEX;
#endif
#if LE_ENABLE_ANALOG
    full.analog_in_count   = LE_MAX_ANALOG_IN;
#endif
    le_process_image_init(img);
    memset(s_test_reg_arena.b, 0, sizeof(s_test_reg_arena.b)); /* clear stale bits (old init zeroed the struct) */
    le_process_image_bind(img, s_test_reg_arena.b, sizeof(s_test_reg_arena.b), &full, NULL);
}

/* Places `count` zeroed blocks of `size` bytes for `kind` into the workspace at
 * the current offset, so le_process_image_timer/counter/kind_state resolve. */
static void test_bind_kind(uint8_t kind, int count, uint16_t size)
{
    if (s_test_off + (uint32_t)size * (uint32_t)count > le_rt_workspace_bytes()) return;
    le_rt_set_kind_base(kind, (int32_t)s_test_off);
    s_test_off += (uint32_t)size * (uint32_t)count;
}

void test_process_image(void)
{
    printf("Running test_process_image...\n");
    le_process_image_t img;
    test_img_init(&img);

    /* Test digital inputs */
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DIN(0)), "DIN 0 initially false");
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DIN(0)), "DIN 0 set to true");
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), false);
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DIN(0)), "DIN 0 set to false");

    /* Test bit packing boundary (e.g. pin 7 and pin 8 across byte boundary) */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(7), true);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(8), true);
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DIN(7)), "DIN 7 true");
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DIN(8)), "DIN 8 true");
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DIN(6)), "DIN 6 untouched");

    /* Test boolean registers (bool reg) */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_BOOL_REG(15), true);
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_BOOL_REG(15)), "Bool Reg 15 true");

    /* Test integer registers (int reg) */
    le_process_image_set_int(&img, LE_ADDR_MAKE_INT_REG(3), 4242);
    TEST_ASSERT(le_process_image_get_int(&img, LE_ADDR_MAKE_INT_REG(3)) == 4242, "Int Reg 3 set to 4242");

    /* Test analog inputs (ain) */
    le_process_image_set_int(&img, LE_ADDR_MAKE_AIN(2), 1024);
    TEST_ASSERT(le_process_image_get_int(&img, LE_ADDR_MAKE_AIN(2)) == 1024, "AIN 2 set to 1024");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_AIN(2)) - 1024.0f) < 1e-4f, "AIN 2 float value is 1024.0");

    /* Test float registers (%R) */
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(5))) < 1e-6f, "Float initially 0.0");
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(5), 123.456f);
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(5)) - 123.456f) < 1e-4f, "Float 5 set");

    /* Test constants */
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_CONST_FALSE), "CONST FALSE is false");
    TEST_ASSERT(le_process_image_get_bool(&img, LE_CONST_TRUE), "CONST TRUE is true");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_CONST_ZERO_F)) < 1e-6f, "CONST 0.0f is 0.0");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_CONST_ONE_F) - 1.0f) < 1e-6f, "CONST 1.0f is 1.0");
}

void test_basic_opcodes(void)
{
    printf("Running test_basic_opcodes...\n");
    le_process_image_t img;
    test_img_init(&img);

    /* Test AND gate */
    le_instruction_t inst_and = {
        .opcode = LE_OP_AND,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_DIN(0),
        .in_b = LE_ADDR_MAKE_DIN(1),
        .out = LE_ADDR_MAKE_DOUT(0)
    };

    /* 0 AND 0 = 0 */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(1), false);
    le_exec_instruction(&inst_and, &img, 0);
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "0 AND 0 == 0");

    /* 1 AND 0 = 0 */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(1), false);
    le_exec_instruction(&inst_and, &img, 0);
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "1 AND 0 == 0");

    /* 1 AND 1 = 1 */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(1), true);
    le_exec_instruction(&inst_and, &img, 0);
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "1 AND 1 == 1");

    /* Test Modifier: Invert Input B -> 1 AND !0 = 1 */
    inst_and.modifier = LE_MOD_INVERT_B;
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(1), false);
    le_exec_instruction(&inst_and, &img, 0);
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "1 AND !0 == 1");

    /* Test Float Addition */
    le_instruction_t inst_add = {
        .opcode = LE_OP_ADD_F,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_MAKE_FLOAT(1),
        .out = LE_ADDR_MAKE_FLOAT(2)
    };
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 10.5f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(1), 20.25f);
    le_exec_instruction(&inst_add, &img, 0);
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(2)) - 30.75f) < 1e-4f, "10.5 + 20.25 == 30.75");
}

void test_timer_opcode(void)
{
    printf("Running test_timer_opcode (TON)...\n");
    le_process_image_t img;
    test_img_init(&img);
        test_rt_reset(); /* clean state arena for this test */
        test_bind_kind(LE_BLK_TIMER, 2, sizeof(le_timer_state_t));

    /* Preset timer 0 to 100ms */
    le_process_image_timer(&img, 0)->preset_ms = 100;

    le_instruction_t inst_ton = {
        .opcode = LE_OP_TON,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_DIN(0),
        .in_b = LE_ADDR_MAKE_TIMER(0),
        .out = LE_ADDR_MAKE_DOUT(0)
    };

    /* Step at t=0ms with input=false */
    le_exec_instruction(&inst_ton, &img, 0);
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "Timer out false at t=0");

    /* Rising edge of input at t=10ms */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_exec_instruction(&inst_ton, &img, 10);
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "Timer out false at t=10 (timing)");

    /* Step at t=50ms (elapsed 40ms < 100ms) */
    le_exec_instruction(&inst_ton, &img, 50);
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "Timer out false at t=50");

    /* Step at t=110ms (elapsed 100ms == preset) */
    le_exec_instruction(&inst_ton, &img, 110);
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "Timer out true at t=110 (completed)");

    /* Input drops to false at t=150ms -> output should immediately reset */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), false);
    le_exec_instruction(&inst_ton, &img, 150);
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "Timer out false after input dropped");
}

void test_embedded_binary_execution(void)
{
    printf("Running test_embedded_binary_execution (compiled example.lebin)...\n");
    le_vm_t vm;
    le_status_t status = le_vm_init(&vm);
    TEST_ASSERT(status == LE_OK, "VM init successful");

    status = le_loader_load(&vm, le_default_program, sizeof(le_default_program));
    TEST_ASSERT(status == LE_OK, "Loader successfully validated and loaded embedded .lebin");
    TEST_ASSERT(vm.instruction_count == 4, "Loaded 4 instructions");
    TEST_ASSERT(vm.running, "VM autostarted from header flag");

    /* The example circuit logic:
     * %M[0] = %I[0] OR %I[1]
     * %M[1] = %I[0] AND %I[1]
     * %Q[0] = %M[0]  (OR output)
     * %Q[1] = %M[1]  (AND output)
     */

    /* Case 1: IN0 = 0, IN1 = 0 */
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    le_vm_step(&vm, 0);
    TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)), "Case 1: OUT0 (OR) is 0");
    TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(1)), "Case 1: OUT1 (AND) is 0");

    /* Case 2: IN0 = 1, IN1 = 0 */
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), false);
    le_vm_step(&vm, 10);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)), "Case 2: OUT0 (OR) is 1");
    TEST_ASSERT(!le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(1)), "Case 2: OUT1 (AND) is 0");

    /* Case 3: IN0 = 1, IN1 = 1 */
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(0), true);
    le_process_image_set_bool(&vm.image, LE_ADDR_MAKE_DIN(1), true);
    le_vm_step(&vm, 20);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)), "Case 3: OUT0 (OR) is 1");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(1)), "Case 3: OUT1 (AND) is 1");
}

#include "le_comms.h"

/* Helper to send a framed packet byte-by-byte into comms */
static void feed_packet_to_comms(le_comms_t* comms, uint8_t cmd, uint8_t seq, const uint8_t* payload, uint16_t len)
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

    /* Feed header */
    for (int i = 0; i < 5; i++) le_comms_process_byte(comms, header[i]);
    /* Feed payload */
    for (uint16_t i = 0; i < len; i++) le_comms_process_byte(comms, payload[i]);
    /* Feed CRC */
    le_comms_process_byte(comms, (uint8_t)(crc & 0xFF));
    le_comms_process_byte(comms, (uint8_t)((crc >> 8) & 0xFF));
}

void test_comms_protocol(void)
{
    printf("Running test_comms_protocol (UART Upload & Control)...\n");
    le_hal_set(le_hal_get_sim());

    le_vm_t vm;
    le_vm_init(&vm);

    le_comms_t comms;
    le_comms_init(&comms, &vm);

    /* 1. Send PING */
    feed_packet_to_comms(&comms, LE_CMD_PING, 1, NULL, 0);
    TEST_ASSERT(true, "Comms handled PING without error");

    /* 2. Send PROG_BEGIN */
    uint32_t total_size = sizeof(le_default_program);
    uint8_t begin_payload[4] = {
        (uint8_t)(total_size & 0xFF),
        (uint8_t)((total_size >> 8) & 0xFF),
        (uint8_t)((total_size >> 16) & 0xFF),
        (uint8_t)((total_size >> 24) & 0xFF)
    };
    feed_packet_to_comms(&comms, LE_CMD_PROG_BEGIN, 2, begin_payload, 4);
    TEST_ASSERT(comms.program_size == total_size, "Comms PROG_BEGIN recorded expected size");

    /* 3. Send PROG_CHUNK with entire program */
    uint8_t chunk_payload[2 + sizeof(le_default_program)];
    chunk_payload[0] = 0x00; /* offset 0 */
    chunk_payload[1] = 0x00;
    memcpy(&chunk_payload[2], le_default_program, sizeof(le_default_program));
    feed_packet_to_comms(&comms, LE_CMD_PROG_CHUNK, 3, chunk_payload, sizeof(chunk_payload));
    TEST_ASSERT(comms.bytes_received == total_size, "Comms received entire program chunk");

    /* 4. Send PROG_END */
    feed_packet_to_comms(&comms, LE_CMD_PROG_END, 4, NULL, 0);
    TEST_ASSERT(vm.instruction_count == 4, "Comms PROG_END committed and loaded VM with 4 instructions");
    TEST_ASSERT(vm.running, "VM autostarted from uploaded program");

    /* 5. Force I/O via UART: Force IN0=true */
    uint8_t force_payload[3] = {
        (uint8_t)(LE_ADDR_MAKE_DIN(0) & 0xFF),
        (uint8_t)((LE_ADDR_MAKE_DIN(0) >> 8) & 0xFF),
        0x01 /* value = true */
    };
    feed_packet_to_comms(&comms, LE_CMD_FORCE_IO, 5, force_payload, 3);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DIN(0)), "Force IO set DIN 0 to true");

    /* Run VM step -> OUT0 (OR) should become true */
    le_vm_step(&vm, 100);
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DOUT(0)), "VM evaluated forced input: OUT0 true");
}

#if LE_ENABLE_PROTECTION
void test_protection_relays(void)
{
    printf("Running test_protection_relays (Phasor 1P, SymComp, Diff 87, Dist 21)...\n");
    le_process_image_t img;
    test_img_init(&img);
        test_rt_reset(); /* clean state arena for this test */
        test_bind_kind(LE_BLK_PHASOR, 4, sizeof(le_phasor_state_t));
        test_bind_kind(LE_BLK_SYMCOMP, 1, sizeof(le_symcomp_state_t));
        test_bind_kind(LE_BLK_21, 1, sizeof(le_dist21_state_t));
        test_bind_kind(LE_BLK_DIFF_87, 1, sizeof(le_diff87_state_t));
        test_bind_kind(LE_BLK_PHASE_COMP, 1, sizeof(le_comp33_state_t));

    le_phasor_state_t* ph = (le_phasor_state_t*)le_process_image_kind_state(&img, LE_BLK_PHASOR, 0);
    le_phasor_state_t* ph1 = (le_phasor_state_t*)le_process_image_kind_state(&img, LE_BLK_PHASOR, 1);
    le_phasor_state_t* ph2 = (le_phasor_state_t*)le_process_image_kind_state(&img, LE_BLK_PHASOR, 2);
    le_phasor_state_t* ph3 = (le_phasor_state_t*)le_process_image_kind_state(&img, LE_BLK_PHASOR, 3);
    le_symcomp_state_t* sc = (le_symcomp_state_t*)le_process_image_kind_state(&img, LE_BLK_SYMCOMP, 0);
    le_dist21_state_t* d21 = (le_dist21_state_t*)le_process_image_kind_state(&img, LE_BLK_21, 0);

    /* Local block descriptor + args (2-in/1-out max needed here). */
    struct { le_block_desc_t d; uint16_t a[4]; } b;

    /* ---------------------------------------------------------------------- */
    /* 1. 1P Winding Phasor Extraction                                        */
    /* ---------------------------------------------------------------------- */
    /* Generate 16 samples of a 60Hz sine wave: A=10.0, phi = 30 deg (0.5236 rad) */
    /* x(t) = A * cos(omega*t + phi) */
    uint16_t N = 16;
    ph->samples_per_cycle = N;
    float amp = 10.0f;
    float phi = 30.0f * (float)M_PI / 180.0f;

    le_instruction_t inst_phasor = {
        .opcode = LE_OP_PHASOR_1P,
        .modifier = 0, /* phasor index 0 */
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(1)
    };

    /* Feed 16 sequential samples through the DFT filter */
    for (uint16_t k = 0; k < N; k++) {
        float t_angle = 2.0f * (float)M_PI * (float)k / (float)N;
        float sample = amp * cosf(t_angle + phi);
        le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), sample);
        le_exec_instruction(&inst_phasor, &img, k * 1);
    }

    float extracted_mag = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(extracted_mag - amp) < 0.2f, "1P Phasor magnitude ~ 10.0");
    TEST_ASSERT(fabsf(ph->angle_rad - phi) < 0.1f, "1P Phasor angle ~ 30 deg (0.52 rad)");

    /* ---------------------------------------------------------------------- */
    /* 2. Symmetrical Components (Fortescue Transformation)                   */
    /* ---------------------------------------------------------------------- */
    /* Balanced 3-phase set: A = 10 < 0, B = 10 < -120, C = 10 < +120 */
    ph1->phasor = le_c_polar(10.0f, 0.0f);
    ph2->phasor = le_c_polar(10.0f, -120.0f * (float)M_PI / 180.0f);
    ph3->phasor = le_c_polar(10.0f, 120.0f * (float)M_PI / 180.0f);

    le_instruction_t inst_sym = {
        .opcode = LE_OP_SYM_COMP,
        .modifier = 0, /* symcomp 0 */
        .in_a = 1,     /* base phasor index: 1, 2, 3 */
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(2) /* writes I1 mag */
    };
    le_exec_instruction(&inst_sym, &img, 0);

    float i1_mag = le_c_mag(sc->seq_1);
    float i2_mag = le_c_mag(sc->seq_2);
    float i0_mag = le_c_mag(sc->seq_0);

    TEST_ASSERT(fabsf(i1_mag - 10.0f) < 0.05f, "Balanced set: Pos Seq I1 ~ 10.0");
    TEST_ASSERT(i2_mag < 0.05f, "Balanced set: Neg Seq I2 ~ 0.0");
    TEST_ASSERT(i0_mag < 0.05f, "Balanced set: Zero Seq I0 ~ 0.0");

    /* Single-Phase-to-Ground Fault: A = 30 < 0, B = 0, C = 0 */
    ph1->phasor = le_c_make(30.0f, 0.0f);
    ph2->phasor = le_c_make(0.0f, 0.0f);
    ph3->phasor = le_c_make(0.0f, 0.0f);
    le_exec_instruction(&inst_sym, &img, 0);

    i1_mag = le_c_mag(sc->seq_1);
    i2_mag = le_c_mag(sc->seq_2);
    i0_mag = le_c_mag(sc->seq_0);
    TEST_ASSERT(fabsf(i1_mag - 10.0f) < 0.05f, "A-G Fault: I1 = 30/3 = 10.0");
    TEST_ASSERT(fabsf(i2_mag - 10.0f) < 0.05f, "A-G Fault: I2 = 30/3 = 10.0");
    TEST_ASSERT(fabsf(i0_mag - 10.0f) < 0.05f, "A-G Fault: I0 = 30/3 = 10.0");

    /* ---------------------------------------------------------------------- */
    /* ---------------------------------------------------------------------- */
    /* 3. Mho Distance Relaying (21) - property round-trip; full behavior  */
    /*    verified via test_compiler.cpp test_dist21_mho.                */
    /* ---------------------------------------------------------------------- */
    d21->reach_ohms = 10.0f;
    d21->line_angle_deg = 75.0f;
    d21->offset_mag = 0.0f;
    d21->offset_angle_deg = 75.0f;
    d21->prefault_v_threshold = 0.5f;
    d21->prefault_duration_ms = 80;
    d21->prefault_v = le_c_polar(50.0f, 75.0f * (float)M_PI / 180.0f);
    TEST_ASSERT(fabsf(d21->reach_ohms - 10.0f) < 1e-4f,
                "21: reach_ohms property round-trips");
    TEST_ASSERT(d21->prefault_duration_ms == 80,
                "21: prefault_duration_ms property round-trips");

    /* ---------------------------------------------------------------------- */
    /* 4. Dual-Slope Differential (87) - correct ANSI naming & dual-slope char. */
    /* ---------------------------------------------------------------------- */
    le_diff87_state_t* d87 = (le_diff87_state_t*)le_process_image_kind_state(&img, LE_BLK_DIFF_87, 0);
    TEST_ASSERT(d87 != NULL, "87: diff state bound");
    d87->o87p = 0.3f; d87->slp1 = 0.25f; d87->irs1 = 1.5f; d87->slp2 = 0.60f;
    /* 2-in/1-out DIFF_87 block: [c0, c1, out_bool] */
    b.d.in_count = 2; b.d.out_count = 1;
    b.a[0] = LE_ADDR_MAKE_CMPLX(0); b.a[1] = LE_ADDR_MAKE_CMPLX(1); b.a[2] = LE_ADDR_MAKE_BOOL_REG(0);
    le_instruction_t d87i = { LE_OP_BLOCK, LE_FUNC_DIFF_87, 0, LE_ADDR_UNUSED, LE_ADDR_UNUSED };

    /* Two equal-magnitude phasors 180 deg apart: operate = |sum| ~ 0, restraint =
     * sum of magnitudes = 2 -> below pickup -> no trip (healthy, no fault). */
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(0), le_c_make(1.0f, 0.0f));
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(1), le_c_make(-1.0f, 0.0f));
    TEST_ASSERT(le_exec_instruction_ex(&d87i, &img, 0, &b.d, 1) == LE_OK, "87: DIFF_87 executes");
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_BOOL_REG(0)),
                "87: balanced phasors -> no differential trip");
    TEST_ASSERT(fabsf(d87->restraint - 2.0f) < 1e-4f, "87: restraint = sum of mags = 2");

    /* Two aligned phasors: operate = 2, restraint = sum = 2; I_rt=2 > irs1=1.5 so
     * threshold = o87p + slp1*irs1 + slp2*(2-irs1) = 0.3+0.375+0.3 = 0.975.
     * operate(2) > 0.975 -> trip (dual-slope SECOND segment). */
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(0), le_c_make(1.0f, 0.0f));
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(1), le_c_make(1.0f, 0.0f));
    TEST_ASSERT(le_exec_instruction_ex(&d87i, &img, 0, &b.d, 1) == LE_OK, "87: DIFF_87 executes aligned");
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_BOOL_REG(0)),
                "87: aligned phasors -> differential trip (operate > dual-slope threshold)");

    d87->tripped = false;

    /* ---------------------------------------------------------------------- */
    /* 5. 3-Phase Transformer Phase Compensation (87T)                        */
    /* ---------------------------------------------------------------------- */
    le_comp33_state_t* c33 = (le_comp33_state_t*)le_process_image_kind_state(&img, LE_BLK_PHASE_COMP, 0);
    TEST_ASSERT(c33 != NULL, "87T: comp state bound");
    c33->comp = 1;                          /* odd k -> s = 1/sqrt(3) */
    /* 3-in/3-out PHASE_COMP block: [ia, ib, ic, oa, ob, oc] */
    b.d.in_count = 3; b.d.out_count = 3;
    b.a[0] = LE_ADDR_MAKE_CMPLX(0); b.a[1] = LE_ADDR_MAKE_CMPLX(1); b.a[2] = LE_ADDR_MAKE_CMPLX(2);
    b.a[3] = LE_ADDR_MAKE_CMPLX(3); b.a[4] = LE_ADDR_MAKE_CMPLX(4); b.a[5] = LE_ADDR_MAKE_CMPLX(5);
    le_instruction_t phc = { LE_OP_BLOCK, LE_FUNC_PHASE_COMP, 0, LE_ADDR_UNUSED, LE_ADDR_UNUSED };

    /* k=1 matrix: [1,-1,0; 0,1,-1; -1,0,1] * (1/sqrt(3)).
     * With I_A=1, I_B=0, I_C=0:
     *   oa = (1*1 + -1*0 + 0)/sqrt3 = 1/sqrt3
     *   ob = (0*1 + 1*0 + -1*0)/sqrt3 = 0
     *   oc = (-1*1 + 0 + 1*0)/sqrt3 = -1/sqrt3   (exact SEL matrix row) */
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(0), le_c_make(1.0f, 0.0f));
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(1), le_c_make(0.0f, 0.0f));
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(2), le_c_make(0.0f, 0.0f));
    TEST_ASSERT(le_exec_instruction_ex(&phc, &img, 0, &b.d, 1) == LE_OK, "87T: PHASE_COMP executes");
    le_complex_t oa = le_process_image_get_complex(&img, LE_ADDR_MAKE_CMPLX(3));
    le_complex_t ob = le_process_image_get_complex(&img, LE_ADDR_MAKE_CMPLX(4));
    le_complex_t oc = le_process_image_get_complex(&img, LE_ADDR_MAKE_CMPLX(5));
    float inv_sqrt3 = 1.0f / sqrtf(3.0f);
    TEST_ASSERT(fabsf(oa.r - inv_sqrt3) < 1e-4f && fabsf(oa.i) < 1e-4f, "87T: out_a = 1/sqrt3 (k=1 row0)");
    TEST_ASSERT(fabsf(ob.r) < 1e-4f && fabsf(ob.i) < 1e-4f, "87T: out_b = 0 (k=1 row1)");
    TEST_ASSERT(fabsf(oc.r + inv_sqrt3) < 1e-4f && fabsf(oc.i) < 1e-4f, "87T: out_c = -1/sqrt3 (k=1 row2)");

    /* k=2 (even, s=1/3): balanced I_A=I_B=I_C=1 -> all compensated = 0. */
    c33->comp = 2;
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(0), le_c_make(1.0f, 0.0f));
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(1), le_c_make(1.0f, 0.0f));
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(2), le_c_make(1.0f, 0.0f));
    TEST_ASSERT(le_exec_instruction_ex(&phc, &img, 0, &b.d, 1) == LE_OK, "87T: PHASE_COMP executes k=2");
    le_complex_t o2a = le_process_image_get_complex(&img, LE_ADDR_MAKE_CMPLX(3));
    TEST_ASSERT(le_c_mag(o2a) < 1e-4f, "87T: balanced through-load cancels to ~0 (operate=0)");
}
#endif


#include "le_storage.h"
#include "le_cli.h"

static uint16_t calc_test_crc16(const uint8_t* data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

static void feed_str_to_cli(le_cli_t* cli, const char* str)
{
    while (*str) {
        le_cli_process_char(cli, (uint8_t)*str++);
    }
}

void test_storage_and_terminal_cli(void)
{
    printf("Running test_storage_and_terminal_cli (Multi-Slot, CLI & XMODEM-CRC)...\n");

    const le_hal_t* sim = le_hal_get_sim();
    sim->init();

    le_storage_t storage;
    le_storage_init(&storage, sim);

    le_vm_t vm;
    le_vm_init(&vm);

    le_cli_t cli;
    le_cli_init(&cli, &vm, &storage);

    /* 1. Multi-Slot Storage Tests */
    TEST_ASSERT(le_storage_get_slot_count(&storage) == LE_MAX_CONFIG_SLOTS, "Storage slot count matches LE_MAX_CONFIG_SLOTS");

    le_slot_info_t info0;
    le_storage_get_slot_info(&storage, 0, &info0);
    TEST_ASSERT(!info0.valid, "Slot 0 initially empty/invalid");

    /* Write program into Slot 0 */
    bool ok = le_storage_write_chunk(&storage, 0, 0, le_default_program, sizeof(le_default_program));
    TEST_ASSERT(ok, "Wrote default program into Slot 0");

    le_storage_get_slot_info(&storage, 0, &info0);
    TEST_ASSERT(info0.valid, "Slot 0 verified valid after write");
    TEST_ASSERT(info0.instruction_count == 4, "Slot 0 has 4 instructions");

    /* Activate Slot 0 into VM */
    ok = le_storage_activate_slot(&storage, 0, &vm);
    TEST_ASSERT(ok, "Slot 0 activated and loaded into VM");
    TEST_ASSERT(vm.instruction_count == 4, "VM has 4 instructions from Slot 0");
    TEST_ASSERT(le_storage_get_active_slot(&storage) == 0, "Active slot is 0");

    /* 2. Interactive Terminal Commands */
    feed_str_to_cli(&cli, "help\r\n");
    feed_str_to_cli(&cli, "info\r\n");
    feed_str_to_cli(&cli, "status\r\n");
    feed_str_to_cli(&cli, "slots\r\n");
    feed_str_to_cli(&cli, "io\r\n");
    TEST_ASSERT(cli.mode == LE_CLI_MODE_NORMAL, "CLI remains in normal mode after info/status/slots");

    /* Force I/O via CLI */
    feed_str_to_cli(&cli, "force %IN0 1\r\n");
    TEST_ASSERT(le_process_image_get_bool(&vm.image, LE_ADDR_MAKE_DIN(0)), "CLI 'force %IN0 1' set DIN 0 to true");

    feed_str_to_cli(&cli, "run\r\n");
    TEST_ASSERT(vm.running, "CLI 'run' started VM");

    feed_str_to_cli(&cli, "stop\r\n");
    TEST_ASSERT(!vm.running, "CLI 'stop' stopped VM");

    /* 3. Secure XMODEM-CRC File Upload to Slot 1 */
    feed_str_to_cli(&cli, "upload 1 xmodem\r\n");
    TEST_ASSERT(cli.mode == LE_CLI_MODE_XMODEM, "CLI entered XMODEM upload mode");
    TEST_ASSERT(cli.upload_slot == 1, "Target upload slot is 1");

    /* Construct standard 133-byte XMODEM-CRC packet for block 1 */
    uint8_t xpacket[133];
    xpacket[0] = 0x01; /* SOH */
    xpacket[1] = 0x01; /* Block 1 */
    xpacket[2] = 0xFE; /* ~Block 1 */
    memset(&xpacket[3], 0x00, 128); /* 128 bytes data */
    memcpy(&xpacket[3], le_default_program, sizeof(le_default_program)); /* Put program in block */
    uint16_t crc = calc_test_crc16(&xpacket[3], 128);
    xpacket[131] = (uint8_t)((crc >> 8) & 0xFF); /* CRC MSB */
    xpacket[132] = (uint8_t)(crc & 0xFF);        /* CRC LSB */

    /* Feed 133 bytes of packet */
    for (int i = 0; i < 133; i++) {
        le_cli_process_char(&cli, xpacket[i]);
    }
    TEST_ASSERT(cli.upload_offset == 128, "XMODEM committed 128 bytes to Slot 1");

    /* Send EOT (End of Transmission) */
    le_cli_process_char(&cli, 0x04);
    TEST_ASSERT(cli.mode == LE_CLI_MODE_NORMAL, "CLI returned to normal mode after EOT");

    /* Verify Slot 1 is valid */
    le_slot_info_t info1;
    le_storage_get_slot_info(&storage, 1, &info1);
    TEST_ASSERT(info1.valid, "Slot 1 verified valid after XMODEM transfer");
    TEST_ASSERT(info1.instruction_count == 4, "Slot 1 has 4 instructions");

    /* Switch to Slot 1 via CLI */
    feed_str_to_cli(&cli, "select 1\r\n");
    TEST_ASSERT(le_storage_get_active_slot(&storage) == 1, "CLI 'select 1' switched active slot to 1");

    /* 4. Hex Paste Upload to Slot 2 */
    feed_str_to_cli(&cli, "upload 2 hex\r\n");
    TEST_ASSERT(cli.mode == LE_CLI_MODE_HEX, "CLI entered HEX paste mode");

    /* Feed hex string of le_default_program */
    for (size_t i = 0; i < sizeof(le_default_program); i++) {
        char hex_pair[3];
        snprintf(hex_pair, sizeof(hex_pair), "%02X", le_default_program[i]);
        feed_str_to_cli(&cli, hex_pair);
    }
    /* End hex upload with newline */
    feed_str_to_cli(&cli, "\r\n");
    TEST_ASSERT(cli.mode == LE_CLI_MODE_NORMAL, "CLI returned to normal mode after hex upload");

    le_slot_info_t info2;
    le_storage_get_slot_info(&storage, 2, &info2);
    TEST_ASSERT(info2.valid, "Slot 2 verified valid after Hex paste upload");
}

#if LE_ENABLE_SERIAL_BUS
/* Prototypes from le_hal_sim.c */
void le_sim_set_mock_i2c_response(const uint8_t* data, size_t len);
void le_sim_set_mock_spi_response(const uint8_t* data, size_t len);
size_t le_sim_get_i2c_transfers(void);
size_t le_sim_get_spi_transfers(void);
size_t le_sim_get_i2c_startup_len(void);
size_t le_sim_get_spi_startup_len(void);

void test_serial_bus_i2c_spi(void)
{
    printf("Running test_serial_bus_i2c_spi (Startup Code, Polling Rate & Edge Trigger)...\n");

    const le_hal_t* sim = le_hal_get_sim();
    sim->init();
    le_hal_set(sim);

    le_process_image_t img;
    test_img_init(&img);
        test_rt_reset(); /* clean state arena for this test */
        test_bind_kind(LE_BLK_I2C, 1, sizeof(le_i2c_device_state_t));
        test_bind_kind(LE_BLK_SPI, 1, sizeof(le_spi_device_state_t));
    le_i2c_device_state_t* i2cdev = (le_i2c_device_state_t*)le_process_image_kind_state(&img, LE_BLK_I2C, 0);
    le_spi_device_state_t* spidev = (le_spi_device_state_t*)le_process_image_kind_state(&img, LE_BLK_SPI, 0);

    /* 1. I2C Device Test: Startup Config & Periodic Polling Rate */
    uint8_t i2c_startup[2] = {0x01, 0x60};
    uint8_t i2c_poll_cmd[1] = {0x00};
    le_i2c_device_config(&img, 0, 0x48, i2c_startup, 2, 100, i2c_poll_cmd, 1, 2, LE_ADDR_MAKE_FLOAT(0));

    uint8_t mock_temp[2] = {0x00, 0x19}; /* 0x0019 = 25 */
    le_sim_set_mock_i2c_response(mock_temp, 2);

    le_instruction_t inst_i2c = {
        .opcode = LE_OP_I2C,
        .modifier = 0,               /* Device index 0 */
        .in_a = LE_ADDR_MAKE_DIN(0), /* Trigger bit */
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_DOUT(0)  /* Status output */
    };

    /* Cycle 0 at t=0ms: Trigger false -> executes startup code */
    le_exec_instruction(&inst_i2c, &img, 0);
    TEST_ASSERT(i2cdev->initialized, "I2C Device 0 executed startup code on cycle 0");
    TEST_ASSERT(le_sim_get_i2c_startup_len() == 2, "I2C Startup code sent exactly 2 config bytes");
    TEST_ASSERT(i2cdev->poll_count == 0, "No poll yet at t=0ms without trigger");

    /* Cycle 1 at t=50ms: Time elapsed 50ms < 100ms -> No poll */
    le_exec_instruction(&inst_i2c, &img, 50);
    TEST_ASSERT(i2cdev->poll_count == 0, "I2C does not poll before 100ms period");

    /* Cycle 2 at t=100ms: Polling rate timer expires -> Automatic rate poll */
    le_exec_instruction(&inst_i2c, &img, 100);
    TEST_ASSERT(i2cdev->poll_count == 1, "I2C polled automatically when 100ms polling rate expired");
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "I2C poll strobe asserted on OUT0");
    float temp_val = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(0));
    TEST_ASSERT(fabsf(temp_val - 25.0f) < 0.01f, "I2C received data converted into float %R[0] = 25.0");

    /* 2. I2C Rising Edge Trigger Polling */
    uint8_t mock_temp2[2] = {0x00, 0x1E}; /* 0x001E = 30 */
    le_sim_set_mock_i2c_response(mock_temp2, 2);

    /* Cycle 3 at t=110ms: Rising edge on IN0 (0 -> 1) */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_exec_instruction(&inst_i2c, &img, 110);
    TEST_ASSERT(i2cdev->poll_count == 2, "I2C polled immediately on rising-edge trigger at t=110ms");
    temp_val = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(0));
    TEST_ASSERT(fabsf(temp_val - 30.0f) < 0.01f, "I2C %R[0] updated with new data = 30.0");

    /* Cycle 4 at t=120ms: IN0 remains high (steady high, no rising edge) -> No poll */
    le_exec_instruction(&inst_i2c, &img, 120);
    TEST_ASSERT(i2cdev->poll_count == 2, "I2C does NOT poll on steady high level (edge security)");

    /* 3. SPI Device Test: Startup Code & Rising Edge Polling */
    uint8_t spi_startup[4] = {0x90, 0x00, 0x00, 0x00};
    uint8_t spi_poll_tx[2] = {0x01, 0x80};
    le_spi_device_config(&img, 0, 10, spi_startup, 4, 0, spi_poll_tx, 2, LE_ADDR_MAKE_FLOAT(1));

    uint8_t mock_spi_rx[2] = {0x02, 0x00}; /* 0x0200 = 512 */
    le_sim_set_mock_spi_response(mock_spi_rx, 2);

    le_instruction_t inst_spi = {
        .opcode = LE_OP_SPI,
        .modifier = 0,               /* SPI Device index 0 */
        .in_a = LE_ADDR_MAKE_DIN(1), /* Trigger on DIN 1 */
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_DOUT(1)
    };

    /* Cycle 0 at t=0ms: Trigger false -> sends SPI startup bytes */
    le_exec_instruction(&inst_spi, &img, 0);
    TEST_ASSERT(spidev->initialized, "SPI Device 0 executed startup code on first scan");
    TEST_ASSERT(le_sim_get_spi_startup_len() == 4, "SPI sent 4 startup bytes with CS pin 10");
    TEST_ASSERT(spidev->poll_count == 0, "SPI did not poll without trigger (poll_rate=0)");

    /* Advance time to t=5000ms: poll_rate=0 -> No automatic poll */
    le_exec_instruction(&inst_spi, &img, 5000);
    TEST_ASSERT(spidev->poll_count == 0, "SPI does not poll on timer when poll_rate_ms=0");

    /* Pulse trigger DIN 1 (0 -> 1): Rising edge polls SPI slave */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(1), true);
    le_exec_instruction(&inst_spi, &img, 5010);
    TEST_ASSERT(spidev->poll_count == 1, "SPI polled immediately on rising edge trigger");
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(1)), "SPI poll success output asserted");
    float spi_val = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(spi_val - 512.0f) < 0.01f, "SPI received data converted into float %R[1] = 512.0");

    /* Steady high on DIN 1: No repeat poll */
    le_exec_instruction(&inst_spi, &img, 5020);
    TEST_ASSERT(spidev->poll_count == 1, "SPI does not repeat poll on steady high trigger");
}
#endif

static uint8_t s_captured_uart[8192];
static size_t s_captured_uart_len = 0;

static size_t capturing_uart_write(const uint8_t* buf, size_t len)
{
    if (buf && len > 0) {
        if (s_captured_uart_len + len < sizeof(s_captured_uart)) {
            memcpy(&s_captured_uart[s_captured_uart_len], buf, len);
            s_captured_uart_len += len;
            s_captured_uart[s_captured_uart_len] = '\0';
        }
    }
    return len;
}

void test_board_capabilities_query(void)
{
    printf("Running test_board_capabilities_query (Binary Packet & CLI JSON)...\n");

    const le_hal_t* orig_hal = le_hal_get_sim();
    le_hal_t captured_hal = *orig_hal;
    captured_hal.uart_write = capturing_uart_write;
    le_hal_set(&captured_hal);

    le_vm_t vm;
    le_vm_init(&vm);

    le_comms_t comms;
    le_comms_init(&comms, &vm);

    /* 1. Test binary capabilities packet (LE_CMD_GET_CAPS -> LE_CMD_CAPS_DATA) */
    s_captured_uart_len = 0;
    feed_packet_to_comms(&comms, LE_CMD_GET_CAPS, 10, NULL, 0);

    TEST_ASSERT(s_captured_uart_len >= 5 + sizeof(le_caps_payload_t) + 2, "Received binary CAPS response packet");
    TEST_ASSERT(s_captured_uart[0] == LE_COMMS_SYNC_BYTE, "Packet sync byte is 0xAA");
    TEST_ASSERT(s_captured_uart[1] == LE_CMD_CAPS_DATA, "Packet command is LE_CMD_CAPS_DATA (0x82)");
    TEST_ASSERT(s_captured_uart[2] == 10, "Sequence number matched request (10)");

    uint16_t plen = (uint16_t)(s_captured_uart[3] | (s_captured_uart[4] << 8));
    TEST_ASSERT(plen == sizeof(le_caps_payload_t), "Payload length equals sizeof(le_caps_payload_t)");

    le_caps_payload_t caps;
    memcpy(&caps, &s_captured_uart[5], sizeof(caps));
    TEST_ASSERT(caps.protocol_version == 3, "Reported protocol version is 3");
    TEST_ASSERT(caps.firmware_major == 1 && caps.firmware_minor == 0, "Reported firmware version is v1.0");
    TEST_ASSERT(strcmp(caps.platform_name, "Simulator / Desktop") == 0, "Reported platform name matches HAL");
    TEST_ASSERT(caps.max_digital_in == LE_MAX_DIGITAL_IN, "Max DIN matches configuration");
    TEST_ASSERT(caps.max_digital_out == LE_MAX_DIGITAL_OUT, "Max DOUT matches configuration");
    TEST_ASSERT(caps.max_analog_in == LE_MAX_ANALOG_IN, "Max AIN matches configuration");
    TEST_ASSERT(caps.max_bool_regs == LE_MAX_BOOL_REGS, "Max Bool Regs matches configuration");
    TEST_ASSERT(caps.max_floats == LE_MAX_FLOATS, "Max Floats matches configuration");
    TEST_ASSERT(caps.workspace_bytes == LE_RAM_WORKSPACE_BYTES, "State workspace bytes matches configuration");
    TEST_ASSERT(caps.config_slots == LE_MAX_CONFIG_SLOTS, "Config slots matches LE_MAX_CONFIG_SLOTS");
    TEST_ASSERT(caps.slot_size_bytes == LE_SLOT_SIZE_BYTES, "Slot size matches LE_SLOT_SIZE_BYTES");

#if LE_ENABLE_PROTECTION
    TEST_ASSERT((caps.feature_flags & LE_CAP_PROTECTION) != 0, "LE_CAP_PROTECTION flag set");
#endif
#if LE_ENABLE_SERIAL_BUS
    TEST_ASSERT((caps.feature_flags & LE_CAP_SERIAL_BUS) != 0, "LE_CAP_SERIAL_BUS flag set");
    TEST_ASSERT((caps.feature_flags & LE_CAP_I2C) != 0, "LE_CAP_I2C flag set");
    TEST_ASSERT((caps.feature_flags & LE_CAP_SPI) != 0, "LE_CAP_SPI flag set");
#endif

    /* 2. Test interactive CLI 'caps' command output */
    le_storage_t storage;
    le_storage_init(&storage, &captured_hal);

    le_cli_t cli;
    le_cli_init(&cli, &vm, &storage);

    s_captured_uart_len = 0;
    feed_str_to_cli(&cli, "caps\r\n");

    TEST_ASSERT(s_captured_uart_len > 0, "CLI emitted output for 'caps'");
    TEST_ASSERT(strstr((const char*)s_captured_uart, "\"name\": \"Simulator / Desktop\"") != NULL, "CLI output contains board name");
    TEST_ASSERT(strstr((const char*)s_captured_uart, "\"limits\":") != NULL, "CLI output contains limits object");
    TEST_ASSERT(strstr((const char*)s_captured_uart, "\"features\":") != NULL, "CLI output contains features object");
    TEST_ASSERT(strstr((const char*)s_captured_uart, "\"custom_nodes\":") != NULL, "CLI output contains custom_nodes");
    TEST_ASSERT(strstr((const char*)s_captured_uart, "Sim_HW_Sqrt") != NULL, "CLI output contains Sim_HW_Sqrt custom node");

    le_hal_set(orig_hal);
}

void test_custom_nodes_and_ext_call(void)
{
    printf("Running test_custom_nodes_and_ext_call...\n");

    const le_hal_t* orig_hal = g_le_hal;
    const le_hal_t* sim = le_hal_get_sim();
    le_hal_set(sim);

    le_process_image_t img;
    test_img_init(&img);

    /* 1. Test LE_OP_EXT_CALL Function 1: Hardware Sqrt */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 144.0f);
    le_instruction_t inst_sqrt = {
        .opcode = LE_OP_EXT_CALL,
        .modifier = 0x81, /* func_id = 0x81 (Sim_HW_Sqrt) */
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(1)
    };
    le_status_t st = le_exec_instruction(&inst_sqrt, &img, 0);
    TEST_ASSERT(st == LE_OK, "LE_OP_EXT_CALL func 1 executed with LE_OK");
    float res_sqrt = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(res_sqrt - 12.0f) < 1e-4f, "Hardware Sqrt(144.0) returned 12.0");

    /* 2. Test LE_OP_EXT_CALL Function 2: Scaled Adder */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(2), 20.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(3), 30.0f);
    le_instruction_t inst_add = {
        .opcode = LE_OP_EXT_CALL,
        .modifier = 0x82, /* func_id = 0x82 (Sim_Scaled_Add) */
        .in_a = LE_ADDR_MAKE_FLOAT(2),
        .in_b = LE_ADDR_MAKE_FLOAT(3),
        .out = LE_ADDR_MAKE_FLOAT(4)
    };
    st = le_exec_instruction(&inst_add, &img, 0);
    TEST_ASSERT(st == LE_OK, "LE_OP_EXT_CALL func 2 executed with LE_OK");
    float res_add = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(4));
    TEST_ASSERT(fabsf(res_add - 75.0f) < 1e-4f, "Scaled Adder (20 + 30) * 1.5 returned 75.0");

    /* 3. Test LE_OP_EXT_CALL Function 3: Boolean Pulse */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_BOOL_REG(0), true);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_BOOL_REG(1), false);
    le_instruction_t inst_bool = {
        .opcode = LE_OP_EXT_CALL,
        .modifier = 0x83, /* func_id = 0x83 (Boolean Pulse) */
        .in_a = LE_ADDR_MAKE_BOOL_REG(0),
        .in_b = LE_ADDR_MAKE_BOOL_REG(1),
        .out = LE_ADDR_MAKE_BOOL_REG(2)
    };
    st = le_exec_instruction(&inst_bool, &img, 0);
    TEST_ASSERT(st == LE_OK, "LE_OP_EXT_CALL func 3 executed with LE_OK");
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_BOOL_REG(2)) == true, "Boolean Pulse (true && !false) returned true");

    /* 4. Test UART communications packet: LE_CMD_GET_CUSTOM_NODES */
    le_hal_t captured_hal = *sim;
    captured_hal.uart_write = capturing_uart_write;
    le_hal_set(&captured_hal);

    le_vm_t vm;
    le_vm_init(&vm);
    le_comms_t comms;
    le_comms_init(&comms, &vm);

    s_captured_uart_len = 0;
    feed_packet_to_comms(&comms, LE_CMD_GET_CUSTOM_NODES, 42, NULL, 0);

    TEST_ASSERT(s_captured_uart_len >= 5 + 2, "Received UART packet for GET_CUSTOM_NODES");
    TEST_ASSERT(s_captured_uart[0] == LE_COMMS_SYNC_BYTE, "Packet sync byte is 0xAA");
    TEST_ASSERT(s_captured_uart[1] == LE_CMD_CUSTOM_NODES_DATA, "Packet command is LE_CMD_CUSTOM_NODES_DATA (0x83)");
    TEST_ASSERT(s_captured_uart[2] == 42, "Sequence number matched request (42)");

    uint16_t plen = (uint16_t)(s_captured_uart[3] | (s_captured_uart[4] << 8));
    TEST_ASSERT(plen > 0, "Custom nodes JSON payload length > 0");

    char payload_buf[2048] = {0};
    size_t copy_len = (plen < sizeof(payload_buf) - 1) ? plen : (sizeof(payload_buf) - 1);
    memcpy(payload_buf, &s_captured_uart[5], copy_len);
    payload_buf[copy_len] = '\0';
    TEST_ASSERT(strstr(payload_buf, "Sim_HW_Sqrt") != NULL, "Payload contains Sim_HW_Sqrt");
    TEST_ASSERT(strstr(payload_buf, "Sim_Scaled_Add") != NULL, "Payload contains Sim_Scaled_Add");

    /* 5. Test CLI 'nodes' command */
    le_storage_t storage;
    le_storage_init(&storage, &captured_hal);
    le_cli_t cli;
    le_cli_init(&cli, &vm, &storage);

    s_captured_uart_len = 0;
    feed_str_to_cli(&cli, "nodes\r\n");
    TEST_ASSERT(s_captured_uart_len > 0, "CLI emitted output for 'nodes'");
    TEST_ASSERT(strstr((const char*)s_captured_uart, "=== Custom Board Nodes ===") != NULL, "CLI printed Custom Board Nodes header");
    TEST_ASSERT(strstr((const char*)s_captured_uart, "Sim_HW_Sqrt") != NULL, "CLI output contains Sim_HW_Sqrt");

    le_hal_set(orig_hal);
}

void test_analog_inputs_and_scaling(void)
{
    printf("Running test_analog_inputs_and_scaling...\n");
    test_rt_reset();
    le_process_image_t img;
    test_img_init(&img);
    test_rt_reset(); /* clean state arena for this test */
    test_bind_kind(LE_BLK_SCALER, 4, sizeof(le_scale_state_t));

    /* 1. Test setting and getting raw AIN integer values */
    le_process_image_set_int(&img, LE_ADDR_MAKE_AIN(0), 2048);
    TEST_ASSERT(le_process_image_get_int(&img, LE_ADDR_MAKE_AIN(0)) == 2048, "AIN 0 raw integer is 2048");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_AIN(0)) - 2048.0f) < 1e-4f, "AIN 0 float view matches raw");

    /* 2. Configure scaler #0: 0..4095 -> 0.0..100.0 (clamped) */
    le_process_image_set_scaler(&img, 0, 0.0f, 4095.0f, 0.0f, 100.0f, true);

    le_instruction_t inst_scale = {
        .opcode = LE_OP_SCALE_F,
        .modifier = 0, /* scaler index 0 */
        .in_a = LE_ADDR_MAKE_AIN(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(0)
    };

    /* Midpoint: 2047.5 / 4095 = 50.0 */
    le_process_image_set_int(&img, LE_ADDR_MAKE_AIN(0), 2047);
    le_exec_instruction(&inst_scale, &img, 0);
    float out_val = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(0));
    TEST_ASSERT(fabsf(out_val - 49.98779f) < 1e-3f, "Midpoint scaling 2047/4095 -> ~50.0");

    /* Min: 0 -> 0.0 */
    le_process_image_set_int(&img, LE_ADDR_MAKE_AIN(0), 0);
    le_exec_instruction(&inst_scale, &img, 0);
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(0))) < 1e-5f, "Min scaling 0 -> 0.0");

    /* Max: 4095 -> 100.0 */
    le_process_image_set_int(&img, LE_ADDR_MAKE_AIN(0), 4095);
    le_exec_instruction(&inst_scale, &img, 0);
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(0)) - 100.0f) < 1e-4f, "Max scaling 4095 -> 100.0");

    /* Clamping test: 5000 -> clamped to 100.0 */
    le_process_image_set_int(&img, LE_ADDR_MAKE_AIN(0), 5000);
    le_exec_instruction(&inst_scale, &img, 0);
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(0)) - 100.0f) < 1e-4f, "Over-range clamped to 100.0");

    /* Clamping test: -100 -> clamped to 0.0 */
    le_process_image_set_int(&img, LE_ADDR_MAKE_AIN(0), -100);
    le_exec_instruction(&inst_scale, &img, 0);
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(0))) < 1e-5f, "Under-range clamped to 0.0");

    /* Scaler with negative offset: 0..1023 -> -50.0..150.0 */
    le_process_image_set_scaler(&img, 1, 0.0f, 1023.0f, -50.0f, 150.0f, true);
    inst_scale.modifier = 1;
    inst_scale.in_a = LE_ADDR_MAKE_AIN(1);
    inst_scale.out = LE_ADDR_MAKE_FLOAT(1);

    le_process_image_set_int(&img, LE_ADDR_MAKE_AIN(1), 511);
    le_exec_instruction(&inst_scale, &img, 0);
    float out_val2 = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(out_val2 - 49.90225f) < 1e-2f, "Bipolar/offset scaling works accurately");

    /* 3. Test HAL sim adc_read_raw */
    void le_sim_set_adc_raw(uint8_t channel, uint32_t value);
    le_sim_set_adc_raw(2, 3300);
    const le_hal_t* sim = le_hal_get_sim();
    TEST_ASSERT(sim->adc_read_raw(2) == 3300, "HAL sim_adc_read_raw returned 3300");
}

#if LE_ENABLE_DSP
void test_dsp_filters(void)
{
    printf("Running test_dsp_filters...\n");
    test_rt_reset();
    le_process_image_t img;
    test_img_init(&img);
    test_rt_reset(); /* clean state arena for this test */
    test_bind_kind(LE_BLK_LPF, 1, sizeof(le_lpf_state_t));
    test_bind_kind(LE_BLK_BIQUAD, 1, sizeof(le_biquad_state_t));
    test_bind_kind(LE_BLK_MOVING_AVG, 1, sizeof(le_moving_avg_state_t));
    test_bind_kind(LE_BLK_RATE_LIMITER, 1, sizeof(le_rate_limiter_state_t));
    test_bind_kind(LE_BLK_DEADBAND, 1, sizeof(le_deadband_state_t));
    test_bind_kind(LE_BLK_WASHOUT, 1, sizeof(le_washout_state_t));
    test_bind_kind(LE_BLK_PEAK, 1, sizeof(le_peak_state_t));
    test_bind_kind(LE_BLK_RMS, 1, sizeof(le_rms_state_t));
    test_bind_kind(LE_BLK_MEDIAN, 1, sizeof(le_median_state_t));
    test_bind_kind(LE_BLK_DERIVATIVE, 1, sizeof(le_derivative_state_t));
    test_bind_kind(LE_BLK_ZERO_CROSSING, 1, sizeof(le_zero_crossing_state_t));
    test_bind_kind(LE_BLK_LUT_1D, 1, sizeof(le_lut_1d_state_t));
    test_bind_kind(LE_BLK_TOTALIZER, 1, sizeof(le_totalizer_state_t));
    test_bind_kind(LE_BLK_MIN_MAX_HOLD, 1, sizeof(le_min_max_hold_state_t));
    test_bind_kind(LE_BLK_SCALER, 1, sizeof(le_scale_state_t));

    /* 1. Test Low-Pass Filter (LPF_1P) */
    le_process_image_set_lpf(&img, 0, 0.2f);
    le_instruction_t inst_lpf = {
        .opcode = LE_OP_LPF_1P,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(1)
    };

    /* Cycle 1: initial value 100.0 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 100.0f);
    le_exec_instruction(&inst_lpf, &img, 0);
    float lpf_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(lpf_out - 100.0f) < 1e-4f, "LPF cycle 1 initializes to input 100.0");

    /* Cycle 2: step to 200.0 -> 100 + 0.2*(200 - 100) = 120.0 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 200.0f);
    le_exec_instruction(&inst_lpf, &img, 10);
    lpf_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(lpf_out - 120.0f) < 1e-4f, "LPF cycle 2 step response is 120.0");

    /* Cycle 3: 120 + 0.2*(200 - 120) = 136.0 */
    le_exec_instruction(&inst_lpf, &img, 20);
    lpf_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(1));
    TEST_ASSERT(fabsf(lpf_out - 136.0f) < 1e-4f, "LPF cycle 3 step response is 136.0");

    /* 2. Test 2nd-Order Biquad IIR Filter (BIQUAD_IIR) */
    /* Simple 2nd-order moving average: b0=0.25, b1=0.5, b2=0.25, a1=0, a2=0 */
    le_process_image_set_biquad(&img, 0, 0.25f, 0.5f, 0.25f, 0.0f, 0.0f);
    le_instruction_t inst_biquad = {
        .opcode = LE_OP_BIQUAD_IIR,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(2)
    };

    /* Cycle 1: in = 10.0 -> w=10, y=2.5 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 10.0f);
    le_exec_instruction(&inst_biquad, &img, 0);
    float biq_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(2));
    TEST_ASSERT(fabsf(biq_out - 2.5f) < 1e-4f, "Biquad cycle 1 response is 2.5");

    /* Cycle 2: in = 10.0 -> w=10, y=0.25*10 + 0.5*10 = 7.5 */
    le_exec_instruction(&inst_biquad, &img, 10);
    biq_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(2));
    TEST_ASSERT(fabsf(biq_out - 7.5f) < 1e-4f, "Biquad cycle 2 response is 7.5");

    /* Cycle 3: in = 10.0 -> w=10, y=0.25*10 + 0.5*10 + 0.25*10 = 10.0 */
    le_exec_instruction(&inst_biquad, &img, 20);
    biq_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(2));
    TEST_ASSERT(fabsf(biq_out - 10.0f) < 1e-4f, "Biquad cycle 3 steady state reaches 10.0");

    /* 3. Test Moving Average Filter (MOVING_AVG) */
    le_process_image_set_moving_avg(&img, 0, 4);
    le_instruction_t inst_mavg = {
        .opcode = LE_OP_MOVING_AVG,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(3)
    };

    float samples[4] = { 10.0f, 20.0f, 30.0f, 40.0f };
    for (int i = 0; i < 4; i++) {
        le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), samples[i]);
        le_exec_instruction(&inst_mavg, &img, i * 10);
    }
    float mavg_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(3));
    TEST_ASSERT(fabsf(mavg_out - 25.0f) < 1e-4f, "Moving average of 10,20,30,40 is 25.0");

    /* Add 5th sample 50.0 -> oldest (10.0) discarded -> (20+30+40+50)/4 = 35.0 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 50.0f);
    le_exec_instruction(&inst_mavg, &img, 40);
    mavg_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(3));
    TEST_ASSERT(fabsf(mavg_out - 35.0f) < 1e-4f, "Moving average circular overwrite yields 35.0");

    /* 4. Test Slew Rate Limiter (RATE_LIMITER) */
    le_process_image_set_rate_limiter(&img, 0, 5.0f, 2.0f);
    le_instruction_t inst_slew = {
        .opcode = LE_OP_RATE_LIMITER,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(4)
    };

    /* Cycle 1: initial 0.0 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 0.0f);
    le_exec_instruction(&inst_slew, &img, 0);
    float slew_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(4));
    TEST_ASSERT(fabsf(slew_out - 0.0f) < 1e-4f, "Rate limiter initializes to 0.0");

    /* Jump to 100.0 -> max rise is 5.0 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 100.0f);
    le_exec_instruction(&inst_slew, &img, 10);
    slew_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(4));
    TEST_ASSERT(fabsf(slew_out - 5.0f) < 1e-4f, "Rate limiter clamped to +5.0");

    /* Next cycle jumps again to 100.0 -> 5.0 + 5.0 = 10.0 */
    le_exec_instruction(&inst_slew, &img, 20);
    slew_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(4));
    TEST_ASSERT(fabsf(slew_out - 10.0f) < 1e-4f, "Rate limiter clamped to +10.0");

    /* Target drops to 0.0 -> max fall is -2.0 -> 10.0 - 2.0 = 8.0 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 0.0f);
    le_exec_instruction(&inst_slew, &img, 30);
    slew_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(4));
    TEST_ASSERT(fabsf(slew_out - 8.0f) < 1e-4f, "Rate limiter clamped to -2.0 (8.0)");

    /* 5. Test Deadband Filter (DEADBAND) */
    le_process_image_set_deadband(&img, 0, 5.0f, 0.0f);
    le_instruction_t inst_dead = {
        .opcode = LE_OP_DEADBAND,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(5)
    };

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 3.0f);
    le_exec_instruction(&inst_dead, &img, 0);
    float dead_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(5));
    TEST_ASSERT(fabsf(dead_out) < 1e-4f, "Deadband suppresses +3.0 to 0.0");

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), -4.5f);
    le_exec_instruction(&inst_dead, &img, 10);
    dead_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(5));
    TEST_ASSERT(fabsf(dead_out) < 1e-4f, "Deadband suppresses -4.5 to 0.0");

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 12.0f);
    le_exec_instruction(&inst_dead, &img, 20);
    dead_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(5));
    TEST_ASSERT(fabsf(dead_out - 7.0f) < 1e-4f, "Deadband +12.0 yields +7.0");

    /* 6. Test Washout Filter (WASHOUT) */
    le_process_image_set_washout(&img, 0, 0.8f);
    le_instruction_t inst_wash = {
        .opcode = LE_OP_WASHOUT,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(6)
    };

    /* Continuous DC of 50.0 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 50.0f);
    le_exec_instruction(&inst_wash, &img, 0);
    le_exec_instruction(&inst_wash, &img, 10);
    float wash_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(6));
    TEST_ASSERT(fabsf(wash_out) < 1e-4f, "Washout filter blocks steady-state DC");

    /* Step from 50.0 to 100.0 (delta = 50) -> y = 0.8 * (0 + 100 - 50) = 40.0 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 100.0f);
    le_exec_instruction(&inst_wash, &img, 20);
    wash_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(6));
    TEST_ASSERT(fabsf(wash_out - 40.0f) < 1e-4f, "Washout produces transient spike on step change");

    /* 7. Test Peak / Envelope Follower (PEAK_DETECTOR) */
    le_process_image_set_peak(&img, 0, 0.9f);
    le_instruction_t inst_peak = {
        .opcode = LE_OP_PEAK_DETECTOR,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(7)
    };

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), -80.0f);
    le_exec_instruction(&inst_peak, &img, 0);
    float peak_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(7));
    TEST_ASSERT(fabsf(peak_out - 80.0f) < 1e-4f, "Peak detector tracks absolute magnitude 80.0");

    /* Input drops to 10.0 -> peak decays to 80.0 * 0.9 = 72.0 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 10.0f);
    le_exec_instruction(&inst_peak, &img, 10);
    peak_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(7));
    TEST_ASSERT(fabsf(peak_out - 72.0f) < 1e-4f, "Peak detector exponential decay reaches 72.0");

    /* 8. Test True RMS Meter (RMS) */
    le_process_image_set_rms(&img, 0, 4);
    le_instruction_t inst_rms = {
        .opcode = LE_OP_RMS,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(8)
    };

    /* Feed alternating +12.0 and -12.0 square wave */
    float sq_wave[4] = { 12.0f, -12.0f, 12.0f, -12.0f };
    for (int i = 0; i < 4; i++) {
        le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), sq_wave[i]);
        le_exec_instruction(&inst_rms, &img, i * 10);
    }
    float rms_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(8));
    TEST_ASSERT(fabsf(rms_out - 12.0f) < 1e-4f, "True RMS of +/-12.0V square wave is 12.0V");

    /* 9. Test Median Filter (MEDIAN) */
    le_process_image_set_median(&img, 0, 5);
    le_instruction_t inst_median = {
        .opcode = LE_OP_MEDIAN,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(9)
    };
    float med_samples[5] = { 10.0f, 11.0f, 100.0f, 12.0f, 11.5f }; /* 100.0 is an outlier spike */
    for (int i = 0; i < 5; i++) {
        le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), med_samples[i]);
        le_exec_instruction(&inst_median, &img, i * 10);
    }
    float med_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(9));
    TEST_ASSERT(fabsf(med_out - 11.5f) < 1e-4f, "Median filter rejects 100.0 spike, outputs 11.5");

    /* 10. Test Filtered Derivative (DERIVATIVE) */
    le_process_image_set_derivative(&img, 0, 1.0f, 100.0f); /* Alpha=1.0 (unfiltered), Gain=100.0 (1/0.01s) */
    le_instruction_t inst_deriv = {
        .opcode = LE_OP_DERIVATIVE,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(10)
    };
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 0.0f);
    le_exec_instruction(&inst_deriv, &img, 0); /* Initializer cycle */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 5.0f);
    le_exec_instruction(&inst_deriv, &img, 10); /* Step +5.0 -> deriv = 5.0 * 100.0 = 500.0 */
    float deriv_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(10));
    TEST_ASSERT(fabsf(deriv_out - 500.0f) < 1e-3f, "Derivative computes rate of change 500.0");

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 5.0f);
    le_exec_instruction(&inst_deriv, &img, 20); /* Steady state -> deriv = 0.0 */
    deriv_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(10));
    TEST_ASSERT(fabsf(deriv_out - 0.0f) < 1e-3f, "Derivative is 0.0 on steady input");

    /* 11. Test Zero-Crossing Detector (ZERO_CROSSING) */
    le_process_image_set_zero_crossing(&img, 0, 0.5f, 1000.0f); /* Hysteresis = 0.5, Sample Rate = 1000 Hz */
    le_instruction_t inst_zc = {
        .opcode = LE_OP_ZERO_CROSSING,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(11)
    };
    /* Simulate 50 Hz square wave (period = 20 samples: 10 samples @ +2.0V, 10 samples @ -2.0V) */
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int i = 0; i < 10; i++) {
            le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 2.0f);
            le_exec_instruction(&inst_zc, &img, 0);
        }
        for (int i = 0; i < 10; i++) {
            le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), -2.0f);
            le_exec_instruction(&inst_zc, &img, 0);
        }
    }
    /* Final rising edge transition */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 2.0f);
    le_exec_instruction(&inst_zc, &img, 0);
    float zc_freq = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(11));
    TEST_ASSERT(fabsf(zc_freq - 50.0f) < 1e-2f, "Zero crossing detector measures 50.0 Hz");

    /* 12. Test 1D Lookup Table (LUT_1D) */
    float lut_x[3] = { 0.0f, 10.0f, 20.0f };
    float lut_y[3] = { 0.0f, 50.0f, 150.0f };
    le_process_image_set_lut_1d(&img, 0, lut_x, lut_y, 3);
    le_instruction_t inst_lut = {
        .opcode = LE_OP_LUT_1D,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_UNUSED,
        .out = LE_ADDR_MAKE_FLOAT(12)
    };
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 5.0f);
    le_exec_instruction(&inst_lut, &img, 0);
    float lut_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(12));
    TEST_ASSERT(fabsf(lut_out - 25.0f) < 1e-4f, "LUT_1D interpolates X=5.0 -> Y=25.0");

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 15.0f);
    le_exec_instruction(&inst_lut, &img, 0);
    lut_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(12));
    TEST_ASSERT(fabsf(lut_out - 100.0f) < 1e-4f, "LUT_1D interpolates X=15.0 -> Y=100.0");

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), -10.0f);
    le_exec_instruction(&inst_lut, &img, 0);
    lut_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(12));
    TEST_ASSERT(fabsf(lut_out - 0.0f) < 1e-4f, "LUT_1D clamps lower bound to 0.0");

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 30.0f);
    le_exec_instruction(&inst_lut, &img, 0);
    lut_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(12));
    TEST_ASSERT(fabsf(lut_out - 150.0f) < 1e-4f, "LUT_1D clamps upper bound to 150.0");

    /* 13. Test Totalizer (TOTALIZER) */
    le_process_image_set_totalizer(&img, 0, 1.0f, 1.0f, 0.1f, 1000.0f); /* Time base 1s, dt = 0.1s */
    le_instruction_t inst_tot = {
        .opcode = LE_OP_TOTALIZER,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_MAKE_BOOL_REG(0), /* Reset input */
        .out = LE_ADDR_MAKE_FLOAT(13)
    };
    le_process_image_set_bool(&img, LE_ADDR_MAKE_BOOL_REG(0), false);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 10.0f);
    for (int i = 0; i <= 10; i++) {
        le_exec_instruction(&inst_tot, &img, i * 100);
    }
    float tot_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(13));
    TEST_ASSERT(fabsf(tot_out - 10.0f) < 1e-2f, "Totalizer accumulates 10.0 units over 1.0 second");

    /* Test Reset */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_BOOL_REG(0), true);
    le_exec_instruction(&inst_tot, &img, 1100);
    tot_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(13));
    TEST_ASSERT(fabsf(tot_out - 0.0f) < 1e-4f, "Totalizer resets to 0.0 on reset signal");

    /* 14. Test Min / Max Peak Hold (MIN_MAX_HOLD) */
    le_process_image_set_min_max_hold(&img, 0, 2); /* Mode 2 = Span */
    le_instruction_t inst_mm = {
        .opcode = LE_OP_MIN_MAX_HOLD,
        .modifier = 0,
        .in_a = LE_ADDR_MAKE_FLOAT(0),
        .in_b = LE_ADDR_MAKE_BOOL_REG(0),
        .out = LE_ADDR_MAKE_FLOAT(14)
    };
    le_process_image_set_bool(&img, LE_ADDR_MAKE_BOOL_REG(0), false);
    float mm_vals[4] = { 5.0f, 20.0f, -10.0f, 15.0f };
    for (int i = 0; i < 4; i++) {
        le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), mm_vals[i]);
        le_exec_instruction(&inst_mm, &img, i * 10);
    }
    float span_out = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(14));
    TEST_ASSERT(fabsf(span_out - 30.0f) < 1e-4f, "MinMaxHold mode 2 (Span) is 20.0 - (-10.0) = 30.0");
}
#endif

void test_block_call(void)
{
    printf("Running test_block_call...\n");
    const le_hal_t* orig_hal = g_le_hal;
    le_hal_set(le_hal_get_sim());

    le_process_image_t img;
    test_img_init(&img);

    /* N-arity custom node (sim func 0x84 = 2-of-3 majority) via a BLOCK
     * descriptor: header followed by [in0, in1, in2, out]. */
    struct { le_block_desc_t d; uint16_t args[4]; } b;
    b.d.in_count = 3; b.d.out_count = 1; b.d.flags = 0; b.d.reserved = 0;
    b.args[0] = LE_ADDR_MAKE_DIN(0);
    b.args[1] = LE_ADDR_MAKE_DIN(1);
    b.args[2] = LE_ADDR_MAKE_DIN(2);
    b.args[3] = LE_ADDR_MAKE_DOUT(0);

    le_instruction_t inst = { LE_OP_BLOCK, 0x84, 0, LE_ADDR_UNUSED, LE_ADDR_UNUSED };

    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), false);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(1), true);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(2), true);
    TEST_ASSERT(le_exec_instruction_ex(&inst, &img, 0, &b.d, 1) == LE_OK,
                "2-of-3 majority block executed (0,1,1)");
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)) == true,
                "2-of-3 majority (0,1,1) -> true");

    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(2), false);
    TEST_ASSERT(le_exec_instruction_ex(&inst, &img, 0, &b.d, 1) == LE_OK,
                "2-of-3 majority block executed (0,1,0)");
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)) == false,
                "2-of-3 majority (0,1,0) -> false");

    /* Out-of-bounds descriptor index must be rejected. */
    le_instruction_t bad = { LE_OP_BLOCK, 0x84, 99, LE_ADDR_UNUSED, LE_ADDR_UNUSED };
    TEST_ASSERT(le_exec_instruction_ex(&bad, &img, 0, &b.d, 1) == LE_ERR_OUT_OF_BOUNDS,
                "LE_OP_BLOCK with OOB descriptor index rejected");

    le_hal_set(orig_hal);
}

void test_phasor_shift(void)
{
    printf("Running test_phasor_shift...\n");
    le_process_image_t img;
    test_img_init(&img);

    /* Rotate phasor (1 + i0) by 90 degrees CCW -> real component = 0. */
    le_instruction_t inst = { LE_OP_PHASOR_SHIFT, 90,
                              LE_ADDR_MAKE_FLOAT(0), LE_ADDR_MAKE_FLOAT(1), LE_ADDR_MAKE_FLOAT(2) };
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 1.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(1), 0.0f);
    TEST_ASSERT(le_exec_instruction(&inst, &img, 0) == LE_OK, "PHASOR_SHIFT executes with LE_OK");
    float real = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(2));
    TEST_ASSERT(fabsf(real) < 1e-4f, "PHASOR_SHIFT (1+i0) rotated 90deg -> real=0");

    /* Rotate (0 + i1) by 180 degrees -> real component = 0*cos(180) - 1*sin(180) = 0. */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 0.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(1), 1.0f);
    le_instruction_t inst180 = { LE_OP_PHASOR_SHIFT, 180,
                                 LE_ADDR_MAKE_FLOAT(0), LE_ADDR_MAKE_FLOAT(1), LE_ADDR_MAKE_FLOAT(2) };
    TEST_ASSERT(le_exec_instruction(&inst180, &img, 0) == LE_OK, "PHASOR_SHIFT 180 executes");
    float real180 = le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(2));
    TEST_ASSERT(fabsf(real180) < 1e-4f, "PHASOR_SHIFT (i) rotated 180deg -> real=0");
}

void test_phasor_block_builtins(void)
{
    printf("Running test_phasor_block_builtins...\n");
    le_process_image_t img;
    test_img_init(&img);
        test_rt_reset(); /* clean state arena for this test */
        test_bind_kind(LE_BLK_PHASOR, 1, sizeof(le_phasor_state_t));

    /* contiguous descriptor header + args for the block ops */
    struct { le_block_desc_t d; uint16_t a[8]; } b;
    b.d.flags = 0; b.d.reserved = 0;

    /* RECT2POLAR: (3,4) -> mag=5, angle=atan2(4,3) */
    b.d.in_count = 2; b.d.out_count = 2;
    b.a[0] = LE_ADDR_MAKE_FLOAT(0); b.a[1] = LE_ADDR_MAKE_FLOAT(1);
    b.a[2] = LE_ADDR_MAKE_FLOAT(2); b.a[3] = LE_ADDR_MAKE_FLOAT(3);
    le_instruction_t r2p = { LE_OP_BLOCK, LE_FUNC_RECT2POLAR, 0, LE_ADDR_UNUSED, LE_ADDR_UNUSED };
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 3.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(1), 4.0f);
    TEST_ASSERT(le_exec_instruction_ex(&r2p, &img, 0, &b.d, 1) == LE_OK, "RECT2POLAR block executes");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(2)) - 5.0f) < 1e-4f, "RECT2POLAR mag=5");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(3)) - atan2f(4, 3)) < 1e-4f, "RECT2POLAR angle=atan2(4,3)");

    /* POLAR2RECT: (5, angle) -> real=3, imag=4 */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 5.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(1), atan2f(4, 3));
    le_instruction_t p2r = { LE_OP_BLOCK, LE_FUNC_POLAR2RECT, 0, LE_ADDR_UNUSED, LE_ADDR_UNUSED };
    TEST_ASSERT(le_exec_instruction_ex(&p2r, &img, 0, &b.d, 1) == LE_OK, "POLAR2RECT block executes");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(2)) - 3.0f) < 1e-4f, "POLAR2RECT real=3");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(3)) - 4.0f) < 1e-4f, "POLAR2RECT imag=4");

    /* PHASOR_SHIFT: (1,0) rotate +90deg -> (0,1). 3-in/2-out. */
    b.d.in_count = 3; b.d.out_count = 2;
    b.a[0] = LE_ADDR_MAKE_FLOAT(0); b.a[1] = LE_ADDR_MAKE_FLOAT(1); b.a[2] = LE_ADDR_MAKE_FLOAT(2);
    b.a[3] = LE_ADDR_MAKE_FLOAT(3); b.a[4] = LE_ADDR_MAKE_FLOAT(4);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 1.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(1), 0.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(2), (float)M_PI / 2.0f);
    le_instruction_t ps = { LE_OP_BLOCK, LE_FUNC_PHASOR_SHIFT, 0, LE_ADDR_UNUSED, LE_ADDR_UNUSED };
    TEST_ASSERT(le_exec_instruction_ex(&ps, &img, 0, &b.d, 1) == LE_OK, "PHASOR_SHIFT block executes");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(3))) < 1e-4f, "PHASOR_SHIFT real'=0");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(4)) - 1.0f) < 1e-4f, "PHASOR_SHIFT imag'=1");

    /* PHASOR_1P sync: complex phasor output (2-in/1-out):
     * args = [sample, sync_complex, out_complex]. The raw phasor has its angle
     * referenced to the sync phasor and magnitude normalized by the sync
     * magnitude, so the result stays stable relative to the sync phasor. */
    test_img_init(&img); /* reset phasor state */
    const uint16_t N = 16; float ampA = 10.0f;
    b.d.in_count = 2; b.d.out_count = 1;
    b.a[0] = LE_ADDR_MAKE_FLOAT(0);          /* sample wire */
    b.a[1] = LE_ADDR_MAKE_CMPLX(0);          /* sync phasor */
    b.a[2] = LE_ADDR_MAKE_CMPLX(1);          /* out phasor */
    ((le_phasor_state_t*)le_process_image_kind_state(&img, LE_BLK_PHASOR, 0))->samples_per_cycle = N;
    /* Sync phasor = 10<30deg (matches raw phasor), so mag normalizes to ~1 and
     * relative angle ~0. */
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(0),
        le_c_polar(ampA, 30.0f * (float)M_PI / 180.0f));
    le_instruction_t p1p = { LE_OP_BLOCK, LE_FUNC_PHASOR_1P, 0, LE_ADDR_UNUSED, LE_ADDR_UNUSED };

    float phi = 30.0f * (float)M_PI / 180.0f;
    for (uint16_t k = 0; k < N; k++) {
        le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0),
                                   ampA * cosf(2.0f * (float)M_PI * (float)k / (float)N + phi));
        TEST_ASSERT(le_exec_instruction_ex(&p1p, &img, 0, &b.d, 1) == LE_OK, "PHASOR_1P block step");
    }
    le_complex_t ph1 = le_process_image_get_complex(&img, LE_ADDR_MAKE_CMPLX(1));
    TEST_ASSERT(fabsf(le_c_mag(ph1) - 1.0f) < 0.05f, "PHASOR_1P synced mag normalized ~1");
    TEST_ASSERT(fabsf(le_c_ang(ph1)) < 0.05f, "PHASOR_1P relative angle ~0");

    phi = 60.0f * (float)M_PI / 180.0f; /* system phase advances + sync phasor tracks it */
    le_process_image_set_complex(&img, LE_ADDR_MAKE_CMPLX(0),
        le_c_polar(ampA, 60.0f * (float)M_PI / 180.0f));
    for (uint16_t k = 0; k < N; k++) {
        le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0),
                                   ampA * cosf(2.0f * (float)M_PI * (float)k / (float)N + phi));
        TEST_ASSERT(le_exec_instruction_ex(&p1p, &img, 0, &b.d, 1) == LE_OK, "PHASOR_1P block step 2");
    }
    le_complex_t ph2 = le_process_image_get_complex(&img, LE_ADDR_MAKE_CMPLX(1));
    TEST_ASSERT(fabsf(le_c_mag(ph2) - 1.0f) < 0.05f &&
                fabsf(le_c_ang(ph2)) < 0.05f,
                "PHASOR_1P relative angle stays ~0 after phase advance (sync prevents rotation)");
}

void test_multi_block_conversions(void)
{
    /* Verifies variable-arity block resolution with MULTIPLE block descriptors.
     * Each block is variable-size, so the runtime must offset-walk to the index
     * (a fixed-stride index previously corrupted blocks beyond the first). */
    printf("Running test_multi_block_conversions (RECT2COMPLEX/COMPLEX2RECT/POLAR2COMPLEX/CLAMP)...\n");
    le_process_image_t img;
    test_img_init(&img);

    /* Two blocks in one table, contiguous: blk0 = RECT2COMPLEX (2-in/1-out),
     * blk1 = POLAR2COMPLEX (2-in/1-out). */
    uint8_t tbl[LE_BLOCK_DESC_HEADER_BYTES * 2 + sizeof(uint16_t) * 7] = {0};
    le_block_desc_t* d0 = (le_block_desc_t*)tbl;
    d0->in_count = 2; d0->out_count = 1;
    uint16_t* a0 = (uint16_t*)(d0 + 1);
    a0[0] = LE_ADDR_MAKE_FLOAT(0); a0[1] = LE_ADDR_MAKE_FLOAT(1); a0[2] = LE_ADDR_MAKE_CMPLX(0);
    le_block_desc_t* d1 = (le_block_desc_t*)(tbl + LE_BLOCK_DESC_HEADER_BYTES + 3 * sizeof(uint16_t));
    d1->in_count = 2; d1->out_count = 1;
    uint16_t* a1 = (uint16_t*)(d1 + 1);
    a1[0] = LE_ADDR_MAKE_FLOAT(4); a1[1] = LE_ADDR_MAKE_FLOAT(5); a1[2] = LE_ADDR_MAKE_CMPLX(1);

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 3.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(1), 4.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(4), 5.0f);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(5), atan2f(4, 3));

    le_instruction_t blk0 = { LE_OP_BLOCK, LE_FUNC_RECT2COMPLEX, 0, LE_ADDR_UNUSED, LE_ADDR_UNUSED };
    TEST_ASSERT(le_exec_instruction_ex(&blk0, &img, 0, (le_block_desc_t*)tbl, 2) == LE_OK, "block[0] RECT2COMPLEX executes");
    le_complex_t c0 = le_process_image_get_complex(&img, LE_ADDR_MAKE_CMPLX(0));
    TEST_ASSERT(fabsf(c0.r - 3.0f) < 1e-4f && fabsf(c0.i - 4.0f) < 1e-4f, "block[0] RECT2COMPLEX = 3+4j");

    le_instruction_t blk1 = { LE_OP_BLOCK, LE_FUNC_POLAR2COMPLEX, 1, LE_ADDR_UNUSED, LE_ADDR_UNUSED };
    TEST_ASSERT(le_exec_instruction_ex(&blk1, &img, 0, (le_block_desc_t*)tbl, 2) == LE_OK, "block[1] POLAR2COMPLEX executes");
    le_complex_t c1 = le_process_image_get_complex(&img, LE_ADDR_MAKE_CMPLX(1));
    TEST_ASSERT(fabsf(c1.r - 3.0f) < 1e-4f && fabsf(c1.i - 4.0f) < 1e-4f, "block[1] POLAR2COMPLEX ~= 3+4j");

    /* CLAMP: [value,min,max] -> out with proper independent bounds. */
    d1->in_count = 3; d1->out_count = 1;
    a1[0] = LE_ADDR_MAKE_FLOAT(6); a1[1] = LE_ADDR_MAKE_FLOAT(7); a1[2] = LE_ADDR_MAKE_FLOAT(8);
    a1[3] = LE_ADDR_MAKE_FLOAT(9);
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(6), 100.0f);  /* value */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(7), -5.0f);   /* min */
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(8),  5.0f);   /* max */
    le_instruction_t cl = { LE_OP_BLOCK, LE_FUNC_CLAMP_F, 1, LE_ADDR_UNUSED, LE_ADDR_UNUSED };
    TEST_ASSERT(le_exec_instruction_ex(&cl, &img, 0, (le_block_desc_t*)tbl, 2) == LE_OK, "CLAMP block executes");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(9)) - 5.0f) < 1e-4f, "CLAMP hi -> max 5");
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(6), -42.0f);
    TEST_ASSERT(le_exec_instruction_ex(&cl, &img, 0, (le_block_desc_t*)tbl, 2) == LE_OK, "CLAMP block executes low");
    TEST_ASSERT(fabsf(le_process_image_get_float(&img, LE_ADDR_MAKE_FLOAT(9)) - (-5.0f)) < 1e-4f, "CLAMP lo -> min -5");
}

void test_counter_opcode(void)
{
    printf("Running test_counter_opcode (CTU/CTD/CTUD)...\n");
    le_process_image_t img;
    test_img_init(&img);
        test_rt_reset(); /* clean state arena for this test */
        test_bind_kind(LE_BLK_COUNTER, 3, sizeof(le_counter_state_t));

    /* ---- CTU: count up to preset (3) ---- */
    le_process_image_counter(&img, 0)->preset = 3;
    le_instruction_t ctu = { LE_OP_CTU, 0, LE_ADDR_MAKE_DIN(0), LE_ADDR_UNUSED, LE_ADDR_MAKE_COUNTER(0) };
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_exec_instruction(&ctu, &img, 0);                       /* edge1 -> count=1 */
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_COUNTER(0)), "CTU not done at count=1");
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), false);
    le_exec_instruction(&ctu, &img, 1);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_exec_instruction(&ctu, &img, 2);                       /* edge2 -> count=2 */
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), false);
    le_exec_instruction(&ctu, &img, 3);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_exec_instruction(&ctu, &img, 4);                       /* edge3 -> count=3 */
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_COUNTER(0)), "CTU done at count=3");
    TEST_ASSERT(le_process_image_counter(&img, 0)->count == 3, "CTU count == 3");

    /* CTU reset via second input (in_b) */
    le_instruction_t ctu_r = { LE_OP_CTU, 0, LE_ADDR_MAKE_DIN(0), LE_ADDR_MAKE_DIN(1), LE_ADDR_MAKE_COUNTER(0) };
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), false);
    le_exec_instruction(&ctu_r, &img, 5);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(1), true);  /* reset rising edge */
    le_exec_instruction(&ctu_r, &img, 6);
    TEST_ASSERT(le_process_image_counter(&img, 0)->count == 0, "CTU reset clears count");

    /* ---- CTD: count down to 0 ---- */
    test_img_init(&img);
    le_process_image_counter(&img, 1)->preset = 2;
    le_process_image_counter(&img, 1)->count = 2;
    le_instruction_t ctd = { LE_OP_CTD, 0, LE_ADDR_MAKE_DIN(0), LE_ADDR_UNUSED, LE_ADDR_MAKE_COUNTER(1) };
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_exec_instruction(&ctd, &img, 0);                       /* edge1 -> 2->1 */
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_COUNTER(1)), "CTD not done at count=1");
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), false);
    le_exec_instruction(&ctd, &img, 1);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);
    le_exec_instruction(&ctd, &img, 2);                       /* edge2 -> 1->0 */
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_COUNTER(1)), "CTD done at count=0");
    TEST_ASSERT(le_process_image_counter(&img, 1)->count == 0, "CTD count == 0");

    /* ---- CTUD: count up (in_a) / count down (in_b) ---- */
    test_img_init(&img);
    le_process_image_counter(&img, 2)->preset = 1;
    le_instruction_t cud = { LE_OP_CTUD, 0, LE_ADDR_MAKE_DIN(0), LE_ADDR_MAKE_DIN(1), LE_ADDR_MAKE_COUNTER(2) };
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), true);   /* cu rising -> count=1 */
    le_exec_instruction(&cud, &img, 0);
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_COUNTER(2)), "CTUD done at count>=preset(1)");
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(0), false);
    le_exec_instruction(&cud, &img, 1);
    le_process_image_set_bool(&img, LE_ADDR_MAKE_DIN(1), true);   /* cd rising -> count=0 */
    le_exec_instruction(&cud, &img, 2);
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_COUNTER(2)), "CTUD count down to 0 -> not done");
    TEST_ASSERT(le_process_image_counter(&img, 2)->count == 0, "CTUD count == 0");
}

void test_heap_allocator(void)
{
    printf("Running test_heap_allocator (state workspace)...\n");
    test_rt_reset();

    /* Place a timer block into the workspace and resolve it by kind/index. */
    le_timer_state_t t = {0};
    t.preset_ms = 100;
    memcpy(le_rt_workspace(), &t, sizeof(t));
    le_rt_set_kind_base(LE_BLK_TIMER, 0);
    le_timer_state_t* ts = le_process_image_timer(NULL, 0);
    TEST_ASSERT(ts != NULL, "timer resolved from the workspace");
    TEST_ASSERT(ts->preset_ms == 100, "state written into the workspace is readable");

    /* Blocks of different kinds sit at distinct offsets. */
    uint16_t toff = le_rt_kind_base(LE_BLK_TIMER);
    le_rt_set_kind_base(LE_BLK_COUNTER, toff + sizeof(le_timer_state_t));
    le_counter_state_t* cs = le_process_image_counter(NULL, 0);
    TEST_ASSERT(cs != NULL && (uint8_t*)cs > (uint8_t*)ts, "counter block offset differs from timer");
    TEST_ASSERT(le_rt_kind_base(LE_BLK_TIMER) != le_rt_kind_base(LE_BLK_COUNTER),
                "distinct kinds occupy distinct workspace offsets");

    /* le_rt_state bounds-checks: out-of-workspace index resolves to NULL. */
    TEST_ASSERT(le_rt_state(LE_BLK_TIMER, 0u) != NULL, "block 0 of a placed kind resolves");
    TEST_ASSERT(le_rt_state(LE_BLK_TIMER, 0xFFFFu) == NULL, "out-of-workspace index is rejected");

    /* Workspace capacity is fixed by the platform. */
    TEST_ASSERT(le_rt_workspace_bytes() == LE_RAM_WORKSPACE_BYTES, "workspace size matches config");

    /* le_rt_state rejects a kind that was never placed. */
    TEST_ASSERT(le_rt_state(LE_BLK_LPF, 0) == NULL, "unbound kind resolves to NULL");
}

void test_overcurrent(void)
{
    printf("Running test_overcurrent (ANSI 51)...\n");
    le_process_image_t img;
    test_img_init(&img);
        test_rt_reset(); /* clean state arena for this test */
        test_bind_kind(LE_BLK_OVERCURRENT, 1, sizeof(le_overcurrent_state_t));

    le_overcurrent_state_t* ocs = (le_overcurrent_state_t*)le_process_image_kind_state(&img, LE_BLK_OVERCURRENT, 0);
    ocs->pickup = 1.0f;
    ocs->time_dial = 0.05f;

    le_instruction_t oc = { LE_OP_OVERCURRENT, 0, LE_ADDR_MAKE_FLOAT(0), LE_ADDR_UNUSED, LE_ADDR_MAKE_DOUT(0) };
    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 0.5f);   /* below pickup */
    le_exec_instruction(&oc, &img, 0);
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "OVERCURRENT: 0.5pu does not trip");

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 2.0f);   /* 2pu sustained */
    for (int i = 0; i < 20; i++) le_exec_instruction(&oc, &img, (uint32_t)(i + 1));
    TEST_ASSERT(le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "OVERCURRENT: sustained 2pu trips");

    le_process_image_set_float(&img, LE_ADDR_MAKE_FLOAT(0), 0.0f);   /* clear */
    for (int i = 0; i < 40; i++) le_exec_instruction(&oc, &img, (uint32_t)(i + 100));
    TEST_ASSERT(!le_process_image_get_bool(&img, LE_ADDR_MAKE_DOUT(0)), "OVERCURRENT: clears after trip");
}

int main(void)
{
    printf("============================================================\n");
    printf(" LOGICELEMENTS C RUNTIME TEST SUITE\n");
    printf("============================================================\n");

    test_process_image();
    test_heap_allocator();
    test_basic_opcodes();
    test_analog_inputs_and_scaling();
    test_timer_opcode();
    test_counter_opcode();
    test_embedded_binary_execution();
    test_comms_protocol();
#if LE_ENABLE_PROTECTION
    test_protection_relays();
#endif
    test_storage_and_terminal_cli();
#if LE_ENABLE_SERIAL_BUS
    test_serial_bus_i2c_spi();
#endif
#if LE_ENABLE_DSP
    test_dsp_filters();
#endif
    test_board_capabilities_query();
    test_custom_nodes_and_ext_call();
    test_block_call();
    test_phasor_shift();
    test_phasor_block_builtins();
    test_multi_block_conversions();
    test_overcurrent();

    printf("============================================================\n");
    printf(" RESULTS: %d PASSED, %d FAILED\n", g_tests_passed, g_tests_failed);
    printf("============================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}




