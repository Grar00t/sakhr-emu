/* Sakhr-EMU Z80 core, stage 1: reset state + cycle-annotated decoder + demo.
 * Freestanding: no libc, raw syscalls. Decoder is table-light and follows the
 * x/y/z/p/q scheme, so undocumented forms fall out of the same rules. */
#include "z80.h"
#include "z80_opcodes.h"

void z80_reset(Z80 *z, const Z80Bus *bus) {
    u8 *p = (u8 *)z;
    unsigned n;
    for (n = 0; n < sizeof *z; n++) p[n] = 0;
    z->bus = *bus;
    /* Documented reset: PC=0, I=R=0, IFF1=IFF2=0, IM 0. AF=SP=0xFFFF on silicon;
     * the other registers are undefined there, 0xFFFF is chosen for determinism. */
    z->af = z->bc = z->de = z->hl = z->af_ = z->bc_ = z->de_ = z->hl_ = 0xFFFF;
    z->ix = z->iy = z->sp = 0xFFFF;
}

typedef struct {
    const Z80 *z;
    u16 pc;
    u8 len;
    char *o;
    const char *ix;     /* NULL, "ix" or "iy" */
    int t, tk, extra;   /* base T, taken T (absolute, same scale as t), index adjust */
} D;

static void ps(D *d, const char *s) { while (*s) *d->o++ = *s++; }
static void phx(D *d, u32 v, int digits) {
    int i;
    ps(d, "0x");
    for (i = (digits - 1) * 4; i >= 0; i -= 4) *d->o++ = "0123456789ABCDEF"[(v >> i) & 15];
}
static u8 nb(D *d) { u8 b = d->z->bus.read(d->z->bus.ctx, d->pc++); d->len++; return b; }
static u16 nw(D *d) { u16 lo = nb(d); return (u16)(lo | (u16)(nb(d) << 8)); }
static void tm(D *d, int t, int tk) { d->t = t; d->tk = tk ? tk : t; }

static void pdv(D *d, i8 v) {
    ps(d, "("); ps(d, d->ix); ps(d, v < 0 ? "-" : "+");
    phx(d, (u32)(v < 0 ? -(int)v : v), 2); ps(d, ")");
}
/* 8-bit operand r (0..7). sub: allow IXH/IXL substitution for r=4/5.
 * (hl) under a DD/FD prefix becomes (ix+d); the displacement byte is fetched here. */
static void R(D *d, int r, int sub) {
    if (d->ix && r == 6) { pdv(d, (i8)nb(d)); d->extra += 8; }
    else if (d->ix && sub && (r == 4 || r == 5)) { ps(d, d->ix); ps(d, r == 4 ? "h" : "l"); }
    else ps(d, Z_R[r]);
}
static void RP(D *d, int p) { ps(d, (d->ix && p == 2) ? d->ix : Z_RP[p]); }
static void RP2(D *d, int p) { ps(d, (d->ix && p == 2) ? d->ix : Z_RP2[p]); }
static void indn(D *d, u16 v) { ps(d, "("); phx(d, v, 4); ps(d, ")"); }
static void rel(D *d) { i8 e = (i8)nb(d); phx(d, (u16)(d->pc + e), 4); }

static void do_cb(D *d) {
    i8 off = 0; u8 op, x, y, z; int mem;
    if (d->ix) off = (i8)nb(d);              /* DD CB d op: displacement precedes opcode */
    op = nb(d); x = op >> 6; y = (op >> 3) & 7; z = op & 7;
    mem = d->ix || z == 6;
    if (x == 0) { ps(d, Z_ROT[y]); ps(d, " "); }
    else { ps(d, x == 1 ? "bit " : x == 2 ? "res " : "set "); *d->o++ = (char)('0' + y); ps(d, ","); }
    if (d->ix) pdv(d, off); else ps(d, Z_R[z]);
    if (d->ix && z != 6 && x != 1) { ps(d, ","); ps(d, Z_R[z]); }   /* undocumented: result copied to r */
    if (d->ix) { d->t = x == 1 ? 20 : 23; d->extra = -4; }          /* cancel the prefix charge */
    else d->t = !mem ? 8 : x == 1 ? 12 : 15;
    d->tk = d->t;
}

static void do_ed(D *d) {
    u8 op = nb(d), x = op >> 6, y = (op >> 3) & 7, z = op & 7, p = y >> 1, q = y & 1;
    tm(d, 8, 0);
    if (x == 2 && z < 4 && y >= 4) {
        ps(d, Z_BLK[y - 4][z]);
        tm(d, 16, (y >= 6 && (z == 0 || z == 1)) ? 21 : 0);          /* LDIR/LDDR/CPIR/CPDR */
        if (y >= 6 && z >= 2) d->tk = 21;                              /* INIR/OTIR etc. */
        return;
    }
    if (x != 1) { ps(d, "nop"); return; }                             /* undocumented ED NOPs, 8T */
    switch (z) {
    case 0: ps(d, "in "); ps(d, y == 6 ? "f" : Z_R[y]); ps(d, ",(c)"); tm(d, 12, 0); break;
    case 1: ps(d, "out (c),"); ps(d, y == 6 ? "0" : Z_R[y]); tm(d, 12, 0); break;
    case 2: ps(d, q ? "adc hl," : "sbc hl,"); ps(d, Z_RP[p]); tm(d, 15, 0); break;
    case 3:
        ps(d, "ld ");
        if (q) { ps(d, Z_RP[p]); ps(d, ","); indn(d, nw(d)); }
        else { indn(d, nw(d)); ps(d, ","); ps(d, Z_RP[p]); }
        tm(d, 20, 0); break;
    case 4: ps(d, "neg"); break;                                       /* all 8 encodings */
    case 5: ps(d, y == 1 ? "reti" : "retn"); tm(d, 14, 0); break;
    case 6: ps(d, "im "); *d->o++ = (char)('0' + Z_IM[y]); break;
    default: ps(d, Z_EDLD[y]); tm(d, y < 4 ? 9 : y < 6 ? 18 : 8, 0); break;
    }
}

static void fin(D *d, Z80Insn *in) {
    int pre = d->prefixes * 4;
    *d->o = 0;
    in->len = d->len;
    in->t = (u8)(d->t + pre + d->extra);
    in->t_taken = (u8)(d->tk + pre + d->extra);
}

void z80_decode(const Z80 *z, u16 pc, Z80Insn *in) {
    D d; u8 op, x, y, zz, p, q;
    d.z = z; d.pc = pc; d.len = 0; d.o = in->text; d.ix = 0; d.extra = 0; tm(&d, 4, 0);
    op = nb(&d);
    if (op == 0xDD || op == 0xFD) {
        d.ix = op == 0xDD ? "ix" : "iy";
        nx = z->bus.read(z->bus.ctx, d.pc);
        if (nx == 0xDD || nx == 0xFD || nx == 0xED) {          /* prefix is a 4T NOP; next byte re-decoded */
            ps(&d, "nop"); d.ix = 0; fin(&d, in); return;
        }
        op = nb(&d);
    }
    x = op >> 6; y = (op >> 3) & 7; zz = op & 7; p = y >> 1; q = y & 1;
    if (op == 0xCB) { do_cb(&d); fin(&d, in); return; }
    if (op == 0xED) { do_ed(&d); fin(&d, in); return; }
    switch (x) {
    case 0:
        switch (zz) {
        case 0:
            if (y == 0) ps(&d, "nop");
            else if (y == 1) { ps(&d, "ex af,af'"); }
            else if (y == 2) { ps(&d, "djnz "); tm(&d, 8, 13); rel(&d); }
            else if (y == 3) { ps(&d, "jr "); tm(&d, 12, 0); rel(&d); }
            else { ps(&d, "jr "); ps(&d, Z_CC[y - 4]); ps(&d, ","); tm(&d, 7, 12); rel(&d); }
            break;
        case 1:
            if (!q) { ps(&d, "ld "); RP(&d, p); ps(&d, ","); phx(&d, nw(&d), 4); tm(&d, 10, 0); }
            else { ps(&d, "add "); ps(&d, d.ix ? d.ix : "hl"); ps(&d, ","); RP(&d, p); tm(&d, 11, 0); }
            break;
        case 2:
            ps(&d, "ld ");
            if (p < 2) { tm(&d, 7, 0); if (!q) { ps(&d, p ? "(de),a" : "(bc),a"); } else ps(&d, p ? "a,(de)" : "a,(bc)"); }
            else if (p == 2) {
                tm(&d, 16, 0);
                if (!q) { indn(&d, nw(&d)); ps(&d, ","); ps(&d, d.ix ? d.ix : "hl"); }
                else { ps(&d, d.ix ? d.ix : "hl"); ps(&d, ","); indn(&d, nw(&d)); }
            } else {
                tm(&d, 13, 0);
                if (!q) { indn(&d, nw(&d)); ps(&d, ",a"); } else { ps(&d, "a,"); indn(&d, nw(&d)); }
            }
            break;
        case 3: ps(&d, q ? "dec " : "inc "); RP(&d, p); tm(&d, 6, 0); break;
        case 4: case 5:
            ps(&d, zz == 4 ? "inc " : "dec "); R(&d, y, 1); tm(&d, y == 6 ? 11 : 4, 0); break;
        case 6:
            ps(&d, "ld "); R(&d, y, 1); ps(&d, ","); phx(&d, nb(&d), 2);
            tm(&d, y == 6 ? 10 : 7, 0);
            if (y == 6 && d.ix) d.extra -= 3;                      /* LD (IX+d),n = 19T total */
            break;
        default: ps(&d, Z_ACC[y]); break;
        }
        break;
    case 1:
        if (y == 6 && zz == 6) { ps(&d, "halt"); break; }
        ps(&d, "ld "); R(&d, y, y != 6 && zz != 6); ps(&d, ","); R(&d, zz, y != 6 && zz != 6);
        tm(&d, (y == 6 || zz == 6) ? 7 : 4, 0);
        break;
    case 2: ps(&d, Z_ALU[y]); R(&d, zz, 1); tm(&d, zz == 6 ? 7 : 4, 0); break;
    default:
        switch (zz) {
        case 0: ps(&d, "ret "); ps(&d, Z_CC[y]); tm(&d, 5, 11); break;
        case 1:
            if (!q) { ps(&d, "pop "); RP2(&d, p); tm(&d, 10, 0); }
            else if (p == 0) { ps(&d, "ret"); tm(&d, 10, 0); }
            else if (p == 1) ps(&d, "exx");
            else if (p == 2) { ps(&d, "jp ("); ps(&d, d.ix ? d.ix : "hl"); ps(&d, ")"); }
            else { ps(&d, "ld sp,"); ps(&d, d.ix ? d.ix : "hl"); tm(&d, 6, 0); }
            break;
        case 2: ps(&d, "jp "); ps(&d, Z_CC[y]); ps(&d, ","); phx(&d, nw(&d), 4); tm(&d, 10, 0); break;
        case 3:
            switch (y) {
            case 0: ps(&d, "jp "); phx(&d, nw(&d), 4); tm(&d, 10, 0); break;
            case 2: ps(&d, "out ("); phx(&d, nb(&d), 2); ps(&d, "),a"); tm(&d, 11, 0); break;
            case 3: ps(&d, "in a,("); phx(&d, nb(&d), 2); ps(&d, ")"); tm(&d, 11, 0); break;
            case 4: ps(&d, "ex (sp),"); ps(&d, d.ix ? d.ix : "hl"); tm(&d, 19, 0); break;
            case 5: ps(&d, "ex de,hl"); break;                     /* never index-substituted */
            case 6: ps(&d, "di"); break;
            default: ps(&d, "ei"); break;
            }
            break;
        case 4: ps(&d, "call "); ps(&d, Z_CC[y]); ps(&d, ","); phx(&d, nw(&d), 4); tm(&d, 10, 17); break;
        case 5:
            if (!q) { ps(&d, "push "); RP2(&d, p); tm(&d, 11, 0); }
            else { ps(&d, "call "); phx(&d, nw(&d), 4); tm(&d, 17, 0); }
            break;
        case 6: ps(&d, Z_ALU[y]); phx(&d, nb(&d), 2); tm(&d, 7, 0); break;
        default: ps(&d, "rst "); phx(&d, (u32)y * 8, 2); tm(&d, 11, 0); break;
        }
    }
    fin(&d, in);
}

#ifdef Z80_DECODE_DEMO
static i64 sys3(i64 nr, i64 a, i64 b, i64 c) {
    i64 r;
    __asm__ volatile ("syscall" : "=a"(r) : "a"(nr), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return r;
}
static char ob[4096];
static u32 on;
static void oflush(void) { u32 o = 0; while (o < on) { i64 w = sys3(1, 1, (i64)(ob + o), on - o); if (w <= 0) break; o += (u32)w; } on = 0; }
static void oc(char c) { if (on == sizeof ob) oflush(); ob[on++] = c; }
static void os(const char *s) { while (*s) oc(*s++); }
static void oh(u32 v, int digits) { int i; for (i = (digits - 1) * 4; i >= 0; i -= 4) oc("0123456789ABCDEF"[(v >> i) & 15]); }
static void od(u32 v) { char t[12]; int i = 12; do { t[--i] = (char)('0' + v % 10); v /= 10; } while (v); while (i < 12) oc(t[i++]); }

static u8 rom[256];
static u8 b_read(void *c, u16 a) { (void)c; return a < 256 ? rom[a] : 0xFF; }
static void b_write(void *c, u16 a, u8 v) { (void)c; (void)a; (void)v; }
static u8 b_in(void *c, u16 p) { (void)c; (void)p; return 0xFF; }
static void b_out(void *c, u16 p, u8 v) { (void)c; (void)p; (void)v; }

__attribute__((noreturn, used)) static void demo_main(long argc, char **argv) {
    Z80Bus bus = { b_read, b_write, b_in, b_out, 0 };
    Z80 z; Z80Insn in; i64 fd, n = 0, r; int k;
    for (k = 0; k < 256; k++) rom[k] = 0xFF;
    if (argc < 2) { os("usage: z80dec ROM\n"); oflush(); sys3(231, 2, 0, 0); }
    fd = sys3(2, (i64)argv[1], 0, 0);
    if (fd < 0) { os("error: cannot open "); os(argv[1]); os("\n"); oflush(); sys3(231, 1, 0, 0); }
    while (n < 256 && (r = sys3(0, fd, (i64)(rom + n), 256 - n)) > 0) n += r;
    z80_reset(&z, &bus);
    for (k = 0; k < 20; k++) {
        int j, w = 0;
        z80_decode(&z, z.pc, &in);
        oh(z.pc, 4); os("  ");
        for (j = 0; j < in.len; j++) { oh(b_read(0, (u16)(z.pc + j)), 2); oc(' '); w += 3; }
        while (w++ < 13) oc(' ');
        os(in.text);
        for (j = 0; in.text[j]; j++) w++;
        while (w++ < 44) oc(' ');
        os("T="); od(in.t); if (in.t_taken != in.t) { oc('/'); od(in.t_taken); }
        oc('\n');
        z.pc = (u16)(z.pc + in.len); z.tstates += in.t;
    }
    os("pc="); oh(z.pc, 4); os(" tstates="); od((u32)z.tstates); oc('\n');
    oflush();
    sys3(231, 0, 0, 0);
    for (;;) {}
}
__asm__(".text\n.globl _start\n_start:\n xor %ebp,%ebp\n mov (%rsp),%rdi\n lea 8(%rsp),%rsi\n"
        " and $-16,%rsp\n call demo_main\n ud2\n");
#endif
