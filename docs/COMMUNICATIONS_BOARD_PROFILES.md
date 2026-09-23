# Communications & Board Profiles

Two integration surfaces of LogicElements, moved from the original README:
the framed UART binary protocol (telemetry + program upload), and the board
profile system (`.leconfig`) that enforces hardware bounds.

---

## Framed binary UI packet protocol

For programmatic UI integration (desktop circuit builders, SCADA systems),
LogicElements provides a low-overhead binary framing protocol:

```
[0xAA (SYNC)] [CMD: 1B] [SEQ: 1B] [LEN: 2B] [PAYLOAD: LEN bytes] [CRC16: 2B]
```

| Command | Name | Description |
| :--- | :--- | :--- |
| `0x01` | `LE_CMD_PING` | Ping device alive check |
| `0x02` | `LE_CMD_GET_CAPS` | Query board firmware, memory limits, feature flags |
| `0x82` | `LE_CMD_CAPS_DATA` | Microcontroller capabilities payload response |
| `0x03` | `LE_CMD_GET_CUSTOM_NODES` | Query board custom node JSON definitions |
| `0x83` | `LE_CMD_CUSTOM_NODES_DATA` | Custom node JSON definitions payload response |
| `0x10` | `LE_CMD_PROG_BEGIN` | Announces incoming `.lebin` upload size; resets staging cursor |
| `0x11` | `LE_CMD_PROG_CHUNK` | Sends 32–128 byte chunk of binary program |
| `0x12` | `LE_CMD_PROG_END` | Verifies CRC32, commits to flash/EEPROM, reloads VM |
| `0x20` | `LE_CMD_GET_IMAGE` | Requests snapshot of `%I`, `%Q`, `%M` for live UI watch window |
| `0x30` | `LE_CMD_CONTROL` | START (1), STOP (2), RESET (3) execution |
| `0x40` | `LE_CMD_FORCE_IO` | Force digital input/output high/low for testing |

Program upload (`LE_CMD_PROG_*`) streams `.lebin` payload in bounded chunks;
the loader verifies the IEEE 802.3 CRC32 and header integrity before
committing to flash.

---

## Board discovery & offline profiles

There are two ways to target a board:

1. **Live discovery** — connect over UART/serial to probe the board for its
   firmware version, platform name, memory limits, and enabled feature flags.
2. **Offline `.leconfig` profiles** — import a board profile to enforce
   hardware bounds and resolve symbolic pin aliases (e.g. `USER_BUTTON` →
   `%I0`, `USER_LED` → `%Q0`) without physical hardware.

### Inspecting and generating board profiles (`le_board.py`)

```bash
# Display formatted capability report from a board profile:
py tools/compiler/le_board.py --info ports/stm32/stm32f401.leconfig

# Query live connected hardware over serial and save its profile:
py tools/compiler/le_board.py --port COM3 --baud 115200 --output my_board.leconfig

# Generate a starter template for custom hardware adopters:
py tools/compiler/le_board.py --template --output custom_board.leconfig
```

### Compiling with board validation and pin aliases

Pass `-b` / `--board` to validate the circuit against the board's physical
limits and resolve symbolic pin names:

```bash
./build/Release/le_compile my_circuit.json -b ports/stm32/stm32f401.leconfig -o program.lebin
py tools/compiler/le_compiler.py my_circuit.json -b ports/stm32/stm32f401.leconfig -o program.lebin
```

If the circuit exceeds physical constraints or uses disabled features, the
compiler rejects it with a descriptive error:

```
Board Validation Error: Circuit uses Protection & Control opcode 0x77, but target board
'Arduino Uno (ATmega328P)' has protection feature disabled.
```

### Pre-configured reference board profiles

| Profile | Platform | Flash Slots | %I / %Q | %M / %R | Features |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `ports/stm32/stm32f401.leconfig` | STM32F401RE (Nucleo) | 3 × 2048B | 16 / 16 | 128 / 64 | Protection & Control, I2C, SPI, DSP (16) |
| `ports/rp2040/rp2040_pico.leconfig` | Raspberry Pi Pico | 4 × 4096B | 26 / 26 | 256 / 128 | Protection & Control, I2C, SPI, DSP (16) |
| `ports/avr/atmega328p.leconfig` | Arduino Uno (ATmega328P) | 1 × 512B | 6 / 6 | 32 / 16 | Standard Logic, DSP (4) |