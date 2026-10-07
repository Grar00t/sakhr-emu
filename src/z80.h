#ifndef SAKHR_Z80_H
#define SAKHR_Z80_H

typedef unsigned char u8;
typedef signed char i8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef long i64;

enum {
    Z80_BUS_NONE = 0,
    Z80_BUS_FETCH = 1,
    Z80_BUS_READ = 2,
    Z80_BUS_WRITE = 3,
    Z80_BUS_IN = 4,
    Z80_BUS_OUT = 5
};

enum {
    Z80_STEP_OK = 0,
    Z80_STEP_HALTED = 1,
    Z80_STEP_ERR_INPUT = -1,
    Z80_STEP_ERR_UNSUPPORTED = -2
};

/* Bus: every CPU access goes through these callbacks. ctx is opaque. */
typedef struct Z80Bus {
    u8   (*read)(void *ctx, u16 addr);
    void (*write)(void *ctx, u16 addr, u8 v);
    u8   (*in)(void *ctx, u16 port);
    void (*out)(void *ctx, u16 port, u8 v);
    void *ctx;
} Z80Bus;

typedef struct Z80 {
    u16 af, bc, de, hl, af_, bc_, de_, hl_, ix, iy, sp, pc;
    u8  i, r, iff1, iff2, im, halted;
    u8  bus_kind;
    u64 tstates;                    /* Z80 T-states since reset */
    u64 master_ticks;               /* 3 master ticks per Z80 T-state */
    Z80Bus bus;
} Z80;

typedef struct {
    u8   len;                       /* bytes consumed, prefixes included */
    u8   t;                         /* T-states, branch not taken / unconditional */
    u8   t_taken;                   /* T-states, branch taken or repeat; == t otherwise */
    char text[40];
} Z80Insn;

void z80_reset(Z80 *z, const Z80Bus *bus);
/* Execute one instruction. M1 implements only the opcodes exercised by tests/m1_ok.rom. */
int z80_step(Z80 *z);
/* Side-effect-free decode of the instruction at pc via bus.read. */
void z80_decode(const Z80 *z, u16 pc, Z80Insn *out);

#endif
