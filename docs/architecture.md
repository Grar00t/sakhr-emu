# Sakhr-EMU architecture

## Target contract

A machine profile is an explicit data object: CPU clock, VDP variant and region timing, BIOS/sub-ROM identity, PPI wiring, PSG wiring, slot map, cartridge mapper type, and keyboard matrix. `sakhr-emu --machine ax-170` is not a generic MSX profile with an Arabic font injected. Hardware details that cannot be verified from a board dump, ROM, service material, or trace remain unimplemented rather than guessed.

The repository starts with an MSX1-class profile: Z80A, a TMS9918A-family VDP, AY-3-8910-compatible PSG, 64 KiB RAM and MSX primary slots. The AX-170 hardware listing describes it as an Arabic MSX1 with 64 KiB RAM and built-in Arabic ROM/software; that is the first proposed Sakhr profile, but board-specific port wiring must be verified before claiming compatibility. [web:7]

## Source layout

```
src/
  main.c          process setup, machine profile selection, event loop, recording/replay
  machine.[ch]    owns all device state; master tick, reset, deterministic snapshot
  bus.[ch]        64 KiB address decode, slot/subslot dispatch, open-bus policy, I/O dispatch
  z80.[ch]        registers, instruction executor, interrupt acknowledge, per-M-cycle callbacks
  z80_opcodes.h   generated-from-spec opcode metadata: timing, handler, prefix behavior
  vdp.[ch]        VRAM/control ports, registers/status, scan timing, sprite evaluation, IRQ
  psg.[ch]        AY registers, integer phase counters, envelope/noise LFSR, fixed-point mixer
  ppi.[ch]        8255 port A/B/C: slot select, keyboard row select/read, cassette/motor bits
  cart.[ch]       cartridge image and mapper write decoders; no ROM policy beyond user-supplied file
  input.[ch]      host key state -> 11x8 MSX matrix and two joystick ports
  video_x11.c     raw X11 backend; indexed 256x192 frame upload and host presentation timing
  video_kms.c     KMS/DRM backend; dumb buffer/page-flip path
  audio_alsa.c    ALSA PCM directly; integer ring buffer, fixed-rate resampler
  trace.[ch]      binary event log: CPU bus cycles, port writes, VDP state transitions, hashes
  test/           device unit tests and trace-replay integration tests
```

## Ownership and separation

`Machine` owns device instances and master time. CPU owns registers and emits bus transactions only; it never knows what address is RAM, VDP, PPI, or cartridge. Bus owns decode and forwards each transaction to the selected device. VDP owns VRAM, status flags, scan position, its IRQ level and the completed framebuffer. PSG owns only audio state and appends signed integer samples to a machine-owned ring. Host backends consume already-completed frames/samples; they never advance emulated time.

`z80_step()` must execute one instruction as an ordered sequence of machine cycles. Every memory or port transfer calls `machine_advance(delta_master_ticks)` before completion. This is required because a VDP status read, data-port write, IRQ transition and CPU interrupt sample can occur inside an instruction window. `z80_decode()` currently committed in `src/z80.c` is diagnostic only; it is not the execution engine.

## Bus and memory map

The Z80 sees four 16 KiB pages. PPI port A (`0xA8`) selects one primary slot per page; secondary slot expansion is enabled per primary-slot page via the conventional `0xFFFF` register when that slot is expanded. RAM mapper registers (`0xFC`..`0xFF`) select physical RAM banks only when mapper RAM is visible. Cartridge mapper writes are decoded by cartridge type and must occur after slot routing, not as global address traps. The MSX I/O overview identifies port `0xA8` as PPI register A and describes PPI responsibility for slot selection; mapper selection is reported through ports `0xFC`–`0xFF`. [web:8][web:4]

## Rendering and sound

The VDP renders 256x192 indexed pixels into a completed frame at scanline boundaries. A palette lookup converts 4-bit TMS colors to host XRGB8888 only after the VDP finishes the line; VDP state is never reconstructed from host pixels. The first TMS9918A target is Mode 2, then modes 0/1/3; sprite evaluation uses the original 32-entry attribute table, 4-per-line overflow rule and lowest-index collision behavior.

PSG synthesis is integer only. Tone generators are 12-bit periods; noise is a 17-bit LFSR; envelope is a 16-shape finite-state counter. Output mixing uses signed fixed-point accumulators. Generate samples from master-tick timestamps, then use a rational integer phase accumulator to produce the selected PCM rate. The host ALSA writer only drains a ring and is outside the emulated clock.

## Z80 plan

### Opcode construction

Option A: handwritten 256-entry tables for unprefixed, CB, ED, DD/FD and DDCB/FDCB spaces. Tradeoff: direct dispatch, but duplicated rules and mismatched timings become likely.

Option B: a compact declarative opcode specification expanded by a checked-in C generator into handler/timing tables. Tradeoff: generator work exists before the executor, but every opcode family shares one source of truth.

**Pick: option B.** Keep `tools/mkopcodes.c` in-tree and compile it with the host C compiler; its output (`src/z80_generated.h`) is committed. The generated table records handler ID, length class, base T states, alternate taken/repeat T states, R-register increment count, illegal-prefix behavior and flag recipe. `z80_opcodes.h` is currently a readable metadata seed, not the final generated table. The generator is not an emulation dependency.

Decode follows the documented x/y/z/p/q decomposition for base and CB spaces. Prefix state is explicit: repeated DD/FD prefixes each consume 4T, the last prefix determines IX/IY substitution, `DD/FD ED` consumes prefixes then executes ED without index substitution, and DDCB/FDCB fetches displacement before the CB operation byte. `SLL` and indexed-CB register-copy results are executed, not mapped to NOP.

### Per-opcode tests

For each primary opcode and prefix space, generate a test case containing initial full CPU state, memory/ports, expected final full state, ordered bus-cycle log, expected R increment and T-state count. Test vectors are grouped by: base 256; CB 256; ED 256 including aliases/NOPs; DD and FD substitution matrix; DDCB and FDCB 256x256 destination combinations; interrupt boundaries; HALT; IM0/1/2; reset.

The oracle hierarchy is: physical trace if available; otherwise independent test ROM trace; otherwise a manually reviewed instruction table. Store expected traces as binary fixtures with a versioned format, not text listings. `tracecmp` compares transaction address, direction, data, T-state start/end and IRQ/NMI level. A register-only test is insufficient because it misses ordering failures.

### Undocumented policy

Option A: reject undocumented bytes and stop. Tradeoff: smaller core, fails software that uses legal silicon behavior.

Option B: model observable NMOS Z80 behavior, including aliases, `SLL`, IXH/IYH/IXL/IYL operations, DD/FD CB register-copy forms, repeated prefixes, ED alias behavior, and undocumented flags where verified. Tradeoff: larger test matrix and variant sensitivity.

**Pick: option B.** The target is original software, and the undocumented instructions are part of the observable hardware contract. The initial profile is NMOS Z80A behavior. Open semantics that differ among Zilog, NEC and CMOS variants are represented as profile flags; no implementation is labeled cycle-accurate until a test vector or trace pins that flag.
