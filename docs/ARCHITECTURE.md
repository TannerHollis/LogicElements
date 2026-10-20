# Architecture

This document is the technical overview of LogicElements: the runtime engine,
the compiler pipeline, the `.lebin` binary format, the variable-arity block
mechanism, the zero-heap state workspace, the hardware abstraction layer, tooling,
and testing. It is written in the Google developer-documentation style: an
overview first, then reference detail.

---

## Table of contents

- [Overview](#overview)
- [Runtime engine](#runtime-engine)
  - [Scan loop (`le_vm.c`)](#scan-loop-le_vmc)
  - [Instruction format (`le_types.h`)](#instruction-format-le_typesh)
  - [Process image (`le_process_image.c/.h`)](#process-image-le_process_imagech)
- [Runtime VM size and feature breakdown](#runtime-vm-size-and-feature-breakdown)
  - [Subsystem feature sizing and modular cost](#subsystem-feature-sizing-and-modular-cost)
  - [Deployment profile tiers](#deployment-profile-tiers)
- [Variable-arity block mechanism](#variable-arity-block-mechanism)
  - [State block X-Macro architecture (`le_blocks.def`)](#state-block-x-macro-architecture-le_blocksdef)
- [Zero-heap unified RAM workspace (`src/runtime/src/le_rt.c`, `le_process_image.c`)](#zero-heap-unified-ram-workspace-srcruntimesrcle_rtc-le_process_imagec)
- [Loader, storage & NVRAM (`le_loader.c`, `le_storage.c`, `le_nvram.c`)](#loader-storage--nvram-le_loaderc-le_storagec-le_nvramc)
- [Compiler (`src/compiler`)](#compiler-srccompiler)
  - [Pipeline (`le_compiler_core.cpp`, `le_code_block.cpp`, `le_optimizer.cpp`)](#pipeline-le_compiler_corecpp-le_code_blockcpp-le_optimizercpp)
  - [Public API (`src/compiler/include/le_compiler.h`)](#public-api-srccompilerincludele_compilerh)
- [Hardware abstraction layer (`src/runtime/include/le_hal.h`)](#hardware-abstraction-layer-srcruntimeincludele_halh)
- [Tooling](#tooling)
- [Testing](#testing)

---

## Overview

LogicElements is a two-sided system.

**Host / desktop side**
- A C++17 **compiler library** (`src/compiler`) that turns a circuit schematic
  (JSON) into `.lebin` bytecode, with a multi-pass optimizer, a liveness /
  register allocator, a disassembler, and a C ABI.
- **Tooling** (`tools/`) — Python ctypes SDK, board profiler, disassembler.

**Microcontroller side**
- A **zero-heap ANSI C runtime** (`src/runtime`) that loads the `.lebin` and
  sweeps a scan loop over the process image, driving hardware through a HAL.
  Scalable from **~6.5 KB Flash** (minimal micro-PLC) to **~42 KB Flash** (full substation
  protection relay with Ed25519 signing and DSP; see [Runtime VM Size and Feature Breakdown](#runtime-vm-size-and-feature-breakdown)).
- Stateful elements (timers, counters, PID, DSP filters, protection) get their
  state **baked into a preconfigured state image** that the loader copies
  into a static zero-heap workspace at load time — never `malloc` after boot.

```
Circuit JSON ── compiler ──► .lebin ── UART/upload ──► loader ──► VM ──► process image ──► HAL
                                                                 └─► zero-heap workspace (state image)
```

---

## Runtime engine

### Scan loop (`le_vm.c`)

`le_vm_step()` iterates the active instruction array (execute-in-place, no
copy) and calls `le_exec_instruction_ex()` for each. A `le_vm_t` holds:

| Field | Purpose |
| :--- | :--- |
| `image` | The statically allocated process image (registers + region bits). |
| `instructions` / `instruction_count` | Active `.lebin` program (flash/RAM). |
| `blocks` / `block_count` | Variable-arity block descriptors (flash/RAM). |
| `running` / `cycle_count` | Execution state and scan counter. |

### Instruction format (`le_types.h`)

Every instruction is **8 bytes**:

```c
typedef struct {
    uint8_t  opcode;    // e.g. LE_OP_ADD_F, LE_OP_TON, LE_OP_BLOCK
    uint8_t  modifier;  // flags / block instance index / function id
    uint16_t in_a;      // operand A address
    uint16_t in_b;      // operand B address (or block table index)
    uint16_t out;       // destination address
} le_instruction_t;
```

Most elements use exactly two data operands. Elements that need **more than
two** (MUX, phasor conversions, custom nodes) use `LE_OP_BLOCK` and a
**variable-arity block descriptor**.

### Process image (`le_process_image.c/.h`)

A fixed memory table of registers and per-region status bits:

| Region | Address Range | Contents |
| :--- | :--- | :--- |
| `0x0000` | `0x0000 - 0x0FFF` | Digital inputs `%IN` (bit-packed, up to 4,096) |
| `0x1000` | `0x1000 - 0x1FFF` | Digital outputs `%OUT` (bit-packed, up to 4,096) |
| `0x2000` | `0x2000 - 0x2FFF` | Analog inputs `%AIN` (32-bit floats, up to 4,096) |
| `0x3000` | `0x3000 - 0x3FFF` | Analog outputs `%AOUT` (32-bit floats, up to 4,096) |
| `0x4000`–`0x5000` | `0x4000 - 0x5FFF` | Boolean registers `%B` / `T_B` (bit-packed, up to 8,192) |
| `0x6000`–`0x7000` | `0x6000 - 0x7FFF` | Integer registers `%I` / `T_I` (32-bit signed, up to 8,192) |
| `0x8000`–`0x9000` | `0x8000 - 0x9FFF` | Float registers `%F` / `T_F` (32-bit IEEE 754, up to 8,192) |
| `0xA000` | `0xA000 - 0xAFFF` | Complex registers `%C` / `T_C` (real+imag pairs, up to 4,096) |
| `0xC000` | `0xC000 - 0xCFFF` | Predefined immediate literals (`FALSE`, `TRUE`, `0.0f`, `1.0f`, `60.0f`, `0+0j`) |
| `0xFFFF` | `0xFFFF` | Sentinel / Unused (`LE_ADDR_UNUSED`) |

> [!NOTE]
> **Timers and Counters**: Starting in binary version 12, timers and counters do not have dedicated virtual address regions. Their internal states reside in the unified state image (allocated in the RAM workspace), indexed by the instruction's `modifier` field. Their boolean output flags write directly to normal boolean destinations (`%B` or `%OUT`).

Accessors (`le_process_image_get_bool/float`, `set_bool/float`, and the
kind resolvers) are the single funnel through which handlers and
consumers read/write registers and workspace-backed state.

---

## Runtime VM size and feature breakdown

The LogicElements ANSI C99 runtime is designed for granular, modular scalability. Every subsystem can be independently enabled or stripped at compile time using preprocessor defines in `src/runtime/include/le_config.h` (or through `-D` flags in CMake / Makefiles). This allows the runtime to fit comfortably on deeply constrained 8-bit microcontrollers (such as the ATmega328P with 32 KB Flash and 2 KB RAM) while scaling seamlessly up to 32-bit Cortex-M4F/M7 protective relays.

### Subsystem feature sizing and modular cost

The following table provides the typical Flash (ROM) and static RAM footprint for each modular component when compiled for ARM Cortex-M architecture with standard size optimization (`-Os` / `-O2`):

| Subsystem / Feature | Compile-Time Gate (`le_config.h`) | Flash Footprint | RAM Footprint | Key Functionality & Included Elements |
| :--- | :--- | :--- | :--- | :--- |
| **Core VM & Interpreter** | *(Mandatory core)* | ~3.8 KB | ~128 B | VM scan loop, instruction fetch, digital I/O (`%IN`, `%OUT`), boolean registers (`%B`), logic gates (`AND`, `OR`, `XOR`, `NAND`, `NOR`, `NOT`, `MUX`), latches (`RS`, `SR`, `LATCH`), edge detectors (`RTRIG`, `FTRIG`), constants. |
| **Timers & Counters** | *(Standard core)* | ~1.2 KB | 0 B *(workspace)* | IEC 61131-3 timers (`TON`, `TOF`, `TP`) and counters (`CTU`, `CTD`, `CTUD`). State is allocated directly in the preconfigured state image. |
| **Basic Math & Arithmetic** | `LE_ENABLE_FLOAT` | ~1.8 KB | 0 B | 32-bit floating-point and integer arithmetic (`ADD`, `SUB`, `MUL`, `DIV`, `ABS`, `NEG`, `MIN`, `MAX`, `CLAMP`, comparators `CMP_*`). |
| **Analog I/O & Scaling** | `LE_ENABLE_ANALOG` | ~0.9 KB | 0 B | Physical analog input channels (`%AIN`), analog outputs (`%AOUT`), calibration scaling, and clamping. |
| **Word/Bitwise Logic** | `LE_ENABLE_WORD_OPCODES` | ~0.8 KB | 0 B | 32-bit bitwise logic (`WORD_AND`, `WORD_OR`, `WORD_XOR`, `WORD_NOT`, `SHL`, `SHR`). |
| **Transcendental Math** | `LE_ENABLE_TRANSCENDENTAL_MATH` | ~2.1 KB | 0 B | Extended math functions (`SIN`, `COS`, `TAN`, `ASIN`, `ACOS`, `ATAN`, `LN`, `LOG10`, `EXP`, `POW`). Leverages hardware FPU or C libm. |
| **Complex Numbers & Phasors** | `LE_ENABLE_COMPLEX` | ~2.4 KB | 0 B | Complex registers (`%C`), rectangular $\leftrightarrow$ polar conversions (`RECT2POLAR`, `POLAR2RECT`, `RECT2COMPLEX`, `POLAR2COMPLEX`), complex arithmetic (`CADD`, `CSUB`, `CMUL`, `CDIV`). |
| **DSP & Real-Time Filtering** | `LE_ENABLE_DSP` | ~4.5 KB | 0 B *(workspace)* | Lowpass filters (`LPF`), biquad IIR filters (`BIQUAD`), moving average (`MOVING_AVG`), rate limiter (`RATE_LIMITER`), deadband (`DEADBAND`), washout filter (`WASHOUT`), peak detector (`PEAK_DETECTOR`), true RMS (`RMS`), median filter (`MEDIAN`), derivative (`DERIVATIVE`), zero-crossing detector (`ZERO_CROSSING`), 1D lookup table (`LUT_1D`), totalizer (`TOTALIZER`), min/max hold (`MIN_MAX_HOLD`), PID controller (`PID`). |
| **Protective Relay Elements** | `LE_ENABLE_PROTECTION` | ~6.2 KB | 0 B *(workspace)* | ANSI 51 Time-Overcurrent (`OVERCURRENT_51`), ANSI 87 Differential (`DIFF_87`), ANSI 21 Distance Impedance (`DIST_21`), ANSI 87T Phase Angle Transformer Compensation (`PHASE_COMP`), Phasor DFT Extraction (`PHASOR_1P`, `PHASOR_3P`), Symmetrical Components (`SYM_COMP`), Frequency Tracking (`FREQ_EST`, ANSI 81). |
| **Program Storage Manager** | *(Standard loader)* | ~1.6 KB | 0 B *(XIP)* / $(N+1)\times S$ *(RAM)* | Multi-slot flash/EEPROM manager, atomic staging with hidden phantom slot, sector pre-erase, page programming, and CRC32 verification. |
| **Operational NVRAM & Journal** | `LE_NVRAM_BACKEND` | ~2.2 KB | ~80 B | Retentive register persistence, Sequence of Events (SER) log, Transient Disturbance Recorder (oscillography), circular sector wear-leveling journal. |
| **Serial Comms Protocol** | *(Optional comms)* | ~2.8 KB | ~320 B | Framed packet transport (`le_comms.c`), CCITT CRC-16 validation, live program streaming, process-image telemetry streaming, force/override I/O. |
| **Interactive Terminal CLI** | *(Optional CLI)* | ~3.1 KB | ~128 B | ASCII debug shell (`help`, `status`, `caps`, `image`, `force`, `release`, `pulse`, `slot`, `start`, `stop`, `reset`, `timing`). Stripped in production. |
| **Ed25519 Cryptographic Auth** | `LE_ENABLE_CRYPTO` | ~4.2 KB | ~256 B *(stack)* | Constant-time Ed25519 digital signature verification (`le_crypto.c`). Blocks execution of unsigned or tampered `.lebin` containers. |
| **Multi-Rate Execution Engine** | `LE_ENABLE_MULTI_RATE` | ~0.4 KB | ~16 B | Hardware timer prescaler partitioning instructions into Fast (1x tick), Medium (div), and Slow (div) execution domains. |
| **Symbolic Aliases & Pulse** | `LE_ENABLE_ALIASES` | ~0.7 KB | ~32 B | Dynamic named alias lookup and millisecond/second timed register pulse engine. |

> [!NOTE]
> **Zero Runtime Heap**: All stateful block structures marked *(workspace)* consume **0 bytes** of heap (`malloc`). They reside strictly within the bounded static RAM workspace (`LE_RAM_WORKSPACE_BYTES`) configured by the board HAL.

---

### Deployment profile tiers

To simplify microcontroller selection and board porting, LogicElements organizes features into three standard deployment tiers:

| Tier | Target Platforms | Flash Size | RAM Usage | Included Features | Typical Application |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Tier 1: Minimal Micro-PLC** | ATmega328P (8-bit AVR), Cortex-M0+ (STM32C0, SAMD10) | **~6.5 – 8.5 KB** | **~350 – 500 B** + workspace | Core VM, Digital I/O, Timers, Counters, Basic Math, XIP Flash Loader. | Small smart relays, discrete interlocks, lighting/pump controllers, ultra-low-cost MCUs. |
| **Tier 2: Standard Industrial Controller** | STM32F0/F1/G0, RP2040, SAMD21, ESP32, NXP LPC | **~18 – 24 KB** | **~1.2 – 1.8 KB** + workspace | Tier 1 + Analog I/O, PID, DSP Filters, Word Opcodes, Multi-Slot Storage, NVRAM Wear Leveling, Serial Comms Protocol, Aliases. | Industrial automation PLCs, PID motor controllers, process instrumentation, HVAC automation. |
| **Tier 3: Advanced Protection & Automation Relay** | STM32F4/F7/H7, Cortex-M4F/M7, TI C2000, NXP i.MX RT | **~38 – 44 KB** | **~2.5 – 3.8 KB** + workspace | Tier 2 + Protection Relays (51, 87, 21, 81), Complex Phasors, Transcendental Math, Ed25519 Crypto Signing, Oscillography, ASCII CLI, Multi-Rate Domains. | Substation protection relays, feeder management, utility recloser controls, microgrid power automation. |

---

## Variable-arity block mechanism

The 8-byte instruction holds only two data operands. For elements with more
operands, the compiler emits **one `LE_OP_BLOCK` instruction** plus a compact
**descriptor** in a block table:

```c
typedef struct {
    uint8_t in_count;    // inputs
    uint8_t out_count;   // outputs
    uint8_t flags;
    uint8_t reserved;
    // followed in memory by (in_count + out_count) uint16 operand addresses:
    //   args[0..in_count)      inputs
    //   args[in_count..)       outputs
} le_block_desc_t;
```

The disassembler renders a block call with its operand list, e.g. a MUX:

```
[0000]  MUX          BLK[0]     fn:0x01   -> 3->1 [DIN[2] DIN[0] DIN[1] | DOUT[0]]
```

Built-in block functions live below `LE_FUNC_CUSTOM_BASE (0x80)`; custom node
function ids `>= 0x80` are dispatched to the board HAL `ext_call`:

| Func | Name | Signature |
| :--- | :--- | :--- |
| `0x01` | `LE_FUNC_MUX_SELECT` | `[sel, in0, in1] -> [out]` (Boolean MUX) |
| `0x02` | `LE_FUNC_RECT2POLAR` | `[real, imag] -> [mag, angle]` |
| `0x03` | `LE_FUNC_POLAR2RECT` | `[mag, angle] -> [real, imag]` |
| `0x04` | `LE_FUNC_PHASOR_SHIFT` | `[real, imag, delta] -> [real', imag']` |
| `0x05` | `LE_FUNC_COMPLEX2POLAR` | `[cplx] -> [mag, angle]` |
| `0x06` | `LE_FUNC_COMPLEX2RECT` | `[cplx] -> [real, imag]` |
| `0x07` | `LE_FUNC_RECT2COMPLEX` | `[real, imag] -> [cplx]` |
| `0x08` | `LE_FUNC_POLAR2COMPLEX` | `[mag, angle] -> [cplx]` |
| `0x09` | `LE_FUNC_COMPLEX_MUL` | `[c_a, c_b] -> [c_out]` |
| `0x0A` | `LE_FUNC_CLAMP_F` | `[value, min, max] -> [out]` |
| `0x0B` | `LE_FUNC_PHASOR_1P` | `[sample, sync_cplx] -> [cplx]` |
| `0x0C` | `LE_FUNC_DIFF_87` | `[cph0..cphN-1] -> [trip_bool]` (ANSI 87) |
| `0x0D` | `LE_FUNC_DIST_21` | `[v_c, i_c, offset_on] -> [trip_bool]` (ANSI 21) |
| `0x0E` | `LE_FUNC_PHASE_COMP` | `[c_a, c_b, c_c] -> [c_a', c_b', c_c']` (ANSI 87T) |
| `0x0F` | `LE_FUNC_OVERCURRENT_51` | `[i_c, enable] -> [trip_bool]` (ANSI 51) |
| `0x10` | `LE_FUNC_PHASOR_3P` | `[a, b, c, sync_cplx, freq_hz] -> [pa, pb, pc]` |
| `0x11` | `LE_FUNC_FREQ_EST` | `[sample] -> [freq_hz, valid]` (ANSI 81) |
| `0x12` | `LE_FUNC_MUX_SELECT_F` | `[sel, in0_f, in1_f] -> [out_f]` (Float MUX) |
| `0x13` | `LE_FUNC_MUX_SELECT_I` | `[sel, in0_i, in1_i] -> [out_i]` (Int32 MUX) |
| `0x14` | `LE_FUNC_MUX_SELECT_C` | `[sel, in0_c, in1_c] -> [out_c]` (Complex MUX) |

### State block X-Macro architecture (`le_blocks.def`)

All stateful runtime blocks (timers, counters, DSP filters, protection elements) are declared in a centralized X-Macro table: [`src/runtime/include/le_blocks.def`](file:///c:/Users/tanne/OneDrive/Documents/GitHub/LogicElements/src/runtime/include/le_blocks.def).

```c
LE_BLOCK_DEF(id, val, name, struct_type, feature_gate)
```

This single source of truth automatically generates:
1. Block kind enum values (`LE_BLK_TIMER`, `LE_BLK_COUNTER`, etc.).
2. Kind string lookup tables for disassemblers and debuggers.
3. Concrete C state struct size resolvers (`sizeof(le_timer_state_t)`).
4. Feature-gate validation ensuring disabled subsystems emit zero footprint.

## Zero-heap unified RAM workspace (`src/runtime/src/le_rt.c`, `le_process_image.c`)

The runtime draws **all runtime state from one board-tunable RAM pool**
(`LE_RAM_WORKSPACE_BYTES`, a fixed static buffer inside `le_vm_t`). At load the
loader carves it into two contiguous slices:

1. **Register arena** (front) — the packed process-image registers only, sized
   from the `.lebin`'s declared register counts: a contiguous bit bucket (DIN
   bits, then DOUT, then BOOL), followed by 4-byte-aligned float, int, complex,
   analog in, and analog out buckets. An 8-coil / 1-float program therefore uses a handful of
   bytes, not fixed per-type arrays.
2. **State workspace** (back) — `le_rt_workspace()`, a slice the loader fills by
   memcpy'ing a **preconfigured state image** carried in the `.lebin`. The
   compiler bakes every stateful block (defaults + all circuit properties) as
   concrete bytes; the loader just copies it and records each kind group's byte
   offset.

Register accessors (`le_process_image_get/set_*`) compute offsets into the
arena from the bound counts; stateful handlers resolve blocks via
`le_rt_state(kind, idx)`.

| Term | Meaning |
| :--- | :--- |
| `le_process_image_bind` / `le_process_image_regs_len` | Pack/locate the register arena from header counts. |
| `le_rt_bind(base, len)` / `le_rt_workspace_bytes()` | The RAM slice reserved for state after the register arena. |
| `le_rt_set_kind_base(kind, off)` / `le_rt_kind_base(kind)` | Byte offset of a kind group within the image. |
| `le_rt_state(kind, idx)` | Resolves block `idx` of `kind` to `workspace + base + idx * sizeof`. |
| `le_process_image_kind_state(kind, idx)` | Lookup used by handlers (delegates to `le_rt_state`). |

At boot the loader:

1. Computes the register arena size from the header counts and binds the process
   image to the FRONT of the pool.
2. Reads the **state-directive table** (`{ kind, count, size }`), then `memcpy`'s
   the **state image** into the remaining slice and records each kind group's
   byte offset from the directives. No allocation and no config pass.

During a scan, each stateful handler resolves its block via `le_rt_state(kind, idx)`.
"Does it fit?" is checked at load (`register arena + state image ≤
LE_RAM_WORKSPACE_BYTES`), not by any per-type element cap.

**Why:** RAM proportional to use rather than per-type maxima; one shared budget
across registers *and* state blocks (a register-heavy program leaves less room
for state and vice versa); still *zero heap* at runtime.

---

## Loader, storage & NVRAM (`le_loader.c`, `le_storage.c`, `le_nvram.c`)

`le_loader_validate()` checks the container header `LEB2` magic (`0x4C454232`), container version 13,
resource bounds, individual chunk boundaries and payload IEEE 802.3 **CRC32** checksums.
`le_loader_load()` zero-copy-binds instructions, block descriptors, state
slices, and config, then autostarts if flagged. For deeply resource-constrained 8-bit targets (AVR),
`le_loader_load_micro()` provides a minimal ~35-line loader profile scanning only `HEAD` and `INST`
without dynamic memory or crypto overhead.

`le_storage` manages **multi-slot** flash/EEPROM partitions and slot
activation with a hidden **phantom slot** for atomic, CRC32-verified uploads,
allowing several independent programs to be stored and switched live.

`le_nvram` provides persistent storage for:
1. **Retentive process-image registers** (`%B`, `%I`, `%F`, `%C`), dynamically partitioned and isolated per configuration slot so program setpoints never collide.
2. **Sequence of Events (SER)** transition logs with microsecond timestamps.
3. **Event Recorder (Oscillography)** waveform fault captures.
The loaded `.lebin` circuit designates required retentive registers, SER depth, and oscillography records. The runtime validates that these fit within `LE_NVRAM_TOTAL_BYTES` and dynamically writes offsets into the NVRAM manifest at boot. Supports direct byte-addressable NVRAM (FRAM, EEPROM, battery SRAM) or plain Flash via a built-in log-structured wear-leveling journal.

---

## Compiler (`src/compiler`)

### Pipeline (`le_compiler_core.cpp`, `le_code_block.cpp`, `le_optimizer.cpp`)

```
circuit.json + board.json
  1. Parse & validate elements/nets, map %IN/%OUT/%AIN/%AOUT/%B/%I/%F/%C pins & aliases
  2. Structured code block expansion (CodeBlockExpander: lowers CODE_BLOCK into MUX/clamp/logic)
  3. Build Intermediate Representation (IR graph & tags)
  4. Multi-pass optimizer (fixed-point passes: constant folding, boolean simplification,
     inversion folding, CSE, instruction fusion, direct-destination coalescing, dead-code elimination)
  5. Topological scheduling & liveness register allocation
  6. Multi-rate domain partitioning (Fast, Medium, Slow)
  7. Pack extensible chunked container (.lebin v13 LEB2 with HEAD, INST, BLCK, STAT, SIMG, ALIS, TIME, RATE, NVMD)
  8. Optional cryptographic Ed25519 signing (appends SIGN chunk)
```

Optimization levels (`-O0`..`-Os`) are set through `le_compiler_options_t`;
`-O0` keeps the 1:1 schematic-to-bytecode mapping that the per-element assembly
tests lock in.

### Public API (`src/compiler/include/le_compiler.h`)

- `le_compile_json_ex(circuit, board, options, result)`
- `le_compile_json(circuit, board, result)`
- `le_disassemble_bin(bin, ...)` → disassembly text
- `le_export_c_header(bin, name)` → embedded C byte-array header
- `le_compiler_sign_container(in_container, ...)` → cryptographic Ed25519 container signing
- `le_compiler_keygen(out_pk, out_sk, seed)` → Ed25519 keypair generation
- `le_compiler_get_version()` → compiler version string
- `le_compile_result_free(result)` / `le_compiler_free_buffer(ptr)` → memory management

---

## Hardware abstraction layer (`src/runtime/include/le_hal.h`)

`le_hal_t` is a function-pointer table (`g_le_hal`):

| Domain | Callbacks |
| :--- | :--- |
| Lifecycle | `init`, `shutdown`, `get_platform_name` |
| GPIO | `gpio_read`, `gpio_write` |
| Analog | `adc_read`, `adc_read_raw`, `dac_write` |
| Time & Pacing | `get_time_ms`, `get_time_us`, `set_timer_rate` |
| UART | `uart_available`, `uart_read`, `uart_write` |
| Program Storage | `storage_read`, `storage_erase_sector`, `storage_write_page`, `storage_get_xip_ptr` |
| NVRAM / Persistent | `nvram_read`, `nvram_write`, `nvram_write_page`, `nvram_erase_sector` |
| Bus (optional) | `i2c_write`, `i2c_read`, `i2c_write_read`, `spi_transfer` |
| Custom nodes | `ext_call(func_id, args, in_count, out_count, img)`, `get_custom_nodes_json` |

See [PORTS.md](PORTS.md) and [PLATFORM_GUIDE.md](PLATFORM_GUIDE.md) for porting.

---

## Tooling

- `tools/compiler/le_compiler.py` — ctypes compiler SDK (plus `le_board.py`,
  `le_disasm.py`).
- `tools/sim/main.c` — simulator runner / interactive CLI.
- `le_compile` — standalone compiler CLI.
- `le` — unified host CLI suite (`le compile`, `le disasm`, `le upload`, `le monitor`, `le sim`, `le board`).

---

## Testing

The repository ships four automated test suites (see root `CMakeLists.txt`):

| Suite | Coverage |
| :--- | :--- |
| `test_c_runtime` | Process image, opcodes, timers/counters, DSP, protection, loader/storage, NVRAM wear leveling, comms, serial bus, blocks, phasors, workspace. |
| `test_compiler` | Compilation, optimizer passes, multi-output selection, state-image binding, workspace-backed timer execution. |
| `test_element_assembly` | One fixture per element -> compile + parse disassembly for the exact opcode (see [ELEMENT_FIXTURES.md](ELEMENT_FIXTURES.md)). |
| `test_host_comms` | Host-side packet framing, command dispatch, capabilities parsing, and UART transport layer. |