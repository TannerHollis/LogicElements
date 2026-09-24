# Per-element compiler assembly fixtures

Each `<SPEC>.json` in this directory is a **minimal, canonical LogicElements
circuit** whose nominal element is derived from the filename. The test suite
`../test_element_assembly.c` compiles every fixture through the C compiler API
at `-O0` (the 1:1 schematic-to-bytecode mapping), parses the emitted
disassembly, and asserts that the *exactly expected* opcode mnemonic appears —
and that no-op elements emit zero instructions.

These fixtures are the regression oracle for the compiler's
element->bytecode mapping. If a new element type is added to the runtime, add
its fixture here **and** an entry to the `g_oracle[]` table in
`test_element_assembly.c`, or the coverage gate will report it.

## Regenerating

The fixtures are generated (not hand-edited) so they stay consistent:

    py tests/element_fixtures/generate_fixtures.py tests/element_fixtures

Edit `generate_fixtures.py`, then re-run. The generator deliberately keeps
functional elements **standalone** (no I/O nets) so each compiles to exactly one
instruction; registers and I/O pairs that need a source/sink to emit anything
are wired explicitly.

## Naming convention

| Filename | Circuit |
| :--- | :--- |
| `DIGITALINPUT`, `ANALOGINPUT_plain`, `CONSTANT` | no-op (assert 0 instructions) |
| `ANALOGINPUT_scaled` | scaled analog -> `SCALE_F` |
| `DIGITALOUTPUT`, `BOOLREGISTER`, `INTREGISTER` | driven -> `MOVE` |
| `FLOATREGISTER` | float-driven -> `MOVE_F` |
| `AND`..`MIN_MAX_HOLD` | standalone functional element -> its opcode once |
| `CLAMP`, `RECT2POLAR`, `POLAR2RECT`, `PHASOR_SHIFT` | conversions/math -> opcode once |
| `LATCH_set` / `LATCH_reset` | LATCH dominant=Set/Reset -> `SR` / `RS` |
| `TAG` | full send+receive pair -> single `MOVE` |
| `LE_CUSTOM` | local-fallback custom node (func_id>=0x80) -> `BLOCK` |

## Running

    # Windows (mirrors build_eachopcode_build.bat):
    build_element_assembly.bat
    build_element_assembly\test_element_assembly.exe tests\element_fixtures

    # Via CTest (CMake):
    ctest --test-dir build -R ElementAssembly

