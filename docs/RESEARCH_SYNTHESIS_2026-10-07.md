# Research Synthesis — 2026-10-07

## Papers read for this repository

- **nCPU: Self-Hosting C Compiler on Metal GPU** — evaluated only for its deterministic execution claim. Its mechanism is a Metal-backed ARM64 virtual CPU running compiler-produced binaries, not an MSX/Z80 timing model.
- The remaining supplied papers target content addressing, distributed capabilities, microkernel IPC, datacenter failure detection, cryptographic implementation, or compiler bootstrap; none supplies a mechanism for Z80 instruction timing, MSX bus contention, VDP timing, PSG timing, or emulator conformance.

## Changes committed

- None. No supplied research mechanism maps to a demonstrated defect or missing emulator capability without inventing an unrelated subsystem.

## Papers read but rejected

- **nCPU** — rejected: deterministic cycle variance of its GPU virtual CPU is not an implementable method for obtaining cycle-accurate Z80/MSX behavior.
- **Morello-Cerise; Cloud-Scale Distributed Capabilities; watchpoint IPC; HSEB; FiDe; uKharon; IPFS-DID; Content-Addressing 2025; constant-time side-channel paper; SHA256 accelerator; Onramp** — rejected: no direct applicability to the current MSX/Z80 emulator core.

## Papers requiring operator decision

- None.
