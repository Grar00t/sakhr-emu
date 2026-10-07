# Milestones

All acceptance tests run with pacing disabled and a fixed machine profile. Each test emits a binary trace and a state digest over CPU registers, relevant RAM/VRAM, device registers and master tick. The digest excludes host framebuffer handles, ALSA state and wall-clock data.

## M1 — Z80 execution and bus ordering

| Field | Definition |
|---|---|
| Input | `tests/m1_ok.rom`: reset vector `F3 31 00 F0 21 00 C0 36 4F 23 36 4B 76`; initial map has this ROM in page 0 and writable RAM at `0xC000`. |
| Required output | RAM `0xC000..0xC001 == {'O','K'}`; CPU halted at PC `0x000C`; `SP==0xF000`; global time is the exact sum of executed M-cycles in master ticks. |
| Acceptance test | `make test-m1` runs until HALT, compares final state and complete bus log against `tests/m1_ok.trace`: fetches, immediate reads and two RAM writes in order, with exact address/data/direction/timestamp. Then run the generated opcode suite, which checks final state, R increments, flags, T-states and ordered transaction traces across base/CB/ED/DD/FD/DDCB/FDCB spaces. |
| Done | No host calls occur during `z80_step`; all memory/I/O traffic crosses `Bus`; M1 trace digest remains identical under `--no-video --no-audio` and normal backend builds. |
| Expected failures | PC increment wrong for multi-byte instructions; `HALT` fetch behavior wrong; write timestamp placed at instruction end instead of its M-cycle; flags/R not updated; `EI` delay or INT acknowledgement contaminates an otherwise passing test. |

## M2 — VDP solid screen and VBlank IRQ

| Field | Definition |
|---|---|
| Input | `tests/m2_solid.rom`: CPU writes the two-byte VDP control sequence to set Graphics II/display enable, fills the relevant color table with color `0x4E`, enables VDP interrupts, then HALTs. A deterministic runner advances exactly one field. |
| Required output | Completed frame has exactly `256 * 192` color index `0x0E` pixels; a palette-expanded XRGB dump is uniform; status bit 7 asserts once at VBlank; CPU receives exactly one maskable interrupt after IFF timing permits it; status-port read clears the IRQ source. |
| Acceptance test | `make test-m2` writes `out/m2.ppm` and compares the frame's SHA-256-equivalent in-tree integer digest, VDP trace, and CPU IRQ acknowledge trace to fixtures. The test is repeated with the VDP register writes shifted by one CPU T-state around active display boundaries; only the specified affected line may change. |
| Done | Graphics II background fetch addressing, control latch, VRAM increment/read-ahead behavior, VBlank timing, register-1 IRQ gate, and status-read clear are trace-tested. Host presentation is not part of acceptance. |
| Expected failures | Control bytes reversed; VRAM address increment at wrong side of a transfer; status read fails to clear IRQ; renderer samples post-frame VRAM; VBlank starts on wrong scan event; a line interrupt is mistakenly generated for TMS9918A. |

## M3 — Sakhr software boot trace

| Field | Definition |
|---|---|
| Input | User-supplied, legally held Sakhr cartridge or software image plus an explicitly selected verified Sakhr machine profile and corresponding user-supplied firmware images. The test manifest records SHA-256 values, mapper type, profile ID, reset input state and run length; image bytes are never committed. |
| Required output | BIOS reaches the cartridge entry path; mapper accesses, PPI slot writes, VDP initialization and PSG writes form a stable trace prefix. The program presents its first non-border frame and enters a repeatable input-wait/game-loop state. |
| Acceptance test | `sakhr-emu --profile <id> --cart <path> --record m3.trace --frames 600` produces the manifest state digest and event trace. A second run with `--replay m3.trace` is byte-identical at every event. A regression fixture stores hashes and expected event windows, not image content. |
| Done | Two consecutive 600-frame runs generate identical trace/digest; the first non-border frame digest and CPU PC range match the locally recorded baseline; input replay at fixed ticks causes identical post-input frame digests. |
| Expected failures | Wrong Sakhr BIOS/sub-ROM revision; primary/secondary slot routing error; unsupported mapper; keyboard/PPI polarity mismatch; Z80 undocumented flags used by code; VDP raster/IRQ phase drift; nondeterminism from host input, audio pacing or uninitialised state. |

## Exit criteria

M3 is **not** evidence that the emulator is cycle-accurate. It is a compatibility gate. The accuracy claim is made only per verified device/profile behavior with a passing transaction-level trace suite. The initial M3 compatibility record names exact image hashes locally; it does not publish image material.
