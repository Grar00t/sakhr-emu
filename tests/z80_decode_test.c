#include "../src/z80.h"

static u8 mem[16];
static u8 rd(void *ctx, u16 a) { (void)ctx; return a < sizeof mem ? mem[a] : 0xff; }
static void wr(void *ctx, u16 a, u8 v) { (void)ctx; (void)a; (void)v; }
static u8 inp(void *ctx, u16 p) { (void)ctx; (void)p; return 0xff; }
static void outp(void *ctx, u16 p, u8 v) { (void)ctx; (void)p; (void)v; }

static int same(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static void load(const u8 *p, unsigned n) { unsigned i; for (i=0;i<sizeof mem;i++) mem[i]=0xff; for(i=0;i<n;i++) mem[i]=p[i]; }

int main(void) {
    Z80Bus bus = { rd, wr, inp, outp, 0 };
    Z80 z;
    Z80Insn in;
    static const u8 repeated[] = { 0xDD, 0xFD, 0x21, 0x34, 0x12 };
    static const u8 ignored_ed[] = { 0xDD, 0xED, 0x44 };
    static const u8 indexed_cb[] = { 0xDD, 0xCB, 0xFE, 0x46 };

    z80_reset(&z, &bus);

    load(repeated, sizeof repeated);
    z80_decode(&z, 0, &in);
    if (in.len != 5 || in.t != 18 || !same(in.text, "ld iy,0x1234")) return 1;

    load(ignored_ed, sizeof ignored_ed);
    z80_decode(&z, 0, &in);
    if (in.len != 3 || in.t != 12 || !same(in.text, "neg")) return 2;

    load(indexed_cb, sizeof indexed_cb);
    z80_decode(&z, 0, &in);
    if (in.len != 4 || in.t != 20 || !same(in.text, "bit 0,(ix-0x02)")) return 3;

    return 0;
}
