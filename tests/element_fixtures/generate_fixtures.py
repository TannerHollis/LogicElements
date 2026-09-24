#!/usr/bin/env python3
"""Generate per-element compiler fixtures for tests/test_element_assembly.c.

Each fixture is a pure LogicElements circuit JSON named <SPEC>.json. The
filename encodes the element under test; the C harness owns the
element->opcode oracle and derives expected mnemonics from the filename.

Usage: python generate_fixtures.py [out_dir]
"""
import json
import os
import sys

OUT = os.path.dirname(os.path.abspath(__file__))


def e(name, etype, **props):
    d = {"name": name, "type": etype}
    d.update(props)
    return d


def din(name, addr="%I0"):
    return e(name, "DIGITALINPUT", address=addr)


def dout(name, addr="%Q0"):
    return e(name, "DIGITALOUTPUT", address=addr)


def net(src, sp, dst, dp):
    return {"output": {"name": src, "port": sp}, "inputs": [{"name": dst, "port": dp}]}


def standalone(etype, **props):
    return [e("N1", etype, **props)], []


SPECS = []


def add(key, build):
    SPECS.append((key, build))


# --- I/O & storage ---
add("DIGITALINPUT", lambda: ([din("IN1")], []))
add("DIGITALOUTPUT", lambda: ([din("IN1"), dout("OUT1")], [net("IN1", "out", "OUT1", "in")]))
add("ANALOGINPUT_plain", lambda: ([e("AI1", "ANALOGINPUT", channel=0, mode="raw")], []))
add("ANALOGINPUT_scaled", lambda: ([e("AI1", "ANALOGINPUT", channel=0, mode="float",
    raw_min=0.0, raw_max=4095.0, scale_min=0.0, scale_max=100.0)], []))
add("BOOLREGISTER", lambda: ([din("IN1"), e("M1", "BOOLREGISTER", address="%M0")],
    [net("IN1", "out", "M1", "in")]))
add("FLOATREGISTER", lambda: ([e("C1", "CONSTANT", dataType="Float", value=1.0),
    e("R1", "FLOATREGISTER", address="%R0")], [net("C1", "out", "R1", "in")]))
add("INTREGISTER", lambda: ([din("IN1"), e("I1", "INTREGISTER", address="%N0")],
    [net("IN1", "out", "I1", "in")]))
add("CONSTANT", lambda: ([e("C1", "CONSTANT", dataType="Boolean", value=True)], []))

# --- Logic gates (standalone -> exactly one opcode at -O0) ---
for _t in ["AND", "OR", "XOR", "NAND", "NOR"]:
    add(_t, lambda t=_t: standalone(t))

# --- Multi-input logic gates (decomposed into 2-input gates) ---
# Simple 3-input gate with inputs directly connected
for _t in ["AND", "OR", "XOR", "NAND", "NOR"]:
    def build_multi(t=_t):
        # Create a 3-input gate with inputs IN1, IN2, IN3
        ins = [din("IN1", "%I0"), din("IN2", "%I1"), din("IN3", "%I2")]
        outs = [dout("OUT", "%OUT0")]
        # Use a single gate with multiple inputs (will be decomposed)
        gate = e(f"{t}3", t)
        # Wire inputs using standard port names
        nets = [
            net("IN1", "out", f"{t}3", "in_a"),
            net("IN2", "out", f"{t}3", "in_b"),
            net("IN3", "out", f"{t}3", "in_c"),
            net(f"{t}3", "out", "OUT", "in")
        ]
        return ins + outs + [gate], nets
    add(f"{_t}_3INPUT", build_multi)

add("NOT", lambda: standalone("NOT"))
add("MUX", lambda: standalone("MUX"))

# --- Edge detection & latches ---
add("RTRIG", lambda: standalone("RTRIG"))
add("FTRIG", lambda: standalone("FTRIG"))
add("SR", lambda: standalone("SR"))
add("RS", lambda: standalone("RS"))
add("LATCH_set", lambda: standalone("LATCH", dominant="Set Dominant"))
add("LATCH_reset", lambda: standalone("LATCH", dominant="Reset Dominant"))

# --- Timers & counters ---
add("TON", lambda: standalone("TON", preset_ms=1000))
add("TOF", lambda: standalone("TOF", preset_ms=1000))
add("TP", lambda: standalone("TP", preset_ms=1000))
add("CTU", lambda: standalone("CTU", preset=10))
add("CTD", lambda: standalone("CTD", preset=10))
add("CTUD", lambda: standalone("CTUD", preset=10))

# --- Float math ---
for _t in ["ADD", "SUB", "MUL", "DIV", "ABS", "NEG", "MIN", "MAX"]:
    add(_t, lambda t=_t: standalone(t))
add("CLAMP", lambda: standalone("CLAMP"))

# --- Complex math (T_CMPLX operands/out) ---
for _t in ["CADD", "CSUB", "CMUL", "CDIV"]:
    add(_t, lambda t=_t: standalone(t))
add("COMPLEXREGISTER", lambda: ([e("C1", "COMPLEXREGISTER", address="%C0")], []))
add("COMPLEX2POLAR", lambda: standalone("COMPLEX2POLAR"))
add("COMPLEX2RECT", lambda: standalone("COMPLEX2RECT"))
add("RECT2COMPLEX", lambda: standalone("RECT2COMPLEX"))
add("POLAR2COMPLEX", lambda: standalone("POLAR2COMPLEX"))

# --- Comparisons ---
for _t in ["CMP_GT", "CMP_LT", "CMP_GE", "CMP_LE", "CMP_EQ", "CMP_NE"]:
    add(_t, lambda t=_t: standalone(t))

# --- Control, protection & conversions ---
add("PID", lambda: standalone("PID", kp=1.0, ki=0.1, kd=0.0, out_min=-100.0, out_max=100.0))
add("OVERCURRENT_51", lambda: standalone("OVERCURRENT_51", pickup=1.0, time_dial=1.0))
add("RECT2POLAR", lambda: standalone("RECT2POLAR"))
add("POLAR2RECT", lambda: standalone("POLAR2RECT"))
add("PHASOR_SHIFT", lambda: standalone("PHASOR_SHIFT", delta_deg=45.0))
add("PHASOR_1P", lambda: standalone("PHASOR_1P", samples_per_cycle=16))
add("PHASOR_3P", lambda: standalone("PHASOR_3P", samples_per_cycle=16))
add("FREQ_EST", lambda: standalone("FREQ_EST", nominal_freq_hz=60.0, hysteresis=0.05,
    min_freq_hz=45.0, max_freq_hz=65.0, filter_alpha=0.0))
add("SYM_COMP", lambda: standalone("SYM_COMP"))
add("DIFF_87", lambda: standalone("DIFF_87", input_count=3, o87p=0.3, slp1=0.25, irs1=1.5, slp2=0.6))
add("PHASE_COMP", lambda: standalone("PHASE_COMP", compensation=6))
add("DIST_21", lambda: standalone("DIST_21", reach=10.0, line_angle=75.0,
    offset=0.0, offset_angle=75.0, prefault_v_threshold=0.5, prefault_v_duration=80))

# --- Serial bus ---
add("I2C", lambda: standalone("I2C", addr=0x40, poll_rate_ms=100))
add("SPI", lambda: standalone("SPI", cs_pin=0))

# --- DSP & filters ---
add("LPF", lambda: standalone("LPF", alpha=0.5))
add("BIQUAD", lambda: standalone("BIQUAD", filter_type="Lowpass", b0=0.067455,
    b1=0.134911, b2=0.067455, a1=-1.142981, a2=0.412802))
add("MOVING_AVG", lambda: standalone("MOVING_AVG", window_size=8))
add("RATE_LIMITER", lambda: standalone("RATE_LIMITER", rising_rate=10.0, falling_rate=10.0))
add("DEADBAND", lambda: standalone("DEADBAND", threshold=0.5, center=0.0))
add("WASHOUT", lambda: standalone("WASHOUT", alpha=0.95))
add("PEAK_DETECTOR", lambda: standalone("PEAK_DETECTOR", decay_rate=0.995))
add("RMS", lambda: standalone("RMS", window_size=16))
add("MEDIAN", lambda: standalone("MEDIAN", window_size=5))
add("DERIVATIVE", lambda: standalone("DERIVATIVE", alpha=0.5, gain=1.0))
add("ZERO_CROSSING", lambda: standalone("ZERO_CROSSING", hysteresis=0.01, sample_rate_hz=1000.0))
add("LUT_1D", lambda: standalone("LUT_1D", x=[0.0, 1.0], y=[0.0, 10.0]))
add("TOTALIZER", lambda: standalone("TOTALIZER", time_base_sec=1.0, scale_factor=1.0,
    sample_time_sec=0.01))
add("MIN_MAX_HOLD", lambda: standalone("MIN_MAX_HOLD", mode=0))

# --- Tags (send+receive pair resolves to a single MOVE) ---
add("TAG", lambda: ([din("IN1"), e("TS1", "TAG_SEND", direction="send", tag_name="T1"),
    e("TR1", "TAG_RECEIVE", direction="receive", tag_name="T1"), dout("OUT1")],
    [net("IN1", "out", "TS1", "in"), net("TR1", "out", "OUT1", "in")]))

# --- Board extensibility (local fallback def; no board profile required) ---
add("LE_CUSTOM", lambda: standalone("LE_CUSTOM", function_id=0x81, output_type="bool"))


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for key, build in SPECS:
        elements, nets = build()
        circ = {"name": "Element_" + key, "elements": elements, "nets": nets}
        path = os.path.join(out_dir, key + ".json")
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(circ, fh, indent=2)
        print("wrote %s" % path)
    print("generated %d fixtures" % len(SPECS))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else OUT)