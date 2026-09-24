# LogicElements Custom Nodes Guide

This guide describes how to design, configure, implement, and compile custom board nodes and hardware extension functions for the LogicElements engine.

---

## Overview

LogicElements provides a standard library of deterministic PLC elements, including combinational gates, latches, timers, counters, floating-point arithmetic, PID controllers, and protection relays. However, embedded microcontrollers often incorporate specialized hardware peripherals, such as:

- Hardware pulse-width modulation (PWM) timer channels
- Quadrature encoder interface (QEI) counters
- Analog-to-digital converter (ADC) digital signal processing (DSP) filters
- Fieldbus transceivers (CAN, Modbus, LIN)
- Cryptographic acceleration engines
- Stepper and brushless DC motor driver stages
- Proprietary coprocessors

Custom nodes allow adopters to expose these hardware capabilities directly to schematic designers as first-class graphical blocks, while preserving the real-time determinism, memory isolation, and safety interlocks of the LogicElements virtual machine.

---

## Architectural data flow

The lifecycle of a custom node spans hardware profile definition, interactive discovery, schematic compilation, and runtime dispatch:

```
┌────────────────────────────────────────────────────────┐
│               1. Board Profile (.leconfig)             │
│  Declares custom node types, function IDs, categories, │
│  and pin signatures for the target hardware.           │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│             2. Discovery & Tooling Support             │
│  - le_cli: "nodes" command prints JSON over UART.      │
│  - le_comms: Binary packet 0x03 interrogates firmware. │
│  - Desktop UI / IDE loads nodes into component palette.│
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│             3. Schematic Circuit Authoring             │
│  Designer places custom node and wires input/output    │
│  pins to digital signals, registers, or sensors.       │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│             4. Compiler Translation (le_compile)       │
│  - Validates node pins against board profile.          │
│  - Preserves hardware side effects during optimization.│
│  - Emits a variable-arity LE_OP_BLOCK (0xA0) call (func id ≥ 0x80).     │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│             5. Microcontroller Virtual Machine         │
│  - VM encounters LE_OP_BLOCK (custom func id) during scan loop.      │
│  - Dispatches to adopter HAL callback: ext_call().     │
│  - Executes native C peripheral code in real time.     │
└────────────────────────────────────────────────────────┘
```

---

## Binary instruction format (`LE_OP_BLOCK`)

Custom nodes are emitted as **variable-arity `LE_OP_BLOCK` calls** (`0xA0`) so a
node can have any number of inputs and outputs. The instruction carries the
function id and a block-table index; the node's operand list lives in a
`le_block_desc_t` (in the block table at the end of the payload):

```
Byte:   0           1           2       3       4       5       6       7
      [  0xA0    ][  func_id  ][  block-desc index ][     unused      ]
      (opcode)  ( >= 0x80 )   (in_a: index into    (in_b & out unused;
      0xA0 =      custom-      the block-table      operands live in the
      LE_OP_      node id)      descriptor)         descriptor args)
      BLOCK
```

### Field breakdown

| Field | Size | Description |
| :--- | :--- | :--- |
| `opcode` | 1 byte | `0xA0` (`LE_OP_BLOCK`). |
| `modifier` / `func_id` | 1 byte | Function identifier. Custom-node ids are `>= LE_FUNC_CUSTOM_BASE (0x80)`; the runtime dispatches those to the HAL `ext_call`. |
| `in_a` | 2 bytes | Index into the **block-table** (`le_block_desc_t`). The descriptor lists `in_count` inputs then `out_count` outputs as 16-bit process-image addresses. |
| `in_b` | 2 bytes | Unused for block calls (`LE_ADDR_UNUSED`). |
| `out` | 2 bytes | Unused for block calls (`LE_ADDR_UNUSED`); outputs are written to the descriptor's output addresses. |

The disassembler renders a custom node's call with its function id and arity,
for example `[0000] BLOCK BLK[0] fn:0x81 -> 0->1 [ | T_BOOL[0]]`.

## Board profile specification (`.leconfig`)

To make custom nodes available to desktop design tools and the compiler, define them in your board's `.leconfig` profile under the `"custom_nodes"` array.

### Schema and syntax

```json
{
  "device": {
    "name": "STM32F4-PLC-Pro",
    "firmware_version": "1.0",
    "protocol_version": 1
  },
  "custom_nodes": [
    {
      "type_id": "BOARD_PWM",
      "display_name": "PWM Timer Output",
      "category": "Hardware",
      "description": "Configures hardware timer TIM1 Channel 1 PWM duty cycle (0.0 to 100.0 percent).",
      "function_id": 1,
      "c_header": "board_pwm.h",
      "inputs": [
        { "name": "DUTY", "type": "float" },
        { "name": "ENABLE", "type": "bool" }
      ],
      "outputs": [
        { "name": "ACTIVE", "type": "bool" }
      ]
    },
    {
      "type_id": "BOARD_ENCODER",
      "display_name": "Quadrature Encoder Reader",
      "category": "Motion",
      "description": "Reads hardware quadrature encoder counter TIM3 with reset strobe.",
      "function_id": 2,
      "c_header": "board_encoder.h",
      "inputs": [
        { "name": "RESET", "type": "bool" }
      ],
      "outputs": [
        { "name": "POSITION", "type": "float" }
      ]
    }
  ]
}
```

### Attribute definitions

- `type_id`: Unique uppercase string used in circuit JSON schematics (for example, `"BOARD_PWM"`). The compiler also recognizes the keys `"type"` and `"name"` as fallbacks.
- `display_name`: Human-readable label displayed in graphical UI component palettes.
- `category`: Category group in the UI palette (such as `"Hardware"`, `"Motion"`, `"Sensors"`, or `"DSP"`).
- `description`: Text describing node behavior and electrical characteristics.
- `function_id`: Numeric identifier (`1` to `255`) passed directly to `ext_call()` in firmware.
- `c_header`: Optional C header file (for example, `"board_pwm.h"`). When generating standalone C code via `le_compile -c output.h`, the compiler automatically inserts an `#include` directive for this header.
- `inputs`: Array of input pin descriptors containing `"name"` and `"type"` (`"bool"`, `"float"`, or `"int"`). Up to two inputs are mapped directly to `in_a` and `in_b`.
- `outputs`: Array of output pin descriptors containing `"name"` and `"type"`. The primary output maps directly to `out`.

---

## Firmware implementation in C

Integrating custom nodes into microcontroller firmware requires two callbacks in the hardware abstraction layer (`le_hal_t`).

### 1. HAL callback declarations (`le_hal.h`)

```c
typedef struct {
    /* ... standard HAL methods (time, GPIO, UART, storage, bus) ... */

    /* Board custom node and external hardware callbacks */
    le_status_t (*ext_call)(uint8_t func_id, const uint16_t* args, int in_count, int out_count, le_process_image_t* img);
    const char* (*get_custom_nodes_json)(void);
} le_hal_t;
```

> **v2 variable-arity contract** - `ext_call` no longer receives a fixed
> `(in_a, in_b, out)` triple. Instead `args` holds the live operand addresses
> as **inputs first, then outputs**: `args[0..in_count)` are inputs and
> `args[in_count..in_count+out_count)` are outputs. Custom nodes can therefore
> have any number of terminals. Operand data types are implied by each
> address's process-image region (bool / float / AIN / constant).
> `LE_OP_BLOCK` dispatches custom nodes this way; the recipes below read/write
> the first operand(s) and write the output at `args[in_count]`.
### 2. Process image memory access helpers

Use the following helper functions declared in `src/runtime/include/le_process_image.h` to read inputs and write outputs safely:

| Data type | Read helper | Write helper |
| :--- | :--- | :--- |
| **Boolean / Bit** | `bool le_process_image_get_bool(const le_process_image_t* img, uint16_t addr)` | `void le_process_image_set_bool(le_process_image_t* img, uint16_t addr, bool val)` |
| **Float (32-bit)** | `float le_process_image_get_float(const le_process_image_t* img, uint16_t addr)` | `void le_process_image_set_float(le_process_image_t* img, uint16_t addr, float val)` |
| **Digital Input (%I)** | `le_process_image_get_bool(img, LE_ADDR_MAKE_DIN(i))` | N/A (read from physical GPIO) |
| **Digital Output (%Q)**| `le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(i))`| `le_process_image_set_bool(img, LE_ADDR_MAKE_DOUT(i), val)` |

---

## Concrete implementation recipes

### Recipe 1: Hardware PWM timer output controller

This recipe implements function ID `1` (`BOARD_PWM`). It reads a float duty cycle from `in_a`, an enable flag from `in_b`, updates the hardware timer compare register, and sets an active indicator flag at `out`:

```c
#include "stm32f4xx_hal.h"
#include "le_hal.h"
#include "le_process_image.h"

extern TIM_HandleTypeDef htim1;

static le_status_t hal_ext_call(uint8_t func_id, const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    if (!img) {
        return LE_ERROR;
    }

    switch (func_id) {
    case 1: { /* BOARD_PWM */
        /* in_a: float duty cycle address (%R) */
        /* in_b: boolean enable address (%I or %M) */
        float duty = le_process_image_get_float(img, in_a);
        bool enable = le_process_image_get_bool(img, in_b);

        if (enable) {
            /* Clamp duty cycle between 0.0% and 100.0% */
            if (duty < 0.0f) duty = 0.0f;
            if (duty > 100.0f) duty = 100.0f;

            /* Compute timer compare register value */
            uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim1);
            uint32_t compare = (uint32_t)((duty / 100.0f) * (float)period);
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare);

            /* Set active output flag */
            if (out != LE_ADDR_UNUSED) {
                le_process_image_set_bool(img, out, true);
            }
        } else {
            /* Disable PWM output */
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            if (out != LE_ADDR_UNUSED) {
                le_process_image_set_bool(img, out, false);
            }
        }
        return LE_OK;
    }

    default:
        return LE_ERROR;
    }
}
```

---

### Recipe 2: Hardware quadrature encoder reader

This recipe implements function ID `2` (`BOARD_ENCODER`). It checks `in_a` for a reset pulse, reads the 32-bit hardware encoder counter, and writes the position as a float to `out`:

```c
extern TIM_HandleTypeDef htim3;

static le_status_t hal_ext_call(uint8_t func_id, const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    (void)in_b;
    if (!img) return LE_ERROR;

    switch (func_id) {
    case 2: { /* BOARD_ENCODER */
        /* Check reset input */
        if (in_a != LE_ADDR_UNUSED && le_process_image_get_bool(img, in_a)) {
            __HAL_TIM_SET_COUNTER(&htim3, 0);
        }

        /* Read signed 32-bit counter value */
        int32_t count = (int32_t)__HAL_TIM_GET_COUNTER(&htim3);

        /* Write position to float register destination */
        if (out != LE_ADDR_UNUSED) {
            le_process_image_set_float(img, out, (float)count);
        }
        return LE_OK;
    }

    default:
        return LE_ERROR;
    }
}
```

---

### Recipe 3: Fast exponential smoothing / DSP filter

This recipe implements function ID `3` (`BOARD_FILTER`). It applies single-pole exponential smoothing:
$$y[n] = y[n-1] + \alpha \cdot (x[n] - y[n-1])$$

```c
static float s_filter_state = 0.0f;
static const float s_alpha = 0.15f;

static le_status_t hal_ext_call(uint8_t func_id, const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    (void)in_b;
    if (!img) return LE_ERROR;

    switch (func_id) {
    case 3: { /* BOARD_FILTER */
        float raw_val = le_process_image_get_float(img, in_a);
        s_filter_state += s_alpha * (raw_val - s_filter_state);

        if (out != LE_ADDR_UNUSED) {
            le_process_image_set_float(img, out, s_filter_state);
        }
        return LE_OK;
    }

    default:
        return LE_ERROR;
    }
}
```

---

### Defining the JSON metadata callback

Provide a static JSON array describing all supported custom nodes. Return this string in `get_custom_nodes_json`:

```c
static const char s_custom_nodes_json[] =
"["
"  {"
"    \"type\": \"BOARD_PWM\","
"    \"func_id\": 1,"
"    \"category\": \"Hardware\","
"    \"description\": \"Hardware PWM duty cycle generator (0..100%)\","
"    \"inputs\": [\"DUTY\", \"ENABLE\"],"
"    \"outputs\": [\"ACTIVE\"]"
"  },"
"  {"
"    \"type\": \"BOARD_ENCODER\","
"    \"func_id\": 2,"
"    \"category\": \"Motion\","
"    \"description\": \"Quadrature encoder counter\","
"    \"inputs\": [\"RESET\"],"
"    \"outputs\": [\"POSITION\"]"
"  }"
"]";

static const char* hal_get_custom_nodes_json(void)
{
    return s_custom_nodes_json;
}
```

Assign both functions when initializing your HAL struct:

```c
static const le_hal_t s_my_board_hal = {
    /* ... other HAL functions ... */
    .ext_call              = hal_ext_call,
    .get_custom_nodes_json = hal_get_custom_nodes_json,
};
```

---

## Discovery and interrogation

LogicElements provides three mechanisms to query custom node capabilities from connected hardware without requiring hardcoded desktop software changes.

### 1. Interactive terminal CLI (`nodes`)

Technicians can open any standard serial terminal (115200 8N1) and execute the `nodes` command:

```text
LE> nodes
[
  {
    "type": "BOARD_PWM",
    "func_id": 1,
    "category": "Hardware",
    "description": "Hardware PWM duty cycle generator (0..100%)",
    "inputs": ["DUTY", "ENABLE"],
    "outputs": ["ACTIVE"]
  }
]
```

### 2. Binary framed packet protocol (`0x03` / `0x83`)

Desktop engineering tools, automated test fixtures, and web configurators query custom nodes using framed binary packets:

```
HOST                                      TARGET MCU
  │                                           │
  │── LE_CMD_GET_CUSTOM_NODES (0x03) ────────►│
  │◄── LE_CMD_CUSTOM_NODES_DATA (0x83, JSON) ─│
```

- **Request Command**: `0x03` (`LE_CMD_GET_CUSTOM_NODES`), length 0.
- **Response Command**: `0x83` (`LE_CMD_CUSTOM_NODES_DATA`), payload contains the null-terminated JSON metadata string.

### 3. Python discovery CLI

Use the included `le_board.py` utility to query a target device and generate a `.leconfig` file automatically:

```bash
py tools/compiler/le_board.py --port COM3 --query-nodes --output my_board.leconfig
```

---

## Circuit schematic authoring

In circuit JSON files, custom nodes are declared with their corresponding `type` and wired through named nets:

```json
{
  "name": "MotorSpeedControl",
  "elements": [
    {
      "name": "SPEED_POT",
      "type": "FLOAT_REGISTER",
      "address": "%R0"
    },
    {
      "name": "START_SWITCH",
      "type": "DIGITALINPUT",
      "address": "%I0"
    },
    {
      "name": "PWM_DRIVER",
      "type": "BOARD_PWM"
    },
    {
      "name": "RUN_INDICATOR",
      "type": "DIGITALOUTPUT",
      "address": "%Q0"
    }
  ],
  "nets": [
    {
      "output": { "name": "SPEED_POT", "port": "out" },
      "inputs": [ { "name": "PWM_DRIVER", "port": "duty" } ]
    },
    {
      "output": { "name": "START_SWITCH", "port": "out" },
      "inputs": [ { "name": "PWM_DRIVER", "port": "enable" } ]
    },
    {
      "output": { "name": "PWM_DRIVER", "port": "active" },
      "inputs": [ { "name": "RUN_INDICATOR", "port": "in" } ]
    }
  ]
}
```

---

## Compilation and optimization behavior

Invoke the optimizing compiler by specifying the target board profile:

```bash
# Using the C++ optimizing compiler:
le_compile circuit.json -b my_board.leconfig -o circuit.lebin --stats

# Or using the Python compiler CLI:
py tools/compiler/le_compiler.py circuit.json -b my_board.leconfig -o circuit.lebin
```

### Compiler resolution process

1. **Board matching**: The compiler matches the element `type: "BOARD_PWM"` against the entries in the target `my_board.leconfig`.
2. **Function ID assignment**: The compiler extracts `func_id` (`1`) and writes it to the instruction's `modifier` byte.
3. **Port mapping**: The compiler maps schematic pin names (`duty`, `enable`, `active`) to the input and output operands `in_a`, `in_b`, and `out`.
4. **Side effect preservation**: Custom nodes are registered in the intermediate representation as having hardware side effects. This prevents dead code elimination from pruning them, even if their outputs do not drive downstream logic gates.
5. **Instruction generation**: The compiler emits an `LE_OP_BLOCK` (`0xA0`) call with a custom function id (>= `0x80`) and a block descriptor carrying the node's operands.

---

## Disassembly verification

To inspect the generated bytecode, disassemble the binary using `le_compile -d` or `le_disasm`:

```bash
le_compile circuit.json -b my_board.leconfig -o circuit.lebin -d disasm.txt
```

Example disassembly listing:

```text
============================================================
 LOGICELEMENTS BINARY DISASSEMBLY (.lebin)
============================================================
Magic:               0x4C454231 ('LEB1')
Version:             5
Flags:               0x0001 (Autostart: True)
Instruction Count:   1
------------------------------------------------------------
INDEX   OPCODE         IN_A         IN_B         OUT         
------------------------------------------------------------
[0000]  BLOCK          BLK[0]       fn:0x81      -> 2->1 [R[0] DIN[0] | DOUT[0]]     
============================================================
```

---

## Troubleshooting and best practices

### Execution determinism
`ext_call` runs synchronously inside the real-time PLC scan loop. Code inside `ext_call` must:
- Execute deterministically and complete within microseconds.
- Never use blocking delays (`HAL_Delay()`, busy-wait loops, or unbounded polling).
- Use DMA or interrupt-driven flags for external peripheral communication.

### Memory safety and isolation
- Never perform dynamic heap allocation (`malloc`, `free`, or `new`) within `ext_call`.
- Always validate `img != NULL` before accessing memory.
- Use the accessor functions (`le_process_image_get_float`, `le_process_image_set_bool`) rather than directly indexing internal struct pointers.

### Handling unused operands
If a custom node only requires one input or does not generate an output, the unused operands are set to `LE_ADDR_UNUSED` (`0xFFFF`). Always check that an operand address is valid before attempting to write output results:

```c
if (out != LE_ADDR_UNUSED) {
    le_process_image_set_float(img, out, calculated_value);
}
```

### Unrecognized function IDs
Always include a `default` branch in your `ext_call` switch statement returning `LE_ERROR`. If a binary compiled for a different board attempts to invoke an unsupported function ID, the virtual machine will immediately flag an execution error and halt or de-energize outputs safely.


