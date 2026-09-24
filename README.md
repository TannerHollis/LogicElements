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
| **Runtime core** | ~10 KB flash, zero-heap, execute-in-place (XIP) |
| **Binary format** | `.lebin` v6 — `LEB1` magic, IEEE 802.3 CRC32 |
| **License** | MIT |

## Quick start

```bash
# Compile a circuit into bytecode (with board validation):
le_compile my_circuit.json -b ports/stm32/stm32f401.leconfig -o my_circuit.lebin

# Or via the Python ctypes SDK:
py tools/compiler/le_compiler.py my_circuit.json -b my_board.leconfig -o my_circuit.lebin

# Inspect a compiled program:
py tools/compiler/le_disasm.py my_circuit.lebin
```

---

## Documentation

All documentation lives in `docs/`.

### Landing / overview

- **[Architecture](docs/ARCHITECTURE.md)** — end-to-end design: the runtime,
  the compiler pipeline, the `.lebin` binary format, the variable-arity block
  mechanism, the zero-heap state workspace, the HAL, and the tooling.
- **[Elements Reference](docs/ELEMENTS.md)** — every element: description,
  usage (ports & properties), and an example disassembly.

### Guides

- **[Platform / HAL integration](docs/PLATFORM_GUIDE.md)** — porting a board,
  the hardware abstraction layer, flash partitioning, and UART program upload.
- **[Compiler & optimizer](docs/COMPILER_GUIDE.md)** — compilation pipeline,
  optimization passes, liveness/register allocation, and CLI/API usage.
- **[Custom nodes](docs/CUSTOM_NODES_GUIDE.md)** — adding board-specific
  hardware blocks (`LE_OP_BLOCK` / `LE_FUNC_CUSTOM_BASE`, `ext_call`).
- **[Supported boards](docs/Ports.md)** — reference board profiles.
- **[Custom-nodes example](docs/CustomNodesExample.md)** — working PWM /
  encoder / filter example.
- **[Element test fixtures](docs/ElementFixtures.md)** — the per-element
  assembly test suite.

### Reference

- **Elements** — [all elements and their disassembly](docs/ELEMENTS.md)
- **Board profiles & UART protocol** — [communications & profiles](docs/COMMUNICATIONS_BOARD_PROFILES.md)
- **Board profiles** — `ports/*.leconfig` schemas (see [ports](docs/Ports.md))
- **Binary format** — [`.lebin` v6](docs/COMPILER_GUIDE.md#binary-bytecode-specification-lebin)
- **Generation & tooling** — `tools/compiler/le_compiler.py`,
  `le_board.py`, `le_disasm.py`; `tools/sim/main.c`

---

## Repository layout

```
CMakeLists.txt               # C/C++ build, library + CLI + tests
build_element_assembly.bat   # Windows per-element assembly test build
docs/                        # all documentation
examples/custom_nodes/       # custom node example (circuit, board, HAL, runner)
ports/                       # board profiles + HAL ports (stm32, rp2040, avr, template)
src/compiler/                # C++17 compiler engine + C API + CLI
src/hal/                     # simulator HAL
src/runtime/                 # ANSI C runtime (VM, process image, opcodes, loader, storage, comms, cli, le_rt)
tests/                       # C runtime tests, compiler tests, per-element assembly tests
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

MIT — see the [LICENSE](LICENSE) and the headers of the core modules.