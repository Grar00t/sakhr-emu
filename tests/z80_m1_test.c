#include "../src/z80.h"

#define ROM_SIZE 13U
#define TRACE_MAX 32U
#define RAM_BASE 0xC000U
#define RAM_SIZE 0x4000U

typedef struct {
    u8 kind;
    u16 addr;
    u8 data;
    u64 tick;
} TraceEvent;

typedef struct {
    Z80 *cpu;
    u8 rom[ROM_SIZE];
    u8 ram[RAM_SIZE];
    TraceEvent trace[TRACE_MAX];
    unsigned trace_count;
    int bus_error;
} M1Machine;

static const u8 expected_rom[ROM_SIZE] = {
    0xF3, 0x31, 0x00, 0xF0, 0x21, 0x00, 0xC0,
    0x36, 0x4F, 0x23, 0x36, 0x4B, 0x76
};

static const TraceEvent expected_trace[] = {
#include "m1_ok.trace"
};

static long sys3(long nr, long a, long b, long c) {
    long r;
    __asm__ volatile ("syscall" : "=a"(r) : "a"(nr), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
    return r;
}

__attribute__((noreturn)) static void finish(int code) {
    static const char ok[] = "M1 OK
";
    static const char fail[] = "M1 FAIL
";
    const char *msg = code == 0 ? ok : fail;
    unsigned len = code == 0 ? (unsigned)(sizeof ok - 1U) : (unsigned)(sizeof fail - 1U);
    (void)sys3(1, 1, (long)msg, (long)len);
    (void)sys3(60, code, 0, 0);
    for (;;) {}
}

static int hex_digit(u8 c, u8 *v) {
    if (c >= (u8)'0' && c <= (u8)'9') { *v = (u8)(c - (u8)'0'); return 0; }
    if (c >= (u8)'A' && c <= (u8)'F') { *v = (u8)(c - (u8)'A' + 10U); return 0; }
    return 1;
}

static int load_rom(u8 out[ROM_SIZE]) {
    static const char path[] = "tests/m1_ok.rom";
    u8 buf[ROM_SIZE * 2U + 2U];
    long fd = sys3(2, (long)path, 0, 0);
    unsigned n = 0;
    unsigned i;

    if (fd < 0) return 1;
    while (n < sizeof buf) {
        long r = sys3(0, fd, (long)(buf + n), (long)(sizeof buf - n));
        if (r < 0) { (void)sys3(3, fd, 0, 0); return 2; }
        if (r == 0) break;
        n += (unsigned)r;
    }
    (void)sys3(3, fd, 0, 0);
    if (n != ROM_SIZE * 2U + 1U || buf[n - 1U] != (u8)'
') return 3;
    for (i = 0; i < ROM_SIZE; i++) {
        u8 hi, lo;
        if (hex_digit(buf[i * 2U], &hi) != 0 || hex_digit(buf[i * 2U + 1U], &lo) != 0) return 4;
        out[i] = (u8)((hi << 4) | lo);
        if (out[i] != expected_rom[i]) return 5;
    }
    return 0;
}

static void trace_add(M1Machine *m, u8 kind, u16 addr, u8 data) {
    if (m->trace_count >= TRACE_MAX) {
        m->bus_error = 1;
        return;
    }
    m->trace[m->trace_count].kind = kind;
    m->trace[m->trace_count].addr = addr;
    m->trace[m->trace_count].data = data;
    m->trace[m->trace_count].tick = m->cpu->master_ticks;
    m->trace_count++;
}

static u8 mem_read(void *ctx, u16 addr) {
    M1Machine *m = (M1Machine *)ctx;
    u8 v = 0xFF;
    if (addr < ROM_SIZE) v = m->rom[addr];
    else if (addr >= RAM_BASE) v = m->ram[(unsigned)(addr - RAM_BASE)];
    trace_add(m, m->cpu->bus_kind, addr, v);
    return v;
}

static void mem_write(void *ctx, u16 addr, u8 v) {
    M1Machine *m = (M1Machine *)ctx;
    trace_add(m, m->cpu->bus_kind, addr, v);
    if (addr >= RAM_BASE) m->ram[(unsigned)(addr - RAM_BASE)] = v;
    else m->bus_error = 1;
}

static u8 port_in(void *ctx, u16 port) {
    M1Machine *m = (M1Machine *)ctx;
    (void)port;
    m->bus_error = 1;
    return 0xFF;
}

static void port_out(void *ctx, u16 port, u8 v) {
    M1Machine *m = (M1Machine *)ctx;
    (void)port;
    (void)v;
    m->bus_error = 1;
}

static int trace_matches(const M1Machine *m) {
    unsigned i;
    if (m->trace_count != (unsigned)(sizeof expected_trace / sizeof expected_trace[0])) return 0;
    for (i = 0; i < m->trace_count; i++) {
        const TraceEvent *a = &m->trace[i];
        const TraceEvent *b = &expected_trace[i];
        if (a->kind != b->kind || a->addr != b->addr || a->data != b->data || a->tick != b->tick)
            return 0;
    }
    return 1;
}

static int run_test(void) {
    M1Machine m;
    Z80 z;
    Z80Bus bus;
    unsigned i;
    int rc;

    for (i = 0; i < sizeof m; i++) ((u8 *)&m)[i] = 0;
    rc = load_rom(m.rom);
    if (rc != 0) return 10 + rc;

    bus.read = mem_read;
    bus.write = mem_write;
    bus.in = port_in;
    bus.out = port_out;
    bus.ctx = &m;
    for (i = 0; i < sizeof z; i++) ((u8 *)&z)[i] = 0;
    z.bus = bus;
    m.cpu = &z;

    while (!z.halted) {
        rc = z80_step(&z);
        if (rc != Z80_STEP_OK && rc != Z80_STEP_HALTED) return 20;
    }

    if (m.bus_error) return 21;
    if (m.ram[0] != 0x4F || m.ram[1] != 0x4B) return 22;
    if (z.pc != 0x000C || z.sp != 0xF000 || z.hl != 0xC001) return 23;
    if (!z.halted || z.r != 7) return 24;
    if (z.tstates != 54 || z.master_ticks != 162) return 25;
    if (!trace_matches(&m)) return 26;
    return 0;
}

__attribute__((noreturn, used)) static void test_main(void) {
    finish(run_test());
}

__asm__(".text
.globl _start
_start:
 xor %ebp,%ebp
 and $-16,%rsp
 call test_main
 ud2
");
