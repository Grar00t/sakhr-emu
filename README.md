# Sakhr-EMU

Cycle-accurate Sakhr MSX emulator. C + asm. Zero dependencies.

## Scope

- Linux x86-64
- Explicit, trace-verified Sakhr/MSX machine profiles
- Z80A including observable undocumented NMOS behavior
- TMS9918A-class VDP, AY-3-8910-compatible PSG, MSX slot/PPI/keyboard/cartridge bus
- Integer audio synthesis and direct host backends: raw X11 or KMS/DRM, ALSA PCM
- No SDL, libretro, LLVM, or external emulation framework

A compatibility run is not called cycle-accurate unless the relevant device/profile behavior has a passing transaction-level trace suite. `docs/architecture.md` and `docs/timing_model.md` identify profile facts that remain to be verified rather than fabricating them.

## Current state

The committed implementation is a stage-1 freestanding Z80 reset-vector decoder. It reads the first 256 bytes of a supplied image, decodes and prints 20 instructions with byte counts and T-state classes. It is not yet an instruction executor or a runnable emulator.

The decoder covers base, CB, ED, DD/FD and DDCB/FDCB decode forms, including SLL and indexed-CB register-copy disassembly. The executor, flags, ordered M-cycle bus traffic and interrupts are next.

## Build

Requires a C11 compiler and `make`.

```
make
```

The target is `z80dec`. It is built `-static -nostdlib`; the demo uses Linux x86-64 raw syscalls only.

## Run

```
./z80dec path/to/your-image.bin
# or
make run ROM=path/to/your-image.bin
```

The first 20 decoded instructions begin at reset vector `0x0000`. Bytes after offset `0x00FF` read as `0xFF` in this diagnostic target.

Example output shape:

```
0000  F3           di                            T=4
0001  31 00 F0     ld sp,0xF000                  T=10
```

## Layout

```
src/z80.[ch]          CPU state and diagnostic decoder
src/z80_opcodes.h     x/y/z/p/q metadata
src/z80.c             raw-syscall demo entry point under Z80_DECODE_DEMO
docs/architecture.md  device ownership, bus, Z80 execution/test plan
docs/timing_model.md  master clock, scheduler decision, VDP plan
docs/milestones.md    M1..M3 inputs and acceptance tests
```

## License

MIT.
