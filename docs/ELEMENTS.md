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

## How element properties reach the runtime

A "complex" element has **tuning properties** (e.g. `PID` `kp`/`ki`/`kd`,
`LPF` `alpha`, `TON` `preset_ms`) plus wire operands. **Every** tuning property
is set by the circuit designer in the circuit JSON and **baked into the `.lebin`**
as concrete bytes of a **preconfigured state image** — no host/BSP code and no
runtime config pass is needed. The compiled circuit *is* the configuration.

At compile time the compiler materializes each stateful block's state struct
(`le_pid_state_t`, `le_lpf_state_t`, `le_timer_state_t`, …): it starts from
factory defaults (e.g. scaler `0..4095 → 0..100`, LPF `alpha=0.1`) and overwrites
fields with the circuit's properties, then appends the raw bytes to the image.

| Element | Properties baked into the state struct |
| :--- | :--- |
| `ANALOGINPUT` (scaled) / `SCALE_F` | `raw_min`,`raw_max`,`scale_min`,`scale_max`,`clamp` |
| `TON` / `TOF` / `TP` | `preset_ms` |
| `CTU` / `CTD` / `CTUD` | `preset` |
| `LPF` | `alpha` |
| `RATE_LIMITER` | `rising_rate`,`falling_rate` |
| `BIQUAD` | `b0`,`b1`,`b2`,`a1`,`a2` |
| `MOVING_AVG` / `RMS` / `MEDIAN` | `window_size` |
| `PEAK_DETECTOR` | `decay_rate` |
| `DEADBAND` | `threshold`,`center` |
| `WASHOUT` | `alpha` |
| `DERIVATIVE` | `alpha`,`gain` |
| `ZERO_CROSSING` | `hysteresis`,`sample_rate_hz` |
| `LUT_1D` | `x[]`, `y[]`, `num_points` |
| `TOTALIZER` | `time_base_sec`,`scale_factor`,`sample_time_sec`,`max_limit` |
| `MIN_MAX_HOLD` | `mode` |
| `OVERCURRENT_51` | `pickup`,`time_dial`,`curve_type` |
| `PID` | `kp`,`ki`,`kd`,`out_min`,`out_max` |
| `DIST_21` | `reach`,`line_angle`,`offset`,`offset_angle`,`prefault_v_threshold`,`prefault_v_duration` |
| `PHASOR_1P` | `samples_per_cycle` |
| `PHASOR_3P` | `samples_per_cycle`, `self_sync` |
| `FREQ_EST` | `nominal_freq_hz`, `hysteresis`, `min_freq_hz`, `max_freq_hz`, `filter_alpha` |
| `DIFF_87` | `input_count` (N complex phasors), `o87p`,`slp1`,`irs1`,`slp2` |
| `PHASE_COMP` | `compensation` (1-12, SEL matrix index) |

At load, `le_loader_load` **memcpy's the state image** into the platform's RAM
state workspace and records each kind group's byte offset; handlers resolve their
block by `workspace + kind_base[kind] + idx * sizeof`. Capacity is checked once
against the workspace size, not per-type element caps.

> The **wire operands** (`in_a`/`in_b`/block `args`) are separate from properties:
> they are the live signals wired in the circuit net, evaluated every scan.
> Properties are static tuning baked at compile time; operands are data.

The only things a host/BSP must still do at runtime are not block-tuning:
serial-bus (I2C/SPI) peripheral payload buffers, and supplying raw ADC values —
the block parameters themselves come from the circuit.

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
- The scaling parameters (`raw_min`, `raw_max`, `scale_min`, `scale_max`,
  `clamp`) are **baked into the `.lebin`** as bytes of the scaler state
  struct (the state image) (see
  [How element properties reach the runtime](#how-element-properties-reach-the-runtime)).

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

Stateful `le_timer_state_t` instances baked into the preconfigured state image.
Each emits one instruction.

> **`preset_ms` is baked into the `.lebin`** as bytes of the timer state
> struct (the state image) (see
> [How element properties reach the runtime](#how-element-properties-reach-the-runtime)),
> so the circuit fully configures the timer — no host/BSP code needed.

### `TON`
On-delay timer. `in` rising arms it; `q = 1` once `now - start >= preset_ms`.

- Properties: `preset_ms` (baked). Input: `in`. Output: `out` (`q`).

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

Stateful `le_counter_state_t` instances baked into the preconfigured state image. Each
counts on rising edges of its trigger.

> **`preset` is baked into the `.lebin`** as bytes of the counter state
> struct (the state image) (see
> [How element properties reach the runtime](#how-element-properties-reach-the-runtime)),
> so the circuit fully configures the counter — no host/BSP code needed.

### `CTU`
Count-up. Input: `cu` (in `in`). Optional reset: `r`. Done when `count >= preset`.

```json
{ "name": "C1", "type": "CTU", "preset": 10 }
```
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

Example disassembly (all single-operand/2-operand floats write a temp):

```
[0000]  ADD_F          0.0f         0.0f         -> T_FLOAT[0]
[0000]  ABS_F          0.0f         0.0f         -> T_FLOAT[0]
```

### `CLAMP` (variable-arity block)

Clamps a float to **independent** `min`/`max` bounds (asymmetric; not `-b, +b`).
Emitted as a **3-in / 1-out block**: `[value, min, max] -> [out]`. Both bounds are
wireable inputs (registers, constants, or computed values), swapped-guarded so
`min > max` still clamps correctly.

```json
{ "name": "CL1", "type": "CLAMP" }
```
```
[0000]  CLAMP_F        BLK[0]       fn:0x0D      -> 3->1 [T_FLOAT[0] T_FLOAT[1] T_FLOAT[2] | T_FLOAT[3]]
```

### Complex arithmetic (`T_CMPLX`)

Complex values use a dedicated register region (`%C`, `LE_REGION_CMPLX`). Each
complex register holds a real+imaginary pair as one `CMPLX[n]` address.

| Element | Opcode | Meaning |
| :--- | :--- | :--- |
| `CADD` | `CADD_F` | `c = a + b` |
| `CSUB` | `CSUB_F` | `c = a - b` |
| `CMUL` | `CMUL_F` | `c = a * b` |
| `CDIV` | `CDIV_F` | `c = a / b` |
| `COMPLEXREGISTER` | `MOVE_C` | named `%C` register (in→out) |

```json
{ "name": "SUM", "type": "CADD" }
```
```
[0000]  CADD_F         CMPLX[0]     CMPLX[1]     -> T_CMPLX[0]
```

### Conversions (complex register <-> rect/polar floats)

All conversions are variable-arity blocks. The six form a complete round-trip set:

| Element (in -> out) | Block func | Signature |
| :--- | :--- | :--- |
| `RECT2POLAR` | `RECT2POLAR` | `[real, imag] -> [mag, angle]` |
| `POLAR2RECT` | `POLAR2RECT` | `[mag, angle] -> [real, imag]` |
| `COMPLEX2POLAR` | `COMPLEX2POLAR` | `[CMPLX] -> [mag, angle]` |
| `COMPLEX2RECT` | `COMPLEX2RECT` | `[CMPLX] -> [real, imag]` |
| `RECT2COMPLEX` | `RECT2COMPLEX` | `[real, imag] -> [CMPLX]` |
| `POLAR2COMPLEX` | `POLAR2COMPLEX` | `[mag, angle] -> [CMPLX]` |

```json
{ "name": "POL", "type": "COMPLEX2POLAR" }
```
```
[0000]  COMPLEX2POLAR  BLK[0]       fn:0x06      -> 1->2 [CMPLX[0] | T_FLOAT[0] T_FLOAT[1]]
```

```json
{ "name": "C", "type": "RECT2COMPLEX" }
```
```
[0000]  RECT2COMPLEX   BLK[0]       fn:0x0B      -> 2->1 [T_FLOAT[0] T_FLOAT[1] | T_CMPLX[0]]
```

> `COMPLEX2RECT` decomposes a complex register into **real and imaginary** floats
> (it does *not* reconstruct a complex from polar floats — use `POLAR2COMPLEX` for
> that). `COMPLEX2POLAR` decomposes into magnitude and angle.
> ---

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
Closed-loop PID controller. State is an `le_pid_state_t` baked into the state image.

- Inputs: `sp` (setpoint), `pv` (process value). Output: control output.
- Properties: `kp`, `ki`, `kd`, `out_min`, `out_max` — **baked into the `.lebin`**
  as bytes of the PID state struct (the state image) (see
  [How element properties reach the runtime](#how-element-properties-reach-the-runtime)).

```
[0000]  PID            0.0f         0.0f         -> T_FLOAT[0]
```

### `OVERCURRENT` / `OVERCURRENT_51`
IEC/IEEE inverse-time overcurrent (**ANSI 51**). `OVERCURRENT_51` is the preferred
ANSI-convention name; `OVERCURRENT` is accepted as an alias. Emitted as a
**variable-arity block builtin** (`LE_FUNC_OVERCURRENT_51`): one **complex phasor**
`a` + a boolean **`enable`** input -> bool trip.

- `a` / `current` / `i_c` / `in`: complex phasor input; its **magnitude** drives the
  inverse-time curve (`M = |i_c| / pickup` in per-unit). Unconnected = `LE_CONST_ZERO_C`.
- `enable` / `en` / `on` / `b`: **boolean enable** input. It is **AND'd with the
  pickup evaluation BEFORE the timing accumulator** — only an ENABLED element with
  `M > 1` accrues operate time; otherwise it cools along the reset curve and can
  never trip. Unconnected = `LE_CONST_TRUE` (always enabled). Wire a directionality
  boolean here for a **ground directional overcurrent** element, or any designer
  boolean to just enable/disable it.
- Output: trip boolean.

**Inverse-time model** (IEEE C37.112 / IEC 60255):

```
t_operate = time_dial * (A / (M^p - 1) + B),   M = |i_c| / pickup  (for M > 1)
t_reset   = time_dial * (4.6  / (1 - M^2))                           (for M < 1)
```

The runtime integrates `dt / t_operate` into an accumulator each scan and **trips
when it reaches 1.0** (= the full operate time has elapsed); it cools symmetrically
along `t_reset` below pickup. Divisions are clamped near `M = 1` (the operate/reset
times blow up there, so the accumulator simply holds — no divide-by-zero, no
denominator sign flip for a disabled element sitting above pickup).

- `curve_type` selects a standard curve (default **IEC Very Inverse**):
  - IEC `NORMAL` (A=0.14, p=0.02), `VERY` (A=13.5, p=1), `EXTREME` (A=80, p=2)
  - IEEE `MODERATELY` (A=0.0515, B=0.114, p=0.02), `VERY` (A=19.61, B=0.491, p=2),
    `EXTREME` (A=28.2, B=0.1217, p=2), `SHORT-TIME` (A=0.00342, B=0.00262, p=0.02),
    `LONG-TIME` (A=26.13, B=0.349, p=2)
- **Custom curve**: provide `curve_a` / `curve_b` / `curve_p` (or `a_coeff`/`b_coeff`/
  `p_coeff`) — the element is marked `LE_CURVE_CUSTOM` and the coefficients are
  baked verbatim.
- Properties: `pickup`, `time_dial`, `curve_type`, `curve_a/curve_b/curve_p` —
  **baked into the `.lebin`** and resolved to coefficients the target runtime uses
  directly (see [How element properties reach the runtime](#how-element-properties-reach-the-runtime)).

```
{ "type": "OVERCURRENT_51", "pickup": 600.0, "time_dial": 1.0, "curve_type": "IEC_VERY" }
{ "type": "OVERCURRENT_51", "pickup": 600.0, "time_dial": 1.0, "curve_a": 30.0, "curve_p": 2.0 }
```

```
[0000]  OVERCURRENT_51 BLK[0]       -> 2->1 [CMPLX[0] | T_BOOL[0] | T_BOOL[1]]
```

> The obsolete scalar `LE_OP_OVERCURRENT` opcode was **removed** (replaced by a
> reserved placeholder at `0x71`); the element always compiles to the block form.

### `PHASOR_1P` (variable-arity block)
Dynamic frequency phasor extractor with self-sync support. Emitted as a **3-in / 1-out block** producing a
**complex phasor** (`T_CMPLX`), synchronized to a reference phasor so the result
stays stable relative to it instead of rotating with the system frequency.

- **Inputs**: `sample` (float), `sync` (complex phasor), `freq_hz` (float). Output: `phasor` (`T_CMPLX`)

- **Properties**:
  - `samples_per_cycle` (default 16): Number of samples per power cycle for the DFT window
  - `self_sync` (default false): When true, output angle is 0-degree referenced; when false, referenced to sync phasor

- **Cadence**: The scan rate is **not** an element property. It is strictly derived
  from the enforced VM scan period (`le_rt_scan_dt()`); the per-element
  `scan_rate_hz` / `sample_rate_hz` override is removed.

- **Behavior**:
  - The `freq_hz` input determines which subset of high-rate samples to use for the DFT
  - Higher frequency = fewer samples per cycle; lower frequency = more samples per cycle
  - DFT uses linear interpolation to synthesize perfectly spaced samples
  - When `self_sync` = false (default): Output angle is referenced to the sync phasor (sync angle = 0)
  - When `self_sync` = true: Output angle is 0-degree referenced (self-synchronized)

```json
{ "name": "P1", "type": "PHASOR_1P", "samples_per_cycle": 16, "scan_rate_hz": 2400.0, "self_sync": false }
```
```
[0000]  PHASOR_1P      BLK[0]       fn:0x05      -> 2->1 [AIN[0] CMPLX[0] | T_CMPLX[0]]
```

### `PHASOR_3P` (variable-arity block)
Three-phase dynamic frequency phasor extractor. Banks **three independent
single-phase phasor extractors** (a, b, c) against a single bus reference phasor
and a single system frequency, so all three output phasors stay phase-correct
relative to one another. Each phase runs the same `PHASOR_1P` DFT and
self-sync logic, sharing all element properties.

- **Inputs**: `sample_a`, `sample_b`, `sample_c` (float), `sync` (complex phasor), `freq_hz` (float).
  Outputs: `phasor_a`, `phasor_b`, `phasor_c` (each `T_CMPLX`; also aliased as `a`/`b`/`c` and `out`).

- **Properties** (applied identically to all three phases):
  - `samples_per_cycle` (default 16): Number of samples per power cycle for the DFT window
  - `self_sync` (default false): When true, each output angle is 0-degree referenced; when false, referenced to the shared sync phasor

- **Cadence**: Sample rate derives from the enforced VM scan period
  (`le_rt_scan_dt()`); no per-element `scan_rate_hz` override.

- **Behavior**: Identical to `PHASOR_1P`, but the `sample_a/b/c` inputs each feed
  their own DFT accumulator. The `freq_hz` input picks the (shared) DFT window and
  the `sync` phasor is the common reference at which all three phase angles are
  expressed. With a balanced `cos(2πft + φ)` set you recover three phasors with
  magnitudes ≈ amplitude and angles ≈ φ_a/φ_b/φ_c.

```json
{ "name": "P3", "type": "PHASOR_3P", "samples_per_cycle": 16, "scan_rate_hz": 2400.0, "self_sync": false }
```
```
[0000]  PHASOR_3P      BLK[0]       fn:0x10      -> 5->3 [AIN[0] AIN[1] AIN[2] CMPLX[0] AIN[3] | T_CMPLX[0] T_CMPLX[1] T_CMPLX[2]]
```

### `FREQ_EST` (variable-arity block, ANSI 81)
Dynamic fundamental frequency estimator for under/over-frequency protection
(81U/81O) and dynamic phasor tracking. Emitted as a **1-in / 2-out** block: a float
sample in; a float `freq_hz` and a bool `valid` out. The sampling rate is strictly
derived from the enforced VM scan cadence (`le_rt_scan_dt()`); **no sample-rate
property is stored** — the circuit owns the scan rate, not the element.

- **Inputs**: `sample` (float; aliases `in`, `x`, `a`, `pv`).
  Outputs: `freq_hz` (float; aliases `freq`, `out`), `valid` (bool; aliases `lock`, `tracking`).

- **Properties**:
  - `nominal_freq_hz` (default 60): Fallback center frequency (50 or 60 Hz)
  - `hysteresis` (default 0.05): Noise deadband threshold around zero
  - `min_freq_hz` (default 45), `max_freq_hz` (default 65): Validity bounds
  - `filter_alpha` (default 0.0): Optional EWMA output smoothing (0 = unfiltered)

- **Behavior** (hysteresis noise rejection → sub-sample zero-crossing interpolation
  → period/frequency → bounds gating):
  - Healthy in-bounds measurement → `freq_hz` = measured, `valid` = true
  - **Out-of-bounds but measured** → `freq_hz` = measured, `valid` = false
    (the true under/over-frequency is still reported so 81U/81O can trip)
  - **Loss of potential / stall** (no crossing within 1.25× the `min_freq_hz`
    period) → `freq_hz` = `nominal_freq_hz`, `valid` = false

```json
{ "name": "F1", "type": "FREQ_EST", "nominal_freq_hz": 60.0, "hysteresis": 0.05, "min_freq_hz": 45.0, "max_freq_hz": 65.0 }
```
```
[0000]  FREQ_EST       BLK[0]       fn:0x11      -> 1->2 [AIN[0] | T_FLOAT[0] T_BOOL[0]]
```

### `RECT2POLAR` (variable-arity block)
Rectangular → polar. **2-in / 2-out** block: `[real, imag] -> [mag, angle]`.

```json
{ "name": "P1", "type": "RECT2POLAR" }
```
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
[0000]  SYM_COMP       BLK[0]       -> 3->3 [FLOAT[0] FLOAT[1] FLOAT[2] | T_FLOAT[0] T_FLOAT[1] T_FLOAT[2]]
```

### `DIFF` / `DIFF_87` (variable-arity block, dynamic N-input, dual-slope)
Dual-slope differential protection (**ANSI 87**) over **N user-defined complex
phasor inputs** (`DIFF_87` is the preferred ANSI-convention name; `DIFF` is an
alias). The designer sets `input_count` (2..**30**; large-bus differential uses
many phasor terminals); the compiler emits one block call with N complex inputs.

The **operate** current is the magnitude of the phasor vector sum; the
**restraint** current is the **sum** of the phasor magnitudes (no averaging —
large-bus convention). The relay trips when operate exceeds a **dual-slope**
characteristic (SEL-style `O87P`/`SLP1`/`IRS1`/`SLP2`):

```
threshold = o87p + slp1 * I_rt                 for I_rt <= irs1
threshold = o87p + slp1*irs1 + slp2*(I_rt-irs1) for I_rt >  irs1
```

- Properties: `input_count` (complex inputs, 2..30), plus the dual-slope curve:
  `o87p` (pickup, default 0.3), `slp1` (first slope, default 0.25),
  `irs1` (restraint knee, default 1.5, alias `ips1`), `slp2` (second slope,
  default 0.60) — all **baked into the `.lebin`** state image.

```json
{ "name": "D1", "type": "DIFF_87", "input_count": 3, "o87p": 0.3, "slp1": 0.25, "irs1": 1.5, "slp2": 0.6 }
```
```
[0000]  DIFF_87        BLK[0]       fn:0x09      -> 3->1 [CMPLX[0] CMPLX[1] CMPLX[2] | T_BOOL[0]]
```

### `PHASE_COMP` (variable-arity block, 3-in/3-out, ANSI 87T)
Transformer differential **phase compensation** (aliases `TRANSFORM_33`, `TCOMP`).
This is *not* a diff element — it is a transform that aligns the winding currents
before they are summed by a downstream `DIFF_87`. It takes **three complex phasors**
(one per phase/terminal) and applies the **SEL delta/wye compensation matrix
`M(k)`** (k = 1..12) to produce three compensated phasors:

```
I'_x = s * sum_j M(k)[x][j] * I_j      s = 1/sqrt(3) for odd k, 1/3 for even k
```

The matrices are the standard wye/delta transformer winding compensation table
(e.g. k=1: `[1,-1,0; 0,1,-1; -1,0,1]`), so with a balanced through-load the
compensated phasors cancel to ~0 (operate = 0). `comp` selects the SEL
compensation matrix index k.

- Inputs: `a`, `b`, `c` (phase phasors). Outputs: `a'`, `b'`, `c'`.
- Property: `compensation` (1-12, default 6). Baked into the `.lebin` state
  image as `le_comp33_state_t`.

```json
{ "name": "T1", "type": "PHASE_COMP", "compensation": 6 }
```
```
[0000]  PHASE_COMP     BLK[0]       fn:0x0E      -> 3->3 [CMPLX[0] CMPLX[1] CMPLX[2] | T_CMPLX[0] T_CMPLX[1] T_CMPLX[2]]
```

### `DIST_21` — Mho distance relay with prefault-voltage memory
Mho-circle distance relay (ANSI 21). Takes two **complex phasor** inputs
(voltage `v`, current `i`) plus a boolean `offset_on`, and trips when the
measured apparent impedance `Z = V / I` lies inside the mho circle (center at
`reach/2` on the line angle, radius `reach/2`; an *offset-mho* circle is used
when `offset_on` is true, shifting the center by the offset phasor).

- Inputs: `v` (complex phasor), `i` (complex phasor), `offset_on` (boolean,
  applies the mho-offset circle shift). Output: single boolean trip.
- Properties:
  - `reach` — zone reach (ohms)
  - `line_angle` — line/impedance angle (degrees)
  - `offset` — offset magnitude (ohms); `offset_angle` — offset phasor angle (degrees)
  - `prefault_v_threshold` — if live `|V|` falls below this, treat as a fault
  - `prefault_v_duration` — how long (ms) to keep using the remembered
    pre-fault voltage phasor after the measured voltage is depressed

When the live voltage collapses below `prefault_v_threshold`, the relay
substitutes the last known-good `V` phasor for `prefault_v_duration` ms, so the
apparent impedance stays accurate during a fault (SEL-style voltage memory).

```json
{ "name": "D21", "type": "DIST_21", "reach": 10, "line_angle": 75,
  "offset": 0, "offset_angle": 75,
  "prefault_v_threshold": 0.5, "prefault_v_duration": 80 }
```
```
[0000]  DIST_21        BLK[0]       fn:0x0A      -> 3->1 [CMPLX[0] CMPLX[1] BOOL[2] | T_BOOL[0]]
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

Every DSP filter is a stateful block (state baked into the state image) emitting one
instruction; `modifier` carries the per-kind instance index.

> **Baked-in tuning.** Every DSP filter's properties below are written into the
> `.lebin` state image (see (see
> [How element properties reach the runtime](#how-element-properties-reach-the-runtime)).
> Unspecified properties use factory defaults — no host/BSP code needed.

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