# Sakhr-EMU

Sakhr/MSX emulation core in C and x86-64 assembly with no external runtime libraries.

## Scope

- Linux x86-64 host for the current bring-up binaries
- Explicit Sakhr/MSX machine profiles
- Z80 execution with ordered bus transactions
- TMS9918A-class VDP, AY-3-8910-compatible PSG, MSX slot/PPI/keyboard/cartridge bus as later milestones
- No SDL, libretro, LLVM, or external emulation framework

## Current state

M1 is executable. `z80_step()` runs the exact reset-vector program specified in `docs/milestones.md`, performs every memory access through `Z80Bus`, records transfer completion time in master ticks, writes `O` and `K` to RAM at `0xC000` and `0xC001`, and enters HALT with PC fixed at `0x000C` by the project contract.

The M1 executor deliberately implements only the instructions exercised by the committed M1 ROM: `DI`, `LD SP,nn`, `LD HL,nn`, `LD (HL),n`, `INC HL`, and `HALT`. Any other opcode returns `Z80_STEP_ERR_UNSUPPORTED`; no unsupported opcode is treated as a NOP.

The existing `z80_decode()` diagnostic decoder remains available separately. M1 does not claim a complete Z80 implementation, MSX compatibility, or cycle accuracy for untested instructions/devices.

## M1 build and verification

Requires a C11 compiler, GNU-compatible x86-64 assembler syntax accepted by that compiler, and `make`. No libc is linked.

```sh
make clean
make test-m1
```

Expected stdout from the executed test binary:

```text
M1 OK
```

The test fails unless all of these are exact:

- decoded ROM bytes: `F3 31 00 F0 21 00 C0 36 4F 23 36 4B 76`
- RAM: `0xC000=0x4F`, `0xC001=0x4B`
- CPU: `PC=0x000C`, `SP=0xF000`, `HL=0xC001`, `R=7`, halted
- elapsed time: `54` Z80 T-states = `162` master ticks
- ordered bus trace: every fetch, immediate read, and RAM write matches `tests/m1_ok.trace`

`make run-m1` runs the same freestanding artifact directly. The binary uses Linux x86-64 raw syscalls only for loading the ROM fixture, printing the final result, and exiting; `z80_step()` itself performs no host call.

## Existing decoder

```sh
make z80dec
./z80dec path/to/your-image.bin
```

`z80dec` reads the first 256 bytes of an image and prints decoded instructions with timing classes. It is diagnostic code and is not used to satisfy M1 execution.

## Layout

```text
src/z80.h              CPU state, bus contract, executor result codes
src/z80_exec.c         bounded M1 instruction executor and timing
src/z80.c              diagnostic decoder and raw-syscall decode demo
src/z80_opcodes.h      decoder metadata
tests/m1_ok.rom        exact M1 reset-vector program
tests/m1_ok.trace      exact expected ordered/timestamped bus trace
tests/z80_m1_test.c    freestanding M1 acceptance binary
docs/milestones.md     executable milestone contracts
docs/architecture.md   device ownership and future machine structure
docs/timing_model.md   master-clock and scheduler design
```

## License

MIT.
