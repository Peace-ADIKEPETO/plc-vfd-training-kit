# Ladder engine v0.1 - interface spec

The engine is plain C with no HAL. The same two files run on the STM32F103 (PLC) and in the Qt simulator.
Call `ld_scan()` every 10 ms. Timers and counters count scans.

## Bit address space (one space, same numbers as Modbus coil / discrete input addresses)

| Range | Name | Notes |
|---|---|---|
| 0x0000-0x0007 | I0-I7 | physical inputs, set by the firmware before the scan |
| 0x0100-0x0105 | Q0-Q5 | outputs, written by the ladder, copied to relays after the scan |
| 0x0200-0x023F | M0-M63 | internal bits |
| 0x0300-0x030F | T0-T15 done | written by the engine |
| 0x0310-0x031F | C0-C15 done | written by the engine |
| 0x0400-0x0407 | VFD status bits | copied from the VFD STATUS_WORD by the PLC firmware |
| 0x0410-0x0412 | VFD control bits | RUN, REV, RESET: ladder writes them (OTE), firmware copies them to CTRL_WORD |

## Register address space (same numbers as PLC holding registers)

| Range | Name |
|---|---|
| 0x0010-0x001F | D0-D15 general registers |
| 0x0020-0x002F | T0-T15 accumulators (units of 10 ms) |
| 0x0030-0x003F | C0-C15 accumulators |
| 0x0100-0x0112 | mirror of VFD holding registers 0x0000-0x0012 |
| 0x0200-0x0208 | mirror of VFD input registers 0x0000-0x0008 |

The engine does not enforce read-only regions. The PLC firmware layer does (inputs, T/C, VFD status mirrors).

## Program image

Header, 8 bytes, little-endian: `'L' 'D'`, version (1), flags (0), n_instr (u16), CRC-16 (u16, Modbus polynomial) over the instruction bytes.
Then n_instr instructions of 8 bytes: `op u8, elem u8, mode u8, reserved u8, A u16, B u16`.
`elem` is the animation id (0-254) or 0xFF for none. After each instruction the engine stores the rung power in `elem_power[elem]`.
Limits: 512 instructions, 256 element ids, branch depth 4.

## Instructions

| Op | Code | A | B | Behavior |
|---|---|---|---|---|
| RUNG_BEGIN | 0x01 | | | power = 1 |
| XIC / XIO | 0x02 / 0x03 | bit | | power &= bit / !bit |
| BR_OPEN | 0x04 | | | start of parallel branches |
| BR_NEXT | 0x05 | | | next parallel branch |
| BR_CLOSE | 0x06 | | | end: power = OR of the branches |
| OTE | 0x10 | bit | | bit = power |
| TON | 0x11 | timer 0-15 | preset | counts while power, resets when power drops |
| TOF | 0x12 | timer 0-15 | preset | done = 1 while power, drops preset scans after power falls |
| TONR | 0x13 | timer 0-15 | preset | like TON but holds its value, only RES clears it |
| CTU | 0x14 | counter 0-15 | preset | counts rising edges of the rung |
| RES | 0x15 | index | | mode 0 = timer, mode 1 = counter; clears when power |
| GRT / LES / EQU | 0x20 / 0x21 / 0x22 | register or const | register or const | signed 16-bit compare; mode bit0 = A is constant, bit1 = B is constant |
| MOV | 0x30 | source (register or const) | destination register | copies when power; mode bit0 = A is constant |

Timers and counters leave the rung power unchanged. Use their done bit as a contact in a later rung.

## Editor rules (for the Qt compiler)

Series elements and nested parallel branches only (depth up to 4). No bridge circuits.
Compile each rung as: RUNG_BEGIN, then elements left to right, with a branch as BR_OPEN, first path, BR_NEXT, second path, ..., BR_CLOSE.

## Build and run the tests

Command line: `gcc -Wall -Wextra -std=c11 -o test_engine test_engine.c ladder_engine.c && ./test_engine`
Qt: open `tests.pro`, build and run. Any other qmake project includes the engine with `include(path/to/engine.pri)`.
Expected output: `57 checks, 0 failures`.
