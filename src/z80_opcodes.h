#ifndef SAKHR_Z80_OPCODES_H
#define SAKHR_Z80_OPCODES_H
/* Name tables for the x/y/z/p/q opcode decomposition:
 *   x = op>>6, y = (op>>3)&7, z = op&7, p = y>>1, q = y&1 */

static const char *const Z_R[8]   = { "b", "c", "d", "e", "h", "l", "(hl)", "a" };
static const char *const Z_RP[4]  = { "bc", "de", "hl", "sp" };
static const char *const Z_RP2[4] = { "bc", "de", "hl", "af" };
static const char *const Z_CC[8]  = { "nz", "z", "nc", "c", "po", "pe", "p", "m" };
static const char *const Z_ALU[8] = { "add a,", "adc a,", "sub ", "sbc a,", "and ", "xor ", "or ", "cp " };
/* index 6 is the undocumented SLL (shift left, set bit 0) */
static const char *const Z_ROT[8] = { "rlc", "rrc", "rl", "rr", "sla", "sra", "sll", "srl" };
static const char *const Z_ACC[8] = { "rlca", "rrca", "rla", "rra", "daa", "cpl", "scf", "ccf" };
static const char *const Z_BLK[4][4] = {
    { "ldi", "cpi", "ini", "outi" },   { "ldd", "cpd", "ind", "outd" },
    { "ldir", "cpir", "inir", "otir" }, { "lddr", "cpdr", "indr", "otdr" }
};
static const char *const Z_EDLD[8] = { "ld i,a", "ld r,a", "ld a,i", "ld a,r", "rrd", "rld", "nop", "nop" };
static const u8 Z_IM[8] = { 0, 0, 1, 2, 0, 0, 1, 2 };   /* ED 4x/5x/6x/7x z=6, undocumented mirrors */

#endif
