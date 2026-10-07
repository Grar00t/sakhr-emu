# Milestones

Each completed milestone has a deterministic executable acceptance target. Timing is counted from emulated state only; wall-clock time and host presentation do not participate in acceptance.

## M1 — Z80 execution and bus ordering

| Field | Definition |
|---|---|
| Input | `tests/m1_ok.rom`: canonical uppercase-hex encoding of reset vector `F3 31 00 F0 21 00 C0 36 4F 23 36 4B 76`; ROM is readable at reset address `0x0000` and RAM is writable at `0xC000..0xFFFF`. |
| Required output | RAM `0xC000..0xC001 == {'O','K'}`; CPU halted at PC `0x000C`; `SP==0xF000`; `HL==0xC001`; `R==7`; elapsed time is exactly `54` Z80 T-states = `162` master ticks. |
| Acceptance test | `make test-m1` builds with `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -ffreestanding -nostdlib`, executes the ROM until HALT, compares final CPU/RAM state, and compares every bus event against `tests/m1_ok.trace`. Success prints exactly `M1 OK` followed by LF and exits 0. |
| Bus trace contract | Transfer timestamp is the completion time in master ticks. M1 contains 15 events: opcode fetches at `12,24,54,84,114,132,162`; immediate reads at `33,42,63,72,93,141`; RAM writes at `102,150`. |
| Implemented opcode boundary | `DI`, `LD SP,nn`, `LD HL,nn`, `LD (HL),n`, `INC HL`, `HALT`. Any other opcode returns `Z80_STEP_ERR_UNSUPPORTED`. |
| Done | No host call occurs inside `z80_step`; all M1 memory traffic crosses `Z80Bus`; the committed ROM and exact trace fixture pass the executable acceptance target. |
| Failure modes covered | Wrong PC increment, wrong immediate endianness, missing RAM write, wrong HALT PC, wrong R increment, wrong T-state/master-tick total, wrong bus order, wrong bus timestamp, corrupted ROM fixture. |

## M2 — VDP solid screen and VBlank IRQ

| Field | Definition |
|---|---|
| Input | `tests/m2_solid.rom`: CPU writes the two-byte VDP control sequence to set Graphics II/display enable, fills the relevant color table with color `0x4E`, enables VDP interrupts, then HALTs. A deterministic runner advances exactly one field. |
| Required output | Completed frame has exactly `256 * 192` color index `0x0E` pixels; a palette-expanded XRGB dump is uniform; status bit 7 asserts once at VBlank; CPU receives exactly one maskable interrupt after IFF timing permits it; status-port read clears the IRQ source. |
| Acceptance test | `make test-m2` writes `out/m2.ppm` and compares the frame digest, VDP trace, and CPU IRQ acknowledge trace to committed fixtures. |
| Done | Graphics II background fetch addressing, control latch, VRAM increment/read-ahead behavior, VBlank timing, register-1 IRQ gate, and status-read clear are trace-tested. Host presentation is not part of acceptance. |
| Expected failures | Control bytes reversed; VRAM address increment at wrong side of a transfer; status read fails to clear IRQ; renderer samples post-frame VRAM; VBlank starts on wrong scan event; a line interrupt is mistakenly generated for TMS9918A. |

## M3 — Sakhr software boot trace

| Field | Definition |
|---|---|
| Input | User-supplied Sakhr cartridge or software image plus an explicitly selected Sakhr machine profile and corresponding user-supplied firmware images. The test manifest records hashes, mapper type, profile ID, reset input state and run length; image bytes are never committed. |
| Required output | BIOS reaches the cartridge entry path; mapper accesses, PPI slot writes, VDP initialization and PSG writes form a stable trace prefix. The program presents its first non-border frame and enters a repeatable input-wait/game-loop state. |
| Acceptance test | `sakhr-emu --profile <id> --cart <path> --record m3.trace --frames 600` produces the manifest state digest and event trace. A second run with `--replay m3.trace` must be byte-identical at every recorded event. |
| Done | Two consecutive 600-frame runs generate identical trace/digest; the first non-border frame digest and CPU PC range match the locally recorded baseline; input replay at fixed ticks causes identical post-input frame digests. |
| Expected failures | Wrong firmware revision; primary/secondary slot routing error; unsupported mapper; keyboard/PPI polarity mismatch; Z80 behavior used by the program is not implemented; VDP raster/IRQ phase drift; nondeterminism from host input, audio pacing or uninitialised state. |

## Exit criteria

M3 is a compatibility gate, not a blanket cycle-accuracy claim. Accuracy is claimed only for behavior covered by passing transaction-level tests.
