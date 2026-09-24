# LogicElements Compiler and Optimizer Guide

This document describes the architecture, optimization pipeline, application programming interfaces (APIs), and data formats for the canonical LogicElements compiler engine.

---

## Architectural overview

The LogicElements compiler translates high-level graphical circuit schematics or JSON circuit representations into compact, deterministic bytecode (`.lebin`) targeted at embedded microcontrollers.

The compilation engine is implemented in standard C++17 (`src/compiler/src/le_compiler_core.cpp` and `le_optimizer.cpp`) and exposes:
- A pure **C API** (`include/le_compiler.h`) for integration with runtime loaders, external applications, and shared libraries.
- A **Python SDK** (`tools/compiler/le_compiler.py`) using `ctypes` for scripting, board capability profiling, and automated testing.
- A standalone **CLI executable** (`le_compile`) for automated build pipelines and firmware embedding.
- Direct static library linking (`le_compiler_static.lib`) into desktop applications such as **LogicElementsUI**.

### Compilation pipeline

```
┌────────────────────────────────────────────────────────┐
│               Input Circuit JSON                       │
│  - Elements: Digital I/O, Gates, Math, Regs, Custom    │
│  - Nets: Port-to-port connections                      │
│  - Target Board Profile JSON (.leconfig)               │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│            1. Parsing and Validation                   │
│  - Validate circuit structure and net connectivity     │
│  - Verify limits against board profile                 │
│    (DIN, DOUT, AIN, Coils, Floats, Timers, Counters)   │
│  - Verify feature support (Protection relays, Buses)   │
│  - Resolve symbolic pin aliases (%I0, %Q0)             │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│            2. Intermediate Representation (IR)         │
│  - Construct dependency graph with producer/consumer   │
│  - Track hardware side effects (DOUT, flash, latches)  │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│            3. Fixed-Point Multi-Pass Optimizer         │
│  - Run iterative pass loop until convergence (-O1..-Os)│
│  - Constant Folding & Algebraic Simplification         │
│  - Double Negation Elimination                         │
│  - Hardware Inversion Folding                          │
│  - Common Subexpression Elimination (CSE)              │
│  - Direct Destination Coalescing (DDC)                 │
│  - Dead Code Elimination (DCE)                         │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│            4. Liveness Analysis & Register Allocation  │
│  - Topological sort of optimized dependency graph      │
│  - Track live ranges for intermediate values           │
│  - Greedy register reuse for temporary bools/floats    │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│            5. Code Generation & Formatting             │
│  - Pack 32-byte binary header with CRC32 checksum      │
│  - Emit 8-byte binary instructions (.lebin)            │
│  - Generate human-readable disassembly text            │
│  - Generate flash-embeddable C static array header     │
└────────────────────────────────────────────────────────┘
```

---

## Optimization levels

The compiler supports four standard optimization levels configured via `le_opt_level_t`:

| Level | Name | Description | Passes applied |
| :--- | :--- | :--- | :--- |
| `-O0` | None | Preserves 1:1 schematic node-to-instruction mapping for interactive debugging. | None |
| `-O1` | Basic | Performs safe structural cleanups without modifying gate semantics. | Direct Destination Coalescing, Dead Code Elimination |
| `-O2` | Full | Default production optimization. Maximizes throughput and minimizes size. | Constant Folding, Inversion Folding, Double Negation, CSE, Destination Coalescing, Dead Code Elimination |
| `-Os` | Size | Aggressive size optimization targeting constrained microcontroller flash slots. | All `-O2` passes executed with maximum pass limit (10 iterations) |

To initialize compiler options programmatically:

```c
#include "le_compiler_options.h"

// Default full optimization (-O2):
le_compiler_options_t options = le_compiler_options_init(LE_OPT_LEVEL_2);

// Debug mode (-O0):
le_compiler_options_t dbg_options = le_compiler_options_init(LE_OPT_LEVEL_0);
```

---

## Multi-pass optimizer details

The multi-pass optimizer executes an iterative fixed-point loop. When any pass alters the IR graph, the loop re-executes up to the configured `max_pass_iterations` (default: 5 for `-O2`, 10 for `-Os`).

### 1. Constant folding and algebraic simplification

Evaluates operations whose inputs are statically known at compile time, eliminating runtime CPU cycles.

- **Boolean logic**:
  - `AND(x, FALSE)` $ightarrow$ `FALSE`
  - `AND(x, TRUE)` $ightarrow$ `x`
  - `OR(x, TRUE)` $ightarrow$ `TRUE`
  - `OR(x, FALSE)` $ightarrow$ `x`
  - `XOR(x, FALSE)` $ightarrow$ `x`
  - `XOR(x, TRUE)` $ightarrow$ `NOT(x)`
- **Float arithmetic**:
  - `ADD_F(x, 0.0f)` $ightarrow$ `x`
  - `SUB_F(x, 0.0f)` $ightarrow$ `x`
  - `MUL_F(x, 1.0f)` $ightarrow$ `x`
  - `MUL_F(x, 0.0f)` $ightarrow$ `0.0f`
  - `DIV_F(x, 1.0f)` $ightarrow$ `x`
  - Statically evaluates two constant inputs: `ADD_F(2.0, 3.0)` $ightarrow$ `5.0`.

### 2. Double negation elimination

Detects consecutive NOT gates in a data path and collapses them to a direct connection:

```
[IN0] ──► [NOT 1] ──► [NOT 2] ──► [OUT0]
               Collapsed to:
[IN0] ──────────────────────────► [OUT0]
```

### 3. Hardware inversion folding

Microcontroller VM instructions contain operand modifier flags (`LE_MOD_INVERT_A` = `0x01`, `LE_MOD_INVERT_B` = `0x02`). When a NOT gate feeds into a compatible gate (AND, OR, XOR), the optimizer folds the inversion into the consumer's modifier byte and deletes the standalone NOT instruction:

```
[IN0] ──► [NOT] ──► (in_a) [AND] ──► [OUT0]
[IN1] ────────────► (in_b)
               Folded to:
[IN0] ────────────► (in_a) [AND: mod=LE_MOD_INVERT_A] ──► [OUT0]
[IN1] ────────────► (in_b)
```

> [!NOTE]
> Hardware inversion folding reduces instruction count by 1 and eliminates 1 temporary boolean register.

### 4. Common subexpression elimination (CSE)

Identifies duplicate computations receiving identical inputs within the same execution cycle:

```
[IN0] ──┬──► [AND 1] ──► [DOUT 0]
[IN1] ──┼──►
        │
        ├──► [AND 2] ──► [DOUT 1]
        └──► (Identical inputs)
```

The optimizer detects that `AND 1` and `AND 2` perform identical operations on commutative inputs, removes `AND 2`, and rewires `DOUT 1` to read directly from `AND 1`.

### 5. Direct destination coalescing (DDC)

When a gate's output drives a digital output (`%Q`) or user register (`%M`, `%R`), unoptimized compilation creates an intermediate temporary register and a trailing `MOVE` instruction:

```
Unoptimized (-O0):
  OP_AND   DIN[0], DIN[1]  -> T_BOOL[0]
  OP_MOVE  T_BOOL[0]       -> DOUT[0]

Coalesced (-O2):
  OP_AND   DIN[0], DIN[1]  -> DOUT[0]
```

Direct Destination Coalescing eliminates the redundant `OP_MOVE` instruction and frees the temporary register.

### 6. Dead code elimination (DCE)

Performs reverse reachability analysis rooted at all hardware side-effect nodes:
- Physical outputs (`DIGITALOUTPUT`, `DOUT`)
- State registers (`BOOLREGISTER`, `FLOATREGISTER`, `INTREGISTER`)
- Stateful elements (`TON`, `TOF`, `TP`, `CTU`, `CTD`, `CTUD`, `LATCH`, `SR`, `RS`)
- Custom peripheral drivers (`LE_CUSTOM`) — emitted as an `LE_OP_BLOCK` with a custom function id
- Protection relays and serial bus elements (`PID`, `DIFF_87`, `I2C`, `SPI`)

Any pure logic gate whose output is never consumed and cannot affect circuit state is pruned from the emitted bytecode.

---

## Canonical Circuit JSON specification

The compiler ingests standard circuit JSON containing `name`, `elements`, and `nets`.

```json
{
  "name": "MotorControlCircuit",
  "elements": [
    { "name": "START_PB", "type": "DIGITALINPUT", "address": "%I0" },
    { "name": "STOP_PB",  "type": "DIGITALINPUT", "address": "%I1" },
    { "name": "RUN_LATCH", "type": "LATCH", "dominant": "Reset Dominant" },
    { "name": "MOTOR_CONTACTOR", "type": "DIGITALOUTPUT", "address": "%Q0", "safe_state": 0 }
  ],
  "nets": [
    {
      "output": { "name": "START_PB", "port": "out" },
      "inputs": [ { "name": "RUN_LATCH", "port": "s" } ]
    },
    {
      "output": { "name": "STOP_PB", "port": "out" },
      "inputs": [ { "name": "RUN_LATCH", "port": "r" } ]
    },
    {
      "output": { "name": "RUN_LATCH", "port": "q" },
      "inputs": [ { "name": "MOTOR_CONTACTOR", "port": "in" } ]
    }
  ]
}
```

### Supported element types

| Type Name | Category | Inputs | Outputs | Required properties |
| :--- | :--- | :--- | :--- | :--- |
| `DIGITALINPUT` | I/O | None | `out` | `address` (`%I0..%In`) or mapped alias |
| `DIGITALOUTPUT` | I/O | `in` | None | `address` (`%Q0..%Qn`) or mapped alias, optional `safe_state` |
| `ANALOGINPUT` | I/O | None | `val` | `channel`, `mode` (`raw`/`float`), `scale_min`, `scale_max` |
| `BOOLREGISTER` | Storage | `in` | `out` | `address` (`%M0..%Mn`) |
| `FLOATREGISTER` | Storage | `in` | `out` | `address` (`%R0..%Rn`), optional `units` |
| `INTREGISTER` | Storage | `in` | `out` | `address` (`0..255`), optional `value` |
| `CONSTANT` | Math | None | `out` | `dataType` (`Boolean`/`Float`/`Integer`), `value` |
| `AND`, `OR`, `XOR`, `NAND`, `NOR` | Logic | `a`, `b` | `out` | None |
| `NOT` | Logic | `in` | `out` | None |
| `MUX` | Logic | `sel`, `in0`, `in1` | `out` | None |
| `RTRIG`, `FTRIG` | Edge | `clk` | `q` | None |
| `LATCH`, `SR`, `RS` | Memory | `s`, `r` | `q` | `dominant` (`"Reset Dominant"` or `"Set Dominant"`) |
| `TON`, `TOF`, `TP` | Timer | `in`, `pt` | `q`, `et` | `preset_ms` |
| `CTU`, `CTD`, `CTUD` | Counter | `cu`, `cd`, `r`, `pv` | `qu`, `qd`, `cv` | `preset` |
| `ADD`, `SUB`, `MUL`, `DIV` | Float Math | `a`, `b` | `out` | None |
| `CMP_GT`, `CMP_LT`, `CMP_GE`, `CMP_LE`, `CMP_EQ`, `CMP_NE` | Compare | `a`, `b` | `out` | None |
| `LE_CUSTOM` | Board Ext | Dynamic | Dynamic | `custom_type`, `function_id` |

---

## Variables in element properties

Circuit schematics may declare a top-level `variables` object and reference
those names from any numeric element property using `%VARNAME%` (or a bare name
inside an arithmetic string). This lets a designer retune a whole schematic from
one place instead of editing every element.

### Declaration

```json
{
  "name": "ZoneRelays",
  "variables": {
    "Z_Line": 5.0,
    "Z2": "Z_Line * 1.20",
    "pu": 0.90
  },
  "elements": [
    { "name": "D21", "type": "DIST_21", "reach": "%Z2%", "prefault_v_threshold": "%pu%" }
  ],
  "nets": []
}
```

A variable is either:

- **Direct scalar** — a JSON number (`"Z_Line": 5.0`).
- **Indirect / derived** — a JSON string expression over other variables and/or
  literals (`"Z2": "Z_Line * 1.20"`). Indirect variables may reference each other
  (`"A": "B * 2", "B": "10"`).

### Usage in properties

Any numeric property (timer `preset_ms`, `reach`, `alpha`, `kp`, `o87p`,
`time_dial`, `input_count`, …) accepts:

- a literal number, unchanged;
- `"%VAR%"` — resolves to the variable's numeric value;
- a string expression mixing variables/literals, e.g. `"reach": "Z2 * 0.8"`.

### Supported expression grammar

Arithmetic operators `+ - * /`, parentheses, unary `+`/`-`, and a set of
**trig + math functions** (case-insensitive; unary or two-argument):

| Class | Functions |
| :--- | :--- |
| Trig    | `sin`, `cos`, `tan`, `asin`, `acos`, `atan` (1- or 2-arg `atan2`) |
| Exp/log | `exp`, `ln`/`log`, `log10`, `log2`, `pow(a,b)` |
| Roots   | `sqrt`, `cbrt` |
| Round   | `abs`, `floor`, `ceil`, `round`, `min(a,b)`, `max(a,b)`, `fmod(a,b)` |

Variables may be bare (`Z_Line`) or wrapped in `%…%`. Examples:
`"imp": "(Z_Line + Z2) / 2.0"`, `"mag": "sqrt(Z2 * Z2 + Z_Line * Z_Line)"`,
`"k": "max(0.5, sin(theta) * 2)"`.

### Resolution & errors

Variables are resolved **topologically** (an indirect variable's dependencies are
evaluated first) and memoized, so a variable used in many properties is
evaluated once. A **circular** reference (`A -> B -> A`) or an **undefined**
reference produces a compile error:

```
Variable Error: undefined variable reference: Missing
Variable Error: circular variable reference: B
```

Direct scalar expressions, indirect (variable-on-variable) definitions, cycle
detection, and undefined-variable rejection are all covered by the compiler test
suite (`test_variables`, `test_variable_errors`).

---

## Target Board Profile specification (`.leconfig`)

Target board profiles define physical microcontroller limits and configuration:

```json
{
  "name": "STM32F401RE-Nucleo",
  "platform": "STM32",
  "firmware_version": "2.0.0",
  "limits": {
    "digital_inputs": 16,
    "digital_outputs": 16,
    "analog_inputs": 6,
    "bool_regs": 128,
    "floats": 64,
    "int_regs": 32,
    "workspace_bytes": 2048,
    "slot_size_bytes": 2048,
    "slots": 3
  },
  "features": {
    "protection": true,
    "serial_bus": true
  },
  "pins": [
    { "address": "%I0", "alias": "USER_BUTTON", "debounce_ms": 20, "mode": "Pull-Up" },
    { "address": "%Q0", "alias": "USER_LED", "mode": "Push-Pull" }
  ],
  "custom_nodes": [
    {
      "type_id": "PWM_Generator",
      "display_name": "PWM Generator",
      "function_id": 1,
      "bsp_function": "bsp_pwm_set",
      "bsp_header": "bsp_pwm.h",
      "inputs": [ { "name": "Enable", "type": "Boolean" }, { "name": "DutyCycle", "type": "Float" } ],
      "outputs": [ { "name": "Active", "type": "Boolean" } ]
    }
  ]
}
```

---

## C API reference

Header: [`include/le_compiler.h`](file:///C:/Users/tanne/OneDrive/Documents/GitHub/LogicElements/src/compiler/include/le_compiler.h)

### `le_compile_json_ex`

Compiles a circuit JSON string with explicit optimization options against an optional board profile.

```c
int le_compile_json_ex(
    const char*                  circuit_json,
    const char*                  board_profile_json,
    const le_compiler_options_t* options,
    le_compile_result_t*         out_result
);
```

**Parameters**:
- `circuit_json` (*const char\**): Null-terminated JSON string defining the circuit.
- `board_profile_json` (*const char\**): Optional null-terminated JSON string defining the board profile. Pass `NULL` to compile without board limit constraints.
- `options` (*const le_compiler_options_t\**): Pointer to configuration options. Pass `NULL` to default to `-O2`.
- `out_result` (*le_compile_result_t\**): Destination struct populated with compilation output.

**Returns**:
- `0` on success. Non-zero if compilation encountered an error.

### `le_compile_result_t` structure

```c
typedef struct {
    int         success;                  /* 1 = succeeded, 0 = failed */
    char*       error_message;            /* Error message string (or NULL) */
    char*       warnings;                 /* Warning messages (or NULL) */
    uint8_t*    binary_data;              /* Pointer to raw .lebin binary */
    size_t      binary_size;              /* Byte length of binary_data */
    char*       c_header_code;            /* C static array header source code */
    char*       disassembly_text;         /* Formatted assembly listing */
    char*       json_circuit;             /* Normalized circuit JSON */
    int         instruction_count;        /* Emitted instruction count */
    int         din_count;                /* Digital inputs used */
    int         dout_count;               /* Digital outputs used */
    int         ain_count;                /* Analog inputs used */
    int         bool_reg_count;           /* Total boolean registers allocated */
    int         float_count;              /* Total float registers allocated */
    int         int_count;                /* Total integer registers allocated */
    int         timer_count;              /* Timers used */
    int         counter_count;            /* Counters used */
    uint32_t    crc32;                    /* IEEE 802.3 CRC32 checksum */
    int         user_bool_count;          /* Explicit user %M registers */
    int         temp_bool_count;          /* Peak temporary scratchpad registers */
    int         user_float_count;         /* Explicit user %R registers */
    int         temp_float_count;         /* Peak temporary float registers */
    int         eliminated_instructions;  /* Instructions pruned by optimizer */
    int         eliminated_registers;     /* Registers saved by optimizer */
} le_compile_result_t;
```

### Memory management

All pointers returned in `le_compile_result_t` are dynamically allocated. You **must** release them using `le_compile_result_free`:

```c
le_compile_result_t result;
memset(&result, 0, sizeof(result));

int rc = le_compile_json_ex(circuit_json, board_json, &options, &result);
if (rc == 0 && result.success) {
    // Process result.binary_data, result.c_header_code, etc.
}

le_compile_result_free(&result);
```

---

## Standalone CLI tool (`le_compile`)

The `le_compile` executable provides complete command-line access to the compiler.

### Syntax

```bash
le_compile <circuit.json> [options]
```

### Options

| Option | Description |
| :--- | :--- |
| `-o, --output <file>` | Destination path for the `.lebin` binary payload. |
| `-b, --board <file>` | Target `.leconfig` board profile for limit validation and pin aliases. |
| `-c, --cheader <file>` | Destination path for an embeddable C static header file. |
| `-d, --disasm <file>` | Destination path for a human-readable assembly disassembly listing. |
| `-j, --json <file>` | Export normalized/optimized circuit JSON. |
| `-O0` | Disable all optimizations (1:1 schematic debug mode). |
| `-O1` | Enable basic optimizations (DDC, DCE). |
| `-O2` | Enable full optimizations (Default). |
| `-Os` | Enable aggressive size optimizations. |
| `--no-dce` | Disable Dead Code Elimination pass. |
| `--no-fold` | Disable Constant Folding pass. |
| `--no-cse` | Disable Common Subexpression Elimination pass. |
| `--no-inversion` | Disable Hardware Inversion Folding pass. |
| `--no-direct-dest` | Disable Direct Destination Coalescing pass. |
| `--stats` | Display optimization metrics and memory savings in console. |
| `-v, --version` | Display compiler version information. |
| `-h, --help` | Display command usage and help. |

---

## Binary bytecode specification (`.lebin`)

A compiled LogicElements binary comprises a fixed **header** followed by $N$
contiguous **8-byte instructions**, then the variable-arity block table, the
**state-group (directive) table**, the **preconfigured state image**, and — when
declared — a **register-alias table**. All of it is covered by a trailing IEEE
802.3 CRC32 over the whole payload.

### Binary Header (34 Bytes)

| Offset | Field | Type | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | `magic` | `uint32_t` | Magic identifier: `'LEB1'` (`0x4C454231`, little-endian). |
| `0x04` | `version` | `uint16_t` | Format version (Current: `6`). |
| `0x06` | `flags` | `uint16_t` | Execution flags (`0x0001` = Autostart VM). |
| `0x08` | `instruction_count` | `uint16_t` | Total instructions in payload ($N$). |
| `0x0A` | `digital_in_count` | `uint16_t` | Number of digital inputs allocated (%IN). |
| `0x0C` | `digital_out_count` | `uint16_t` | Number of digital outputs allocated (%OUT). |
| `0x0E` | `bool_reg_count` | `uint16_t` | Total internal boolean registers allocated (%B). |
| `0x10` | `float_reg_count` | `uint16_t` | Total float registers allocated (%F). |
| `0x12` | `complex_reg_count` | `uint16_t` | Total complex registers allocated (%C). |
| `0x14` | `block_count` | `uint16_t` | Number of variable-arity block descriptors. |
| `0x16` | `state_desc_count` | `uint16_t` | Number of state-group (directive) records. |
| `0x18` | `state_img_len` | `uint32_t` | Bytes of the preconfigured state image. |
| `0x1C` | `alias_count` | `uint16_t` | Number of user-declared register aliases in the trailing alias table. |
| `0x1E` | `crc32` | `uint32_t` | IEEE 802.3 CRC32 over the whole payload (instructions + block + state table + state image + alias table). |

> State element instances are **not** enumerated per type in the header. The
> compiler bakes each block's state (defaults + properties) as concrete bytes
> into the state image; the `state-desc` table records `{ kind, count, size }`
> per kind group so the loader can compute each group's byte offset, and
> `state_img_len` is the bound for the platform's state workspace.

### Binary Instruction (8 Bytes)

```
Byte 0:    [ Opcode (uint8_t) ]
Byte 1:    [ Modifier (uint8_t) ] (Bit 0: Invert A, Bit 1: Invert B, or Ext Function ID)
Bytes 2-3: [ Input Address A (uint16_t, Little-Endian) ]
Bytes 4-5: [ Input Address B (uint16_t, Little-Endian) ]
Bytes 6-7: [ Output Address  (uint16_t, Little-Endian) ]
```

### Memory address mapping

The 16-bit address fields encode the memory region in their high bits. Register
families use the canonical user-facing names (`%IN`, `%OUT`, `%AIN`, `%B`, `%I`,
`%F`, `%C`):

| Region | Address Range | Notation | Description |
| :--- | :--- | :--- | :--- |
| Digital Inputs | `0x0000 - 0x0FFF` | `%IN[i]` | Read-only hardware digital inputs (%IN) |
| Digital Outputs | `0x1000 - 0x1FFF` | `%OUT[i]` | Read/write physical digital outputs (%OUT) |
| Boolean Registers | `0x2000 - 0x3FFF` | `%B[i]` / `T_B[i]` | Bit-packed internal coils & scratchpads (%B) |
| Float Registers | `0x4000 - 0x7FFF` | `%F[i]` / `T_F[i]` | 32-bit floating-point registers (%F) |
| Timers | `0x8000 - 0x8FFF` | `TIMER[i]` | Hardware timer state blocks |
| Counters | `0x9000 - 0x9FFF` | `COUNTER[i]` | Hardware counter state blocks |
| Integer Registers | `0xA000 - 0xAFFF` | `%I[i]` / `T_I[i]` | 32-bit integer registers (%I) |
| Analog Inputs | `0xB000 - 0xBFFF` | `%AIN[i]` | Hardware ADC channels (%AIN) |
| Constants / Special | `0xF000 - 0xFFFF` | `FALSE`, `TRUE`, `0.0f` | Immediate constants & unused ports |

> The legacy `%I` (digital input) / `%Q` / `%M` / `%R` spellings were renamed to
> `%IN` / `%OUT` / `%B` / `%F` respectively in binary version `6`. Existing
> element JSON using the old spellings is still accepted by the compiler, and
> `DIN[i]` / `DOUT[i]` / `BOOL[i]` / `FLOAT[i]` / `INT[i]` / `AIN[i]` remain
> valid synonyms in alias targets.

---

---

## Register aliases

Circuit designers can give a process-image register a short symbolic name so a
host/runtime can **override, pulse, or target** it without knowing its raw 16-bit
address or register family. Aliases are **dynamic per program**: only the names a
circuit explicitly declares are embedded in the `.lebin`; nothing is generated
automatically.

### Declaring aliases in a circuit

Add a top-level `aliases` object mapping a name (1..7 characters) to a register
target written in the canonical notation (`%B`, `%F`, `%I`, `%C`, `%IN`, `%OUT`,
`%AIN`):

```json
{
  "name": "ZoneAlarms",
  "elements": [ ],
  "nets": [ ],
  "aliases": {
    "START": "%B0",
    "SP":    "%F2",
    "TRIP":  "%OUT1"
  }
}
```

A target may be a register mnemonic, the name of a `BOOLREGISTER` /
`FLOATREGISTER` / `INTREGISTER` / `COMPLEXREGISTER` element (resolved to that
element's output address), or a board pin alias supplied by the board profile.

### Binary storage

Declared aliases are appended as a compact **alias table** after the state
image. Each 10-byte `le_alias_t` entry stores the 7-character name, a reserved
`kind`/`pad` byte, and the 16-bit process-image address the name resolves to.
`header.alias_count` reports the number of entries; `alias_count == 0` when the
circuit declares none, so existing binaries without aliases remain valid. The
CRC32 covers the alias table like every other section.

### Board aliases do not grow the binary

A board profile ships its own `pin_map` aliases as compile-time physical pin
names. These are **not** baked into the program. The host/firmware supplies them
at runtime via `le_vm_load_board_aliases`, so the runtime can resolve them by
name without carrying them in every program's `.lebin`. The alias lookup checks
program aliases first, then board aliases.

### Runtime API

| Function | Purpose |
| :--- | :--- |
| `le_alias_lookup(vm, name, &addr)` | Resolve a name to a 16-bit address. |
| `le_alias_set_bool/float/int(vm, name, val)` | Override a register by alias. |
| `le_alias_toggle(vm, name)` | Toggle a boolean register by alias. |
| `le_alias_pulse(vm, name)` | Pulse for the default **1 second**. |
| `le_alias_pulse_for(vm, name, seconds)` | Pulse for an explicit duration. |
| `le_vm_pulse(vm, addr, duration_ms)` | Pulse a raw address for a duration. |

A pulse sets the register to its active (non-zero) state and clears it back to
zero once the VM clock advances past the duration.

### Terminal command

The interactive terminal exposes:

```
pulse <name> [seconds]
```

`pulse <name>` holds the register for 1 second; `pulse <name> 2.5` holds it for
2.5 seconds. The name may be an alias or a raw register reference.

---

## Digital signal processing (DSP) and real-time filtering

LogicElements provides a suite of real-time digital signal processing blocks designed for sensor conditioning, vibration analysis, motion control rate limiting, and AC power measurement.

### Zero-heap static memory architecture

To guarantee deterministic scan cycles and eliminate fragmentation risks on bare-metal microcontrollers, all DSP filter blocks adhere to a **zero-heap memory guarantee** (`malloc = 0`):
- All filter internal states (delay lines, accumulators, sample buffers) are baked into the **preconfigured state image** and copied verbatim into the platform's state workspace at load — never allocated at runtime.
- There are **no per-type instance caps**. Capacity is `state_img_len ≤ LE_STATE_WORKSPACE_BYTES` (the board's RAM workspace); the compiler validates the circuit's total state fits before it is ever loaded.
- DSP state blocks persist across scan cycles and are re-initialized cleanly when switching configuration slots or issuing a system reset.
- State blocks are enabled by default via `LE_ENABLE_DSP 1`.

### DSP opcode specification

The runtime implements opcodes `0x90` through `0x97`:

| Opcode | Identifier | Modifier byte | Inputs | Output | Description & recurrence relation |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `0x90` | `LE_OP_LPF_1P` | Block index `0..15` | `in_a`: Signal $x[n]$<br>`in_b`: Alpha $\alpha$ | `out`: Filtered $y[n]$ | **First-Order Lowpass Filter**: Single-pole IIR filter for noise smoothing.<br>$y[n] = y[n-1] + \alpha \cdot (x[n] - y[n-1])$ |
| `0x91` | `LE_OP_BIQUAD_IIR` | Block index `0..15` | `in_a`: Signal $x[n]$<br>`in_b`: Unused | `out`: Filtered $y[n]$ | **Biquad IIR Filter**: Direct Form II Transposed structure supporting lowpass, highpass, bandpass, and notch responses.<br>$y[n] = b_0 x[n] + d_1[n-1]$<br>$d_1[n] = b_1 x[n] - a_1 y[n] + d_2[n-1]$<br>$d_2[n] = b_2 x[n] - a_2 y[n]$ |
| `0x92` | `LE_OP_MOVING_AVG` | Block index `0..15` | `in_a`: Signal $x[n]$<br>`in_b`: Unused | `out`: Average $\bar{x}[n]$ | **Moving Average**: Circular ring buffer boxcar filter ($N \le 32$) with running $O(1)$ accumulator.<br>$y[n] = \frac{1}{N}\sum_{k=0}^{N-1} x[n-k]$ |
| `0x93` | `LE_OP_RATE_LIMITER` | Block index `0..15` | `in_a`: Signal $x[n]$<br>`in_b`: Unused | `out`: Slew $y[n]$ | **Rate / Slew Limiter**: Clamps the maximum rate of change for rising and falling signal slopes.<br>$\Delta = x[n] - y[n-1]$<br>$y[n] = y[n-1] + \text{clamp}(\Delta, -R_{\text{fall}} \Delta t, R_{\text{rise}} \Delta t)$ |
| `0x94` | `LE_OP_DEADBAND` | Block index `0..15` | `in_a`: Signal $x[n]$<br>`in_b`: Unused | `out`: Filtered $y[n]$ | **Deadband / Hysteresis Gate**: Suppresses small fluctuations around an operating center point.<br>$y[n] = 0 \text{ if } |x[n] - c| \le \text{threshold}$, else $x[n]$ |
| `0x95` | `LE_OP_WASHOUT` | Block index `0..15` | `in_a`: Signal $x[n]$<br>`in_b`: Unused | `out`: AC $y[n]$ | **Washout / Highpass DC Blocker**: Attenuates DC bias and steady-state sensor drift while passing transient high-frequency perturbations.<br>$y[n] = \alpha \cdot y[n-1] + \alpha \cdot (x[n] - x[n-1])$ |
| `0x96` | `LE_OP_PEAK_DETECTOR` | Block index `0..15` | `in_a`: Signal $x[n]$<br>`in_b`: Reset flag | `out`: Peak $\hat{x}[n]$ | **Peak / Envelope Follower**: Fast instantaneous attack tracking positive signal peaks with exponential decay toward zero. |
| `0x97` | `LE_OP_RMS` | Block index `0..15` | `in_a`: Signal $x[n]$<br>`in_b`: Unused | `out`: RMS $x_{\text{rms}}$ | **True RMS Energy Detector**: True Root-Mean-Square calculation over a windowed ring buffer ($N \le 32$).<br>$x_{\text{rms}} = \sqrt{\frac{1}{N}\sum_{k=0}^{N-1} x[n-k]^2}$ |

### Numerical stability safeguards

The runtime incorporates defensive programming safeguards on every DSP evaluation:
- **Denormal Flushing**: Small numbers below $10^{-15}$ are automatically flushed to zero to prevent CPU pipeline stalls on architectures without hardware denormal support.
- **NaN / Infinity Guards**: If an input or accumulator evaluates to `NaN` or `Inf`, the runtime safely resets the filter state and returns `0.0f` without crashing the scan loop.
- **Div-by-Zero Protection**: In the RMS and Moving Average blocks, window sizes are clamped to the valid range ($1 \le N \le 32$) to prevent division by zero.
- **First-Sample Bumpless Initialization**: Filters automatically initialize their internal states to the first incoming sample value ($y[0] = x[0]$), preventing startup step transients.

### Circuit JSON schema for DSP elements

When constructing circuits programmatically or authoring custom tools, declare DSP elements in the `elements` array using the following schemas:

```json
{
  "name": "AnalogFilterPipeline",
  "elements": [
    {
      "name": "LPF1",
      "type": "LPF",
      "alpha": 0.2
    },
    {
      "name": "BIQUAD1",
      "type": "BIQUAD",
      "filter_type": "Lowpass",
      "b0": 0.067455,
      "b1": 0.134911,
      "b2": 0.067455,
      "a1": -1.142981,
      "a2": 0.412802
    },
    {
      "name": "MAVG1",
      "type": "MOVING_AVG",
      "window_size": 16
    },
    {
      "name": "SLEW1",
      "type": "RATE_LIMITER",
      "rising_rate": 50.0,
      "falling_rate": 25.0
    },
    {
      "name": "DEAD1",
      "type": "DEADBAND",
      "threshold": 0.5,
      "center": 0.0
    },
    {
      "name": "WASH1",
      "type": "WASHOUT",
      "alpha": 0.95
    },
    {
      "name": "PEAK1",
      "type": "PEAK_DETECTOR",
      "decay_rate": 0.995
    },
    {
      "name": "RMS1",
      "type": "RMS",
      "window_size": 16
    }
  ],
  "nets": [
    {
      "output": { "name": "AIN0", "port": "out" },
      "inputs": [ { "name": "LPF1", "port": "in" } ]
    },
    {
      "output": { "name": "LPF1", "port": "out" },
      "inputs": [ { "name": "SLEW1", "port": "in" } ]
    }
  ]
}
```

### Board capability validation

The compiler validates circuit DSP usage against target `.leconfig` board profiles:
- **`features.dsp`**: Boolean flag. If `false`, any circuit containing DSP elements will fail compilation with:
  ```
  Board Validation Error: Circuit uses DSP opcode 0x90, but target board 'TargetBoard' has DSP feature disabled.
  ```
- **`limits.dspFilters`**: Maximum allowed count of DSP blocks on the target board (e.g., `16` on STM32 Nucleo, `4` on AVR). If a circuit defines more DSP elements than the board limit, the compiler rejects the circuit:
  ```
  Board Limit Exceeded: Circuit defines 5 DSP filter elements, but target board allows a maximum of 4.
  ```

---

## Related guides

- [Platform Integration Guide](../PLATFORM_GUIDE.md): Microcontroller porting, flash storage partitions, and UART upload protocols.
- [Custom Nodes Guide](CUSTOM_NODES_GUIDE.md): Designing, declaring, and implementing custom hardware nodes (`LE_OP_BLOCK` with a custom function id).
