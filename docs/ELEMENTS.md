# Elements Reference

Every element LogicElements can compile: what it does, how to use it (ports /
properties in the circuit JSON), and an example disassembly of the instruction
it lowers to at `-O0`.

Notation used in example disassembly:

- `T_BOOL[n]` / `T_FLOAT[n]` — transient (compiler-allocated) temp registers.
- `BOOL[n]` / `FLOAT[n]` / `INT[n]` — user registers (`%M` / `%R` / `%N`).
- `DIN[n]` / `DOUT[n]` — digital pins (`%I` / `%Q`).
- `TIMER[n]` / `COUNTER[n]` — stateful timer/counter instances.
- `BLK[i] fn:0x.. -> in->out [args | outs]` — a variable-arity block call.
- `-` — unconnected/unused operand (defaults to a literal).

A circuit net connects an element port to a consumer port:

```json
{ "output": { "name": "IN0", "port": "out" },
  "inputs": [ { "name": "AND1", "port": "a" } ] }
```

---

## I/O and storage

### `DIGITALINPUT`
A physical digital input pin. Produces **no instruction** — it only defines an
address that consumer elements reference.

- Properties: `address` (`%I0`, `%I1`, ...).
- Output port: `out`.

```json
{ "name": "IN0", "type": "DIGITALINPUT", "address": "%I0" }
```
```
(no instruction emitted)
```

### `DIGITALOUTPUT`
A physical digital output pin. Lowers to a `MOVE` from its source.

- Properties: `address` (`%Q0`, `%Q1`, ...).
- Input port: `in`.

```json
{ "name": "OUT0", "type": "DIGITALOUTPUT", "address": "%Q0" }
```
```
[0000]  MOVE           DIN[0]       -            -> DOUT[0]
```

### `BOOLREGISTER`
A named boolean coil (`%M`). Lowers to a `MOVE`.

- Properties: `address` (`%M0`, ...).
- Input port: `in`. Output port: `out`.

```json
{ "name": "B0", "type": "BOOLREGISTER", "address": "%M0" }
```
```
[0000]  MOVE           DIN[0]       -            -> BOOL[0]
```

### `FLOATREGISTER`
A named float register (`%R`). Lowers to a `MOVE_F`.

- Properties: `address` (`%R0`, ...).
- Input port: `in`. Output port: `out`.

```json
{ "name": "R0", "type": "FLOATREGISTER", "address": "%R0" }
```
```
[0000]  MOVE_F         1.0f         -            -> FLOAT[0]
```

### `INTREGISTER`
A named integer register (`%N`). Lowers to a `MOVE`.

- Properties: `address`.
- Input port: `in`. Output port: `out`.

```json
{ "name": "N0", "type": "INTREGISTER", "address": "%N0" }
```
```
[0000]  MOVE           DIN[0]       -            -> INT[0]
```

### `ANALOGINPUT`
An analog input channel (`AIN`). In **raw** mode produces no instruction. In
**float / scaled** mode it emits a `SCALE_F` that linear-maps the ADC value.

- Properties: `channel`, `mode` (`raw` | `float` | `scaled`), `raw_min`,
  `raw_max`, `scale_min`, `scale_max`, `units`, `clamp`.

```json
{ "name": "AI0", "type": "ANALOGINPUT", "channel": 0, "mode": "float",
  "raw_min": 0, "raw_max": 4095, "scale_min": 0.0, "scale_max": 100.0 }
```
```
[0000]  SCALE_F        AIN[0]       SCL[0]       -> T_FLOAT[0]
```

### `CONSTANT`
A literal that feeds consumers. It folds away (`LE_CONST_FALSE` / `TRUE` /
`ZERO_F` / `ONE_F`) and produces **no instruction**.

- Properties: `dataType`/`data_type` (`Boolean` | `Float` | `Int`), `value`.

```json
{ "name": "C1", "type": "CONSTANT", "dataType": "Boolean", "value": true }
```
```
(no instruction emitted)
```

---

## Logic gates

Boolean combinational logic. Each lowers to exactly one instruction writing a
temp boolean.

### `AND`
- Inputs: `a`, `b`. Output: `out` = `a AND b`.
```
[0000]  AND            FALSE        -            -> T_BOOL[0]
```

### `OR`
- Inputs: `a`, `b`. Output = `a OR b`.
```
[0000]  OR             FALSE        -            -> T_BOOL[0]
```

### `NOT`
- Input: `in`. Output = `NOT in`.
```
[0000]  NOT            FALSE        -            -> T_BOOL[0]
```

### `XOR`
- Inputs: `a`, `b`. Output = `a XOR b`.
```
[0000]  XOR            FALSE        -            -> T_BOOL[0]
```

### `NAND` / `NOR`
- Inputs: `a`, `b`. Output = `NOT (a AND b)` / `NOT (a OR b)`.
```
[0000]  NAND           FALSE        -            -> T_BOOL[0]
[0000]  NOR            FALSE        -            -> T_BOOL[0]
```

### `MUX` (variable-arity block)
Selects between two inputs by a selector. Emitted as a **3-in / 1-out block**.

- Inputs: `sel`, `in0`, `in1`. Output = `sel ? in1 : in0`.
```
[0000]  MUX            BLK[0]       fn:0x01      -> 3->1 [DIN[2] DIN[0] DIN[1] | DOUT[0]]
```
---

## Edge detection and latches

### `RTRIG` / `FTRIG`
Rising- / falling-edge detectors (single output pulse).

- Input: `in` (signal). Output: `out` (one-cycle pulse).

```json
{ "name": "R1", "type": "RTRIG" }
```
```
[0000]  RTRIG          FALSE        -            -> T_BOOL[0]
[0000]  FTRIG          FALSE        -            -> T_BOOL[0]
```

### `SR` / `RS` / `LATCH`
Set/Reset latches. `SR` is set-dominant, `RS` is reset-dominant; `LATCH` maps
to `SR` (`dominant: "Set Dominant"`) or `RS` (`dominant: "Reset Dominant"`).
Output `q` is persistent memory held at the destination.

- Inputs: `s`/`set`, `r`/`reset`. Output: `out` = `q`.

```json
{ "name": "L1", "type": "RS" }
```
```
[0000]  SR             FALSE        -            -> T_BOOL[0]
[0000]  RS             FALSE        -            -> T_BOOL[0]
```

---

## Timers

Stateful `le_timer_state_t` instances bound into the zero-heap arena at load;
the preset is configured by the runtime/board. Each emits one instruction.

### `TON`
On-delay timer. `in` rising arms it; `q = 1` once `now - start >= preset_ms`.

- Properties: `preset_ms`. Input: `in`. Output: `out` (`q`).

```json
{ "name": "T1", "type": "TON", "preset_ms": 1000 }
```
```
[0000]  TON            FALSE        -            -> TIMER[0]
```

### `TOF`
Off-delay timer. Input: `in`. Output: `out` (`q`).

```json
{ "name": "T1", "type": "TOF", "preset_ms": 1000 }
```
```
[0000]  TOF            FALSE        -            -> TIMER[0]
```

### `TP`
Pulse timer. Input: `in`. Output: `out` (`q`).

```json
{ "name": "T1", "type": "TP", "preset_ms": 1000 }
```
```
[0000]  TP             FALSE        -            -> TIMER[0]
```

---

## Counters

Stateful `le_counter_state_t` instances bound into the zero-heap arena. Each
counts on rising edges of its trigger; the preset is runtime/board configured.

### `CTU`
Count-up. Input: `cu` (in `in`). Optional reset: `r`. Done when `count >= preset`.

```
[0000]  CTU            FALSE        -            -> COUNTER[0]
```

### `CTD`
Count-down. Input: `cd`. Done when `count <= 0`.

```
[0000]  CTD            FALSE        -            -> COUNTER[0]
```

### `CTUD`
Count up/down (`cu` in `in_a`, `cd` in `in_b`). Done when `count >= preset`.

```
[0000]  CTUD           FALSE        -            -> COUNTER[0]
```

---

## Float arithmetic

Each float op reads two float operands and writes a temp float.

| Element | Opcode | Formula |
| :--- | :--- | :--- |
| `ADD` | `ADD_F` | `a + b` |
| `SUB` / `SUBTRACT` | `SUB_F` | `a - b` |
| `MUL` / `MULTIPLY` | `MUL_F` | `a * b` |
| `DIV` / `DIVIDE` | `DIV_F` | `a / b` (0-safe) |
| `ABS` | `ABS_F` | `|a|` |
| `NEG` | `NEG_F` | `-a` |
| `MIN` | `MIN_F` | `min(a,b)` |
| `MAX` | `MAX_F` | `max(a,b)` |
| `CLAMP` | `CLAMP_F` | clamp `a` within `[-b, +b]` |

Example disassembly (all single-operand/2-operand floats write a temp):

```
[0000]  ADD_F          0.0f         0.0f         -> T_FLOAT[0]
[0000]  ABS_F          0.0f         0.0f         -> T_FLOAT[0]
[0000]  CLAMP_F        0.0f         0.0f         -> T_FLOAT[0]
```

---

## Comparisons

Float inputs compared to a boolean output (`a OP b`).

| Element | Opcode |
| :--- | :--- |
| `CMP_GT` | `CMP_GT` |
| `CMP_LT` | `CMP_LT` |
| `CMP_GE` | `CMP_GE` |
| `CMP_LE` | `CMP_LE` |
| `CMP_EQ` | `CMP_EQ` (epsilon-tolerant) |
| `CMP_NE` | `CMP_NE` |

```
[0000]  CMP_EQ         0.0f         -            -> T_BOOL[0]
```
---

## Control, protection & phasor conversions

### `PID`
Closed-loop PID controller. State is an `le_pid_state_t` bound into the heap.

- Inputs: `sp` (setpoint), `pv` (process value). Output: control output.
- Properties: `kp`, `ki`, `kd`, `out_min`, `out_max`.

```
[0000]  PID            0.0f         0.0f         -> T_FLOAT[0]
```

### `OVERCURRENT`
IEC/IEEE inverse-time overcurrent (ANSI 51). Accumulates overcurrent time;
trips once sustained past the time dial.

- Input: current (in `in_a`). Output: trip boolean.
- Properties: `pickup`, `time_dial`.

```
[0000]  OVERCURRENT    <float>      -            -> T_BOOL[0]
```

### `PHASOR_1P` (variable-arity block)
1-phase DFT phasor extractor with **sync** (mag + angle reference). Emitted as a
**3-in / 2-out block**; the phasor instance is the descriptor index.

- Inputs: `sample`, `sync_mag`, `sync_angle`. Outputs: `magnitude`, `angle`.

```json
{ "name": "P1", "type": "PHASOR_1P", "samples_per_cycle": 16 }
```
```
[0000]  PHASOR_1P      BLK[0]       fn:0x05      -> 3->2 [0.0f 1.0f 0.0f | T_FLOAT[0] T_FLOAT[1]]
```

### `RECT2POLAR` (variable-arity block)
Rectangular → polar. **2-in / 2-out** block: `[real, imag] -> [mag, angle]`.

```
[0000]  RECT2POLAR     BLK[0]       fn:0x02      -> 2->2 [T_FLOAT[0] T_FLOAT[1] | T_FLOAT[1] T_FLOAT[0]]
```

### `POLAR2RECT` (variable-arity block)
Polar → rectangular. **2-in / 2-out** block: `[mag, angle] -> [real, imag]`.

```
[0000]  POLAR2RECT     BLK[0]       fn:0x03      -> 2->2 [...]
```

### `PHASOR_SHIFT` (variable-arity block)
Rotates a phasor CCW by a wired angle. **3-in / 2-out** block:
`[real, imag, delta_rad] -> [real', imag']`.

```
[0000]  PHASOR_SHIFT   BLK[0]       fn:0x04      -> 3->2 [...]
```

### `SYM_COMP`
Three-phase symmetrical (Fortescue) components from phasor state. Reads
phase phasors by base index; writes `seq_0/1/2`.

```
[0000]  SYM_COMP       <phIdxs>     -            -> T_FLOAT[0]
```

### `DIFF_87`
SEL-style dual-slope percentage differential protection.

```
[0000]  DIFF_87        <ph1>        <ph2>        -> T_BOOL[0]
```

### `DIST_21`
Mho-circle distance relay zone.

```
[0000]  DIST_21        <vIdx>       <iIdx>       -> T_BOOL[0]
```

---

## Serial bus

### `I2C` / `SPI`
Poll/transact with I2C/SPI peripherals. State is an `le_i2c/spi_device_state_t`.

```
[0000]  I2C            FALSE        -            -> T_BOOL[0]
[0000]  SPI            FALSE        -            -> T_BOOL[0]
```

---

## DSP & filters

Every DSP filter is a stateful block (state in the arena) emitting one
instruction; `modifier` carries the per-kind instance index.

| Element | Opcode | Key properties |
| :--- | :--- | :--- |
| `LPF` | `LPF_1P` | `alpha` |
| `BIQUAD` | `BIQUAD_IIR` | `b0 b1 b2 a1 a2` |
| `MOVING_AVG` | `MOVING_AVG` | `window_size` |
| `RATE_LIMITER` | `RATE_LIMITER` | `rising_rate`, `falling_rate` |
| `DEADBAND` | `DEADBAND` | `threshold`, `center` |
| `WASHOUT` | `WASHOUT` | `alpha` |
| `PEAK_DETECTOR` | `PEAK_DETECTOR` | `decay_rate` |
| `RMS` | `RMS` | `window_size` |
| `MEDIAN` | `MEDIAN` | `window_size` |
| `DERIVATIVE` | `DERIVATIVE` | `alpha`, `gain` |
| `ZERO_CROSSING` | `ZERO_CROSSING` | `hysteresis`, `sample_rate_hz` |
| `LUT_1D` | `LUT_1D` | `x`[], `y`[] breakpoints |
| `TOTALIZER` | `TOTALIZER` | `time_base_sec`, `scale_factor`, `sample_time_sec`, `max_limit` |
| `MIN_MAX_HOLD` | `MIN_MAX_HOLD` | `mode` |

Example disassembly:

```
[0000]  LPF_1P         0.0f         0.0f         -> T_FLOAT[0]
[0000]  BIQUAD_IIR     0.0f         0.0f         -> T_FLOAT[0]
[0000]  WASHOUT        0.0f         0.0f         -> T_FLOAT[0]
[0000]  MEDIAN         0.0f         0.0f         -> T_FLOAT[0]
[0000]  ZERO_CROSSING  0.0f         -            -> T_FLOAT[0]
[0000]  LUT_1D         0.0f         0.0f         -> T_FLOAT[0]
[0000]  TOTALIZER      0.0f         -            -> T_FLOAT[0]
```

---

## Tags & board extensibility

### `TAG`
A named send/receive pair. `TAG_SEND`/`TAG_RECEIVE` share a `tag_name`; the
compiler resolves the receiver to the sender's source address at compile time,
so a full tag flow lowers to a single `MOVE`.

```json
{ "name": "TS1", "type": "TAG_SEND", "direction": "send", "tag_name": "T1" },
{ "name": "TR1", "type": "TAG_RECEIVE", "direction": "receive", "tag_name": "T1" }
```
```
[0000]  MOVE           DIN[0]       -            -> DOUT[0]
```

### `LE_CUSTOM` (variable-arity block)
A board-defined custom node. Emitted as a **block** with a function id
`>= LE_FUNC_CUSTOM_BASE (0x80)`, dispatched to the HAL `ext_call`.

- Properties: `function_id`, `output_type` (`bool` | `float` | `int`).

```json
{ "name": "X1", "type": "LE_CUSTOM", "function_id": 0x81, "output_type": "bool" }
```
```
[0000]  BLOCK          BLK[0]       fn:0x81      -> 0->1 [- | T_BOOL[0]]
```