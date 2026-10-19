# LogicElements

**A zero-heap, deterministic runtime and compiler for turning microcontrollers
into industrial programmable logic controllers (PLCs).**

LogicElements compiles graphical/JSON circuit schematics into a compact
`.lebin` bytecode format that an ANSI C runtime executes on-field, with a
variable-arity block mechanism for multi-input/multi-output elements. It is
written for flash-constrained MCUs: **no malloc at boot**, a static
zero-heap state workspace, and UART/telemetry support.

| | |
| :--- | :--- |
| **Language** | ANSI C99 runtime · C++17 compiler |
| **Runtime core** | [~6.5 KB – 42 KB Flash (modularly scalable)](docs/ARCHITECTURE.md#runtime-vm-size-and-feature-breakdown), zero-heap, execute-in-place (XIP) |
| **Binary format** | `.lebin` v13 — `LEB2` chunked container, IEEE 802.3 CRC32 |
| **License** | MIT |

## Quick start

```bash
# Compile a circuit into bytecode (with board validation):
le compile my_circuit.json -b ports/stm32/stm32f401.leconfig -o my_circuit.lebin

# Inspect a compiled program:
le disasm my_circuit.lebin

# Flash program to hardware and start PLC execution:
le upload my_circuit.lebin --port COM3 --run

# Stream live process image telemetry:
le monitor --port COM3

# Run local desktop simulation:
le sim my_circuit.lebin
```

---

## Documentation

All documentation lives in `docs/`.

### Landing / overview

- **[Architecture](docs/ARCHITECTURE.md)** — end-to-end design: the runtime,
  the compiler pipeline, the `.lebin` binary format, the variable-arity block
  mechanism, the zero-heap state workspace, the HAL, and the tooling.
- **[Runtime VM Size & Feature Breakdown](docs/ARCHITECTURE.md#runtime-vm-size-and-feature-breakdown)** — granular
  Flash ROM and static RAM cost per subsystem feature and deployment tier.
- **[Elements Reference](docs/ELEMENTS.md)** — every element: description,
  usage (ports & properties), and an example disassembly.

### Guides

- **[Platform / HAL integration](docs/PLATFORM_GUIDE.md)** — porting a board,
  the hardware abstraction layer, flash partitioning, and UART program upload.
- **[Compiler & optimizer](docs/COMPILER_GUIDE.md)** — compilation pipeline,
  optimization passes, liveness/register allocation, and CLI/API usage.
- **[Custom nodes](docs/CUSTOM_NODES_GUIDE.md)** — adding board-specific
  hardware blocks (`LE_OP_BLOCK` / `LE_FUNC_CUSTOM_BASE`, `ext_call`).
- **[Custom Board Implementer's Guide](docs/PORTS.md)** — porting to custom
  microcontrollers, hardware HAL, NVRAM, flash wear leveling, and board profiles.
- **[Custom-nodes example](docs/CUSTOM_NODES_EXAMPLE.md)** — working PWM /
  encoder / filter example.
- **[Element test fixtures](docs/ELEMENT_FIXTURES.md)** — the per-element
  assembly test suite.

### Reference

- **Elements** — [all elements and their disassembly](docs/ELEMENTS.md)
- **Board profiles & UART protocol** — [communications & profiles](docs/COMMUNICATIONS_BOARD_PROFILES.md)
- **Board profiles** — `ports/*.leconfig` schemas (see [ports](docs/PORTS.md))
- **Binary format** — [`.lebin` v13 (LEB2)](docs/COMPILER_GUIDE.md#binary-bytecode-specification-lebin)
- **Generation & tooling** — `tools/compiler/le_compiler.py`,
  `le_board.py`, `le_disasm.py`; `tools/sim/main.c`

---

## Repository layout

```
CMakeLists.txt               # C/C++ build, library + CLI + tests
build_element_assembly.bat   # Windows per-element assembly test build
docs/                        # all documentation
examples/custom_nodes/       # custom node example (circuit, board, HAL, runner)
ports/                       # board profiles + reference HAL ports (template, avr, rp2040, stm32)
src/cli/                     # Unified host CLI toolsuite (`le`) & portable comms engine (`le_host_comms`)
src/compiler/                # C++17 compiler engine + C API
src/hal/                     # simulator HAL
src/runtime/                 # ANSI C runtime (VM, process image, opcodes, loader, storage, comms, cli, ringbuf, le_rt)
tests/                       # C runtime tests, host comms tests, compiler tests, per-element assembly tests
tools/                       # Python compiler/board/disasm SDK + simulator main
```

## Dataflow at a glance

```
Circuit JSON + board profile
   │  le_compile / le_compiler.py
   ▼
.lebin  ──UART/upload──►  loader  ──►  VM step loop  ──►  process image  ──►  HAL  ──►  physical I/O
                          (state blocks baked into a preconfigured state image, copied into the zero-heap workspace at load)
```

## License

MIT — see the headers of the core modules.