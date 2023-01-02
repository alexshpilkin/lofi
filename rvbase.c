#include <stdint.h>

#include "riscv.h"
#include "rvexec.h"
#include "rvinsn.h"

#ifdef DEFINE_EXTENSION
DEFINE_EXTENSION(I)
#endif

#define SIGN (XWORD_C(1) << XWORD_BIT - 1)

static void illins(struct hart *t, uint_least32_t i) {
	trap(t, ILLINS, i);
}

#define MBZ(X) do if (X) { trap(t, ILLINS, i); return; } while (0)

/* FIXME alu/alw duplication */

__attribute__((alias("xalu"))) execute_t exec04, xops00, xops20;

static void xalu(struct hart *t, uint_least32_t i) {
	enum { MASK7 = XWORD_BIT / 32 - 1 }; /* shamt bleeds into funct7 */
	unsigned reg = opcode(i) & 0x20;
	xword_t in1 = t->ireg[rs1(i)],
	        in2 = reg ? t->ireg[rs2(i)] : iimm(i),
	        out;
	switch (funct3(i)) {
	case 0: out = reg && funct7(i) ? in1 - in2 : in1 + in2; break;
	case 1: MBZ(funct7(i) & ~(unsigned)MASK7);
	        out = in1 << (in2 & XWORD_BIT - 1);
	        break;
	case 2: MBZ(reg && funct7(i)); out = (in1 ^ SIGN) < (in2 ^ SIGN); break;
	case 3: MBZ(reg && funct7(i)); out = in1 < in2; break;
	case 4: MBZ(reg && funct7(i)); out = in1 ^ in2; break;
	case 5: MBZ(funct7(i) & ~(unsigned)MASK7 & ~0x20u);
	        out = funct7(i) & ~(unsigned)MASK7 && in1 & SIGN
	            ? ~((~in1 & XWORD_MAX) >> (in2 & XWORD_BIT - 1))
	            :     in1              >> (in2 & XWORD_BIT - 1);
	        break;
	case 6: MBZ(reg && funct7(i)); out = in1 | in2; break;
	case 7: MBZ(reg && funct7(i)); out = in1 & in2; break;
	}
	if (rd(i)) t->ireg[rd(i)] = out & XWORD_MAX;
}

#if XWORD_BIT > 32
__attribute__((alias("walu"))) execute_t exec06, wops00, wops20;

static void walu(struct hart *t, uint_least32_t i) {
	unsigned reg = opcode(i) & 0x20;
	xword_t in1 = t->ireg[rs1(i)],
	        in2 = reg ? t->ireg[rs2(i)] : iimm(i),
	        out;
	switch (funct3(i)) {
	case 0: out = reg && funct7(i) ? in1 - in2 : in1 + in2; break;
	case 1: MBZ(funct7(i)); out = in1 << (in2 & 31); break;
	case 5: MBZ(funct7(i) & ~0x20u);
	        out = funct7(i) && in1 & XWORD_C(1) << 31
	            ? ~((~in1 & (XWORD_C(1) << 32) - 1) >> (in2 & 31))
	            :    (in1 & (XWORD_C(1) << 32) - 1) >> (in2 & 31);
	        break;
	default: trap(t, ILLINS, i); return;
	}
	out &= (XWORD_C(1) << 32) - 1;
	out = (out ^ XWORD_C(1) << 31) - (XWORD_C(1) << 31);
	if (rd(i)) t->ireg[rd(i)] = out & XWORD_MAX;
}
#endif

__attribute__((alias("xop"))) execute_t exec0C;

#define xops00 xops00_ /* defined here */
#define xops20 xops20_ /* defined here */
__attribute__((alias("illins"), weak)) execute_t FUNCT7(xops);
#undef xops00
#undef xops20

static void xop(struct hart *t, uint_least32_t i) {
	static execute_t *const xops[0x80] = { FUNCT7(&xops) };
	(*xops[funct7(i)])(t, i);
}

#if XWORD_BIT > 32
__attribute__((alias("wop"))) execute_t exec0E;

#define wops00 wops00_ /* defined here */
#define wops20 wops20_ /* defined here */
__attribute__((alias("illins"), weak)) execute_t FUNCT7(wops);
#undef wops00
#undef wops20

static void wop(struct hart *t, uint_least32_t i) {
	static execute_t *const wops[0x80] = { FUNCT7(&wops) };
	(*wops[funct7(i)])(t, i);
}
#endif

__attribute__((alias("lui"))) execute_t exec05, exec0D;

static void lui(struct hart *t, uint_least32_t i) {
	xword_t out = opcode(i) & 0x20 ? uimm(i) : t->pc + uimm(i) & XWORD_MAX;
	if (rd(i)) t->ireg[rd(i)] = out;
}

__attribute__((alias("bcc"))) execute_t exec18;

static void bcc(struct hart *t, uint_least32_t i) {
	xword_t in1 = t->ireg[rs1(i)],
	        in2 = t->ireg[rs2(i)];
	int flg;
	switch (funct3(i)) {
	case 0: flg = in1 == in2; break;
	case 1: flg = in1 != in2; break;
	case 4: flg = (in1 ^ SIGN) <  (in2 ^ SIGN); break;
	case 5: flg = (in1 ^ SIGN) >= (in2 ^ SIGN); break;
	case 6: flg = in1 <  in2; break;
	case 7: flg = in1 >= in2; break;
	default: trap(t, ILLINS, i); return;
	}
	if (flg) t->nextpc = t->pc + bimm(i) & XWORD_MAX;
}

__attribute__((alias("jal"))) execute_t exec1B;

static void jal(struct hart *t, uint_least32_t i) {
	t->lr = rd(i); t->nextpc = t->pc + jimm(i) & XWORD_MAX;
}

__attribute__((alias("jalr"))) execute_t exec19;

static void jalr(struct hart *t, uint_least32_t i) {
	xword_t in = t->ireg[rs1(i)];
	t->lr = rd(i); t->nextpc = in + iimm(i) & XWORD_MAX - 1;
}

__attribute__((alias("ldr"))) execute_t exec00;

static void ldr(struct hart *t, uint_least32_t i) {
	const unsigned char *restrict m;
	xword_t a = t->ireg[rs1(i)] + iimm(i) & XWORD_MAX,
	        x = 0,
	        s = 0;
	switch (funct3(i)) {
	case 0: s = XWORD_C(1) <<  7;
	case 4: if (!(m = map(t, a, 1, MAPR))) return; goto byte;
	case 1: s = XWORD_C(1) << 15;
	case 5: if (!(m = map(t, a, 2, MAPR))) return; goto half;
	case 2:
#if XWORD_BIT > 32
	        s = XWORD_C(1) << 31;
	case 6:
#endif
	/*****/ if (!(m = map(t, a, 4, MAPR))) return; goto word;
#if XWORD_BIT >= 64
	case 3: s = XWORD_C(1) << 63;
	/*****/ if (!(m = map(t, a, 8, MAPR))) return; goto doub;

	doub: x |= (xword_t)m[7] << 56;
	      x |= (xword_t)m[6] << 48;
	      x |= (xword_t)m[5] << 40;
	      x |= (xword_t)m[4] << 32;
#endif
	word: x |= (xword_t)m[3] << 24;
	      x |= (xword_t)m[2] << 16;
	half: x |= (xword_t)m[1] <<  8;
	byte: x |= (xword_t)m[0];
	      break;

	default: trap(t, ILLINS, i); return;
	}
	if (rd(i)) t->ireg[rd(i)] = (x ^ s) - s & XWORD_MAX;
}

__attribute__((alias("str"))) execute_t exec08;

static void str(struct hart *t, uint_least32_t i) {
	xword_t a = t->ireg[rs1(i)] + simm(i) & XWORD_MAX,
	        x = t->ireg[rs2(i)];
	unsigned char *restrict m;
	switch (funct3(i)) {
	case 0: if (!(m = map(t, a, 1, MAPW))) return; goto byte;
	case 1: if (!(m = map(t, a, 2, MAPW))) return; goto half;
	case 2: if (!(m = map(t, a, 4, MAPW))) return; goto word;
#if XWORD_BIT >= 64
	case 3: if (!(m = map(t, a, 8, MAPW))) return; goto doub;

	doub: m[7] = x >> 56 & 0xFF;
	      m[6] = x >> 48 & 0xFF;
	      m[5] = x >> 40 & 0xFF;
	      m[4] = x >> 32 & 0xFF;
#endif
	word: m[3] = x >> 24 & 0xFF;
	      m[2] = x >> 16 & 0xFF;
	half: m[1] = x >>  8 & 0xFF;
	byte: m[0] = x       & 0xFF;
	      break;

	default: trap(t, ILLINS, i); return;
	}
}

__attribute__((alias("fence"))) execute_t memo0, memo1;

static void fence(struct hart *t, uint_least32_t i) {
	(void)t; (void)i; /* FIXME */
}

__attribute__((alias("mem"))) execute_t exec03;

#define memo0 memo0_ /* defined here */
#define memo1 memo1_ /* defined here */
__attribute__((alias("illins"), weak)) execute_t FUNCT3(memo);
#undef memo0
#undef memo1

static void mem(struct hart *t, uint_least32_t i) {
	static execute_t *const memo[8] = { FUNCT3(memo) };
	(*memo[funct3(i)])(t, i);
}
