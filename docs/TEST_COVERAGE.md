# Test Bench Coverage Matrix

This document is the coverage contract for the LogicElements test bench. It maps
every runtime subsystem and compiler feature to the test(s) that exercise it, so
a new feature or regression has a known home. Any opcode marked **RUN** is
executed on the VM (behavior asserted); **ASM** means the assembly-fixture suite
verifies correct opcode emission; **EMIT** means a compiler test proves the
circuit compiles into expected bytecode/state.

## Suites

| Suite | Binary | Assertions (current) | Focus |
| :--- | :--- | :--- | :--- |
| Runtime | `test_c_runtime.c` | 410 | opcode behavior, loader/CRC, comms, storage, CLI, pulses |
| Compiler | `test_compiler.cpp` | 40 tests | JSON -> bytecode pipeline, optimizer, aliases, error paths |
| Assembly | `test_element_assembly.c` | 89 oracle rows | element -> opcode emission parity |

## Opcode coverage

| Opcode family | ASM | Compiler EMIT/RUN | Runtime RUN | Notes |
| :--- | :--- | :--- | :--- | :--- |
| MOVE / NOT / AND / OR | Y | Y | Y | truth tables + invert modifiers |
| XOR / NAND / NOR | Y | Y | Y | full truth tables |
| MUX | Y | Y | Y | scalar out=in_a; N-arity via test_mux_block |
| RTRIG / FTRIG | Y | Y | Y | rising/falling edge incl. re-arm |
| SR / RS | Y | Y | Y | set/reset dominance |
| TON / TOF / TP | Y | Y | Y | bake-in + heap binding |
| CTU / CTD / CTUD | Y | Y | Y | count up/down/dual, preset |
| MOVE_F..MAX_F | Y | Y | Y | div-by-0 guard, NaN, -0 sign |
| SCALE_F | Y | Y | Y | linear remap + clamp |
| CLAMP_F | Y | Y | - | block builtin |
| CADD/CSUB/CMUL/CDIV/MOVE_C | Y | Y | Y | LE_ENABLE_COMPLEX-gated |
| CMP_GT/LT/GE/LE/EQ/NE | Y | Y | Y | 1e-6 EQ, INVERT_OUT |
| PID | Y | Y | - | state bake-in |
| OVERCURRENT | Y | Y | Y | ANSI-51 curve + clear |
| Rect/Polar conversions (6) | Y | Y | Y | |
| PHASOR_1P | Y | Y | Y | DFT golden |
| SYM_COMP | Y | Y | Y | seq 0/1/2 |
| DIST_21 | Y | Y | Y | Mho zone trip + baked reach |
| DIFF_87 | Y | Y | Y | N-input dual-slope |
| PHASE_COMP | Y | Y | Y | 33-transform |
| I2C / SPI | Y | Y | Y | startup code + mock bus |
| EXT_CALL | Y | Y | Y | sim fns 0x81..0x84 |
| BLOCK | Y | Y | Y | variable-arity dispatch |
| All 14 DSP ops | Y | Y | Y | compile + run per filter |

## Deep runtime coverage (expansion)

| Area | Test |
| :--- | :--- |
| CRC32 IEEE 802.3 | test_crc32_known_answers - known-answer 0xCBF43926, NULL/empty, bit-flip |
| Loader negatives | test_loader_negative_paths - magic/version/truncation/OOB (block/state/alias)/CRC/capacity |
| State-desc edges | test_loader_state_desc_edges - NONE/count-0 skip; timer bake-in; OOB index NULL |
| Kind-count hardening | le_rt_state declared-count bounds - OOB index -> NULL |
| Pulse semantics | test_pulse_duration_expiry + test_comms_pulse_command - arm/anchor/expiry, refresh, exhaustion, wire 0x41, const NACK |
| Comms negatives | test_comms_negative_paths - unknown cmd, oversize, OOB chunk, empty PROG_END |
| Edge/latch/compare | test_edge_latch_compare_opcodes + test_gates_mux_compare_opcodes |
| Float edge cases | test_float_edge_cases + test_scale_opcode_with_state |
| Arena access | test_arena_boundary_access + test_arena_unaligned_reject |
| Storage integrity | test_storage_corrupt_slot - corruption detected, siblings isolated |
| CLI surface | test_cli_new_surface - force mnemonics, pulse durations, junk input |

## Compiler coverage (expansion)

| Area | Test |
| :--- | :--- |
| Hard error paths | test_json_error_paths (malformed JSON, non-object root) |
| Lenient-contract locks | test_net_error_paths (dangling/self-loop tolerated + defaults; unknown types emit 0 instr) |
| Alias validation | test_alias_error_paths (name length, unresolvable target, 33-alias table resolves) |
| Determinism | test_binary_determinism (byte-identical repeat compiles) |
| Header format | test_binary_header_fields (40-byte header, all count fields, rsvd==0) |
| Disassembly | test_disasm_content (%IN/%OUT/%F mnemonics + alias table) |
| Optimizer guards | test_dce_preserves_stateful + test_inversion_combinations |
| Full pipeline | test_integrated_pipeline_step (multi-scan DIN->TON->AND->DOUT + float->CMP->DOUT) |
| Capacity contract | test_register_limits_load_reject (compile ok, loader LE_ERR_CAPACITY) |
| Wire end-to-end | test_comms_upload_endtoend (compile -> PROG upload -> run -> GET_IMAGE) |

## How to run

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

## Keeping this matrix honest

- Every element fixture in tests/element_fixtures/ must have a row in the
  g_oracle[] table in tests/test_element_assembly.c (and vice versa).
- New opcodes must land in **two** of the three columns (ASM or compiler RUN,
  plus a runtime behavior check) before this matrix is considered updated.
- The runtime "Results" line and compiler "Summary" line are the source of
  truth for the assertion counts above.