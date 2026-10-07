# Timing model

## Clock domain

The master tick is the VDP crystal tick: 10,738,635 Hz for the NTSC-class profile. This gives exact integer conversion for the primary devices:

| Device | Rate | Master ticks | Notes |
|---|---:|---:|---|
| VDP dot | 10,738,635 Hz | 1 | TMS9918A runs at 5.37 MHz internally, two crystal ticks per internal VDP cycle |
| Z80 T-state | 3,579,545 Hz | 3 | 1/3 of the master crystal |
| PSG clock | 1,789,772.5 Hz | 6 | half the CPU clock; represented as a phase divider, not a floating value |
| VDP scan line | 63.556 µs | 684 | 342 internal VDP cycles / 228 3.58-MHz CPU cycles |
| NTSC frame | 59.9227 Hz | 179,550 | 262.5 lines; field parity is explicit |

TMS9918 timing evidence reports a 5.37 MHz VDP clock and 342 cycles per line; MSX timing discussion reports a VDP clock six times the Z80 clock and 1368/6 = 228 CPU-clock cycles per line. [web:3][web:6] The 525-line TMS9918A family signal is distinct from the 625-line TMS9929A PAL signal, so PAL is a separate machine profile, not a frame-rate toggle. [web:1]

The initial `ax-170-ntsc` profile uses the table above provisionally. A Sakhr motherboard/oscillator inspection can replace it with a measured profile. Clock constants live only in `MachineProfile`; no device has hard-coded frame duration.

## Options

| Model | Mechanism | Cost | Failure surface |
|---|---|---|---|
| Cycle-stepped | Increment all devices once per master tick; CPU advances after every 3 ticks | At 10.738635 million loop iterations/s before CPU, scan/video/audio work; 644 million iterations/minute even when CPU is HALTed | Lowest scheduling complexity; high host overhead and easy to accidentally make a device do full work every tick |
| Event-driven | CPU emits bus/M-cycle boundaries; scheduler advances each device directly to the next event timestamp | Roughly instruction count plus VDP line events: a 3.58 MHz Z80 averages about 0.6–0.9 million instructions/s for 4–6T mixes; about 15,734 NTSC scanline events/s; PSG sample events at selected PCM rate | Requires exact event ordering and catch-up before every externally observable bus operation |

**Pick: event-driven scheduler with M-cycle-granular bus boundaries and lazy device catch-up.** It preserves external timing while avoiding a 10.7 MHz global tick loop. `machine_advance(to)` processes all events with timestamp `<= to` in a total order: VDP internal transition, VDP IRQ line transition, PSG divider edge/sample, PPI external input latch, CPU bus completion. Before a CPU read/write/in/out, the executor advances to the transfer's completion timestamp, then invokes the bus callback. The next instruction cannot observe stale VDP status or an IRQ that should already be asserted.

Concrete CPU instruction accounting remains exact. For `LD A,(IX+d)` the executor schedules the documented 19 T states = 57 master ticks and the memory read in its correct M-cycle window. For `LDIR`, each repeated iteration is a 21-T execution unit = 63 master ticks; the final iteration is 16T = 48 ticks. The decoder currently prints those timing classes but does not execute them.

## Event order and IRQ sampling

A device event stores `(master_tick, priority, serial)`. Priority breaks simultaneous events: (1) VDP internal/state changes, (2) IRQ/NMI line update, (3) PSG state/sample, (4) external input latch, (5) CPU bus completion. `serial` makes equal-priority order deterministic. The Z80 samples maskable INT at the documented instruction boundary; NMI is latched at its defined edge. IFF enable after `EI` is delayed one following instruction.

Never derive emulated time from `clock_gettime`, host audio clock, X11 present, or DRM page flips. Those are pacing only. The emulator may run ahead into bounded video/audio queues then sleep; its trace must be identical with pacing disabled.

## VDP plan

### Scan timing and framebuffer

VDP state carries `master_tick`, field parity, line number, dot/internal-cycle position, registers, status, control latch, read-ahead buffer, VRAM address and 16 KiB VRAM. On catch-up it advances scan events, not host pixels. Every visible line writes exactly 256 color indices into `frame[line][256]`; border/sync timing still advances internal state even where pixels are not stored.

Option A: compose a complete 256x192 image at VBlank from VRAM. Tradeoff: compact, but mid-frame register/VRAM changes appear on the wrong lines.

Option B: rasterize a line when its active-display fetch phase begins, using register and VRAM state at that timestamp. Tradeoff: needs explicit scan timing, but preserves raster effects.

**Pick: option B.** Games can write VDP registers and VRAM during active display. A VBlank-only renderer cannot be cycle-accurate even if its final static image looks correct.

### Modes and sprite pipeline

Implement Graphics II first because MSX1 software commonly uses its 256x192, 32x24 tile organization. Per visible line: derive the name-table row, pattern/color addresses from R2/R3/R4 and mode bits, fetch 32 background cells into the color-index line, then evaluate sprites in ascending SAT index. Modes 0/1/3 use the same line contract with mode-specific addressing.

Sprites: scan all 32 SAT entries in index order until the Y=208 terminator; select at most four whose vertical range contains the current line; fifth qualifying sprite sets status bit 6 and its index encoding as specified by the chosen VDP variant. Render selected sprites in reverse selected order only when compositing so the lower index wins; color 0 is transparent; overlap of two nontransparent sprite pixels sets collision bit 5. Magnification and 8x16 mode change source-coordinate selection before pattern-bit lookup. Status read returns status then clears the VBlank flag and IRQ source at the port-defined completion point.

### Interrupts and ports

Port decode owns the two VDP ports: data and control/status. Control is a two-byte latch: first write is low address/data, second selects register-write or VRAM read/write address operation. VRAM reads return the prior read-ahead byte then refill; writes update VRAM at the current address and advance it. The exact status-read, control-latch reset and read-ahead ordering is covered by bus traces.

TMS9918A has a VBlank interrupt. It does **not** have a line interrupt; line interrupts are a later V9938-class feature. Therefore the TMS9918A machine profile has one VDP IRQ source: VBlank/status bit 7 gated by register 1 interrupt enable. A V9938 profile later adds line-counter IRQ as a separate event and must not be merged into the TMS9918A implementation. [web:1][web:3]
