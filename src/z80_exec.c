#include "z80.h"

#define MASTER_TICKS_PER_TSTATE 3UL

static void advance(Z80 *z, u64 tstates) {
    z->tstates += tstates;
    z->master_ticks += tstates * MASTER_TICKS_PER_TSTATE;
}

static void r_inc(Z80 *z) {
    z->r = (u8)((z->r & 0x80U) | ((z->r + 1U) & 0x7FU));
}

static u8 fetch8(Z80 *z) {
    u8 v;
    advance(z, 4);
    z->bus_kind = Z80_BUS_FETCH;
    v = z->bus.read(z->bus.ctx, z->pc);
    z->pc = (u16)(z->pc + 1U);
    r_inc(z);
    z->bus_kind = Z80_BUS_NONE;
    return v;
}

static u8 read8(Z80 *z) {
    u8 v;
    advance(z, 3);
    z->bus_kind = Z80_BUS_READ;
    v = z->bus.read(z->bus.ctx, z->pc);
    z->pc = (u16)(z->pc + 1U);
    z->bus_kind = Z80_BUS_NONE;
    return v;
}

static u16 read16(Z80 *z) {
    u16 lo = read8(z);
    u16 hi = read8(z);
    return (u16)(lo | (u16)(hi << 8));
}

static void write8(Z80 *z, u16 addr, u8 v) {
    advance(z, 3);
    z->bus_kind = Z80_BUS_WRITE;
    z->bus.write(z->bus.ctx, addr, v);
    z->bus_kind = Z80_BUS_NONE;
}

int z80_step(Z80 *z) {
    u16 op_pc;
    u8 op;

    if (z == 0 || z->bus.read == 0 || z->bus.write == 0) return Z80_STEP_ERR_INPUT;
    if (z->halted) return Z80_STEP_HALTED;

    op_pc = z->pc;
    op = fetch8(z);

    switch (op) {
    case 0xF3: /* DI */
        z->iff1 = 0;
        z->iff2 = 0;
        return Z80_STEP_OK;

    case 0x31: /* LD SP,nn */
        z->sp = read16(z);
        return Z80_STEP_OK;

    case 0x21: /* LD HL,nn */
        z->hl = read16(z);
        return Z80_STEP_OK;

    case 0x36: { /* LD (HL),n */
        u8 v = read8(z);
        write8(z, z->hl, v);
        return Z80_STEP_OK;
    }

    case 0x23: /* INC HL */
        advance(z, 2);
        z->hl = (u16)(z->hl + 1U);
        return Z80_STEP_OK;

    case 0x76: /* HALT */
        z->pc = op_pc;
        z->halted = 1;
        return Z80_STEP_HALTED;

    default:
        return Z80_STEP_ERR_UNSUPPORTED;
    }
}
