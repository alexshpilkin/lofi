#include <stdint.h>
#include <stdlib.h> /* FIXME */

#include "riscv.h"
#include "rvinsn.h"

#define SIGN (XWORD_C(1) << XWORD_BIT - 1)

static void mbz(unsigned x) {
	if (x) abort(); /* FIXME */
}

/* FIXME alu/alw duplication */

static void aluint(struct hart *t, uint_least32_t i) {
	enum { MASK7 = XWORD_BIT / 32 - 1 }; /* shamt bleeds into funct7 */
	unsigned reg = opcode(i) & 0x20;
	xword_t in1 = t->ireg[rs1(i)],
	        in2 = reg ? t->ireg[rs2(i)] : iimm(i),
	        out;
	switch (funct3(i)) {
	case 0: out = reg && funct7(i) ? in1 - in2 : in1 + in2; break;
	case 1: mbz(funct7(i) & ~(unsigned)MASK7);
	        out = in1 << (in2 & XWORD_BIT - 1);
	        break;
	case 2: mbz(reg && funct7(i)); out = (in1 ^ SIGN) < (in2 ^ SIGN); break;
	case 3: mbz(reg && funct7(i)); out = in1 < in2; break;
	case 4: mbz(reg && funct7(i)); out = in1 ^ in2; break;
	case 5: mbz(funct7(i) & ~(unsigned)MASK7 & ~0x20u);
	        out = funct7(i) & ~(unsigned)MASK7 && in1 & SIGN
	            ? ~((~in1 & XWORD_MAX) >> (in2 & XWORD_BIT - 1))
	            :     in1              >> (in2 & XWORD_BIT - 1);
	        break;
	case 6: mbz(reg && funct7(i)); out = in1 | in2; break;
	case 7: mbz(reg && funct7(i)); out = in1 & in2; break;
	}
	if (rd(i)) t->ireg[rd(i)] = out & XWORD_MAX;
}

#if XWORD_BIT > 32
static void alwint(struct hart *t, uint_least32_t i) {
	unsigned reg = opcode(i) & 0x20;
	xword_t in1 = t->ireg[rs1(i)],
	        in2 = reg ? t->ireg[rs2(i)] : iimm(i),
	        out;
	switch (funct3(i)) {
	case 0: out = reg && funct7(i) ? in1 - in2 : in1 + in2; break;
	case 1: mbz(funct7(i)); out = in1 << (in2 & 31); break;
	case 5: mbz(funct7(i) & ~0x20u);
	        out = funct7(i) && in1 & XWORD_C(1) << 31
	            ? ~((~in1 & (XWORD_C(1) << 32) - 1) >> (in2 & 31))
	            :    (in1 & (XWORD_C(1) << 32) - 1) >> (in2 & 31);
	        break;
	default: abort(); /* FIXME */
	}
	out &= (XWORD_C(1) << 32) - 1;
	out = (out ^ XWORD_C(1) << 31) - (XWORD_C(1) << 31);
	if (rd(i)) t->ireg[rd(i)] = out & XWORD_MAX;
}
#endif

static xword_t mulhu(xword_t x, xword_t y, xword_t neg) {
	xword_t a = x >> XWORD_BIT / 2,
	        b = x & (XWORD_C(1) << XWORD_BIT / 2) - 1,
	        c = y >> XWORD_BIT / 2,
	        d = y & (XWORD_C(1) << XWORD_BIT / 2) - 1,
	        p = a * d, q = b * c, r = a * c, s = b * d;
	r += p >> XWORD_BIT / 2; p &= (XWORD_C(1) << XWORD_BIT / 2) - 1;
	r += q >> XWORD_BIT / 2; q &= (XWORD_C(1) << XWORD_BIT / 2) - 1;
	r += p + q + (s >> XWORD_BIT / 2) >> XWORD_BIT / 2;
	return r + (neg && (p + q << XWORD_BIT / 2) + s & XWORD_MAX);
}

static void alumul(struct hart *t, uint_least32_t i) {
	xword_t in1 = t->ireg[rs1(i)],
	        in2 = t->ireg[rs2(i)],
	        neg = 0,
	        out;
	switch (funct3(i)) {
	case 0: out = in1 * in2; break;
	case 1: neg = (in1 ^ in2) & SIGN;
	        in2 = in2 & SIGN ? -in2 & XWORD_MAX : in2;
	        goto mulhsu;
	case 2: neg = in1 & SIGN;
	mulhsu: in1 = in1 & SIGN ? -in1 & XWORD_MAX : in1;
	case 3: out = mulhu(in1, in2, neg); break;
	case 4: neg = (in1 ^ in2) & SIGN;
	        in1 = in1 & SIGN ? -in1 & XWORD_MAX : in1;
	        in2 = in2 & SIGN ? -in2 & XWORD_MAX : in2;
	case 5: out = in2 ? in1 / in2 : neg ? 1 : -1; break;
	case 6: neg = in1 & SIGN;
	        in1 = in1 & SIGN ? -in1 & XWORD_MAX : in1;
	        in2 = in2 & SIGN ? -in2 & XWORD_MAX : in2;
	case 7: out = in2 ? in1 % in2 : in1; break;
	}
	if (rd(i)) t->ireg[rd(i)] = (neg ? -out : out) & XWORD_MAX;
}

#if XWORD_BIT > 32
static void alwmul(struct hart *t, uint_least32_t i) {
	xword_t in1 = t->ireg[rs1(i)],
	        in2 = t->ireg[rs2(i)],
	        neg = 0,
	        out;
	switch (funct3(i)) {
	case 0: out = in1 * in2; break;
	case 4: neg = (in1 ^ in2) & XWORD_C(1) << 31;
	        in1 = in1 & XWORD_C(1) << 31 ? -in1 : in1;
	        in2 = in2 & XWORD_C(1) << 31 ? -in2 : in2;
	case 5: in1 &= (XWORD_C(1) << 32) - 1;
	        in2 &= (XWORD_C(1) << 32) - 1;
	        out = in2 ? in1 / in2 : neg ? 1 : -1; break;
	case 6: neg = in1 & XWORD_C(1) << 31;
	        in1 = in1 & XWORD_C(1) << 31 ? -in1 : in1;
	        in2 = in2 & XWORD_C(1) << 31 ? -in2 : in2;
	case 7: in1 &= (XWORD_C(1) << 32) - 1;
	        in2 &= (XWORD_C(1) << 32) - 1;
	        out = in2 ? in1 % in2 : in1; break;
	default: abort(); /* FIXME */
	}
	out = (neg ? -out : out) & (XWORD_C(1) << 32) - 1;
	out = (out ^ XWORD_C(1) << 31) - (XWORD_C(1) << 31);
	if (rd(i)) t->ireg[rd(i)] = out & XWORD_MAX;
}
#endif

static void alu(struct hart *t, uint_least32_t i) {
	switch (funct7(i)) {
	case 0x00: case 0x20: aluint(t, i); break;
	case 0x01: alumul(t, i); break;
	default: abort(); /* FIXME */
	}
}

#if XWORD_BIT > 32
static void alw(struct hart *t, uint_least32_t i) {
	switch (funct7(i)) {
	case 0x00: case 0x20: alwint(t, i); break;
	case 0x01: alwmul(t, i); break;
	default: abort(); /* FIXME */
	}
}
#endif

static void lui(struct hart *t, uint_least32_t i) {
	xword_t out = opcode(i) & 0x20 ? uimm(i) : t->pc + uimm(i) & XWORD_MAX;
	if (rd(i)) t->ireg[rd(i)] = out;
}

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
	default: abort(); /* FIXME */
	}
	if (flg) t->nextpc = t->pc + bimm(i) & XWORD_MAX;
}

static void jal(struct hart *t, uint_least32_t i) {
	if (rd(i)) t->ireg[rd(i)] = t->pc + 4 & XWORD_MAX;
	t->nextpc = t->pc + jimm(i) & XWORD_MAX;
}

static void jalr(struct hart *t, uint_least32_t i) {
	xword_t in = t->ireg[rs1(i)];
	if (rd(i)) t->ireg[rd(i)] = t->pc + 4 & XWORD_MAX;
	t->nextpc = in + iimm(i) & XWORD_MAX - 1;
}

static void ldr(struct hart *t, uint_least32_t i) {
	unsigned char *restrict m;
	xword_t a = t->ireg[rs1(i)] + iimm(i) & XWORD_MAX,
	        x = 0,
	        s = 0;
	switch (funct3(i)) {
	case 0: s = XWORD_C(1) <<  7;
	case 4: m = map(t, a, 1); goto byte;
	case 1: s = XWORD_C(1) << 15;
	case 5: m = map(t, a, 2); goto half;
	case 2:
#if XWORD_BIT > 32
	        s = XWORD_C(1) << 31;
	case 6:
#endif
	        m = map(t, a, 4); goto word;
#if XWORD_BIT >= 64
	case 3: s = XWORD_C(1) << 63;
	        m = map(t, a, 8); goto doub;

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

	default: abort(); /* FIXME */
	}
	if (rd(i)) t->ireg[rd(i)] = (x ^ s) - s & XWORD_MAX;
}

static void str(struct hart *t, uint_least32_t i) {
	xword_t a = t->ireg[rs1(i)] + simm(i) & XWORD_MAX,
	        x = t->ireg[rs2(i)];
	unsigned char *restrict m;
	switch (funct3(i)) {
	case 0: m = map(t, a, 1); goto byte;
	case 1: m = map(t, a, 2); goto half;
	case 2: m = map(t, a, 4); goto word;
#if XWORD_BIT >= 64
	case 3: m = map(t, a, 8); goto doub;

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

	default: abort(); /* FIXME */
	}
}

static void mem(struct hart *t, uint_least32_t i) {
	switch (funct3(i)) {
	case 0: /* FENCE */ break;
	case 1: /* FENCE.I */ break;
	default: abort(); /* FIXME */
	}
}

static void super(struct hart *t, uint_least32_t i) {
	switch (iimm(i)) {
	case 0x000: if (!rs1(i) && !rd(i)) { ecall(t); break; }
	case 0x302: if (!rs1(i) && !rd(i)) { /* FIXME mret */ break; }
	default: abort(); /* FIXME */
	}
}

static void hyper(struct hart *t, uint_least32_t i) {
	abort(); /* FIXME */
}

static void sys(struct hart *t, uint_least32_t i) {
	xword_t in1 = funct3(i) & 4 ? rs1(i) : t->ireg[rs1(i)],
	        nul = 0, *out = rd(i) ? &t->ireg[rd(i)] : &nul;
	unsigned csr = iimm(i) & 0xFFF;
	switch (funct3(i)) {
	case 0: super(t, i); break;
	case 4: hyper(t, i); break;
	case 1: case 5: csrxchg(t, out, csr, in1); break;
	case 2: case 6: if (rs1(i)) csrxset(t, out, csr, in1); else csrread(t, out, csr); break;
	case 3: case 7: if (rs1(i)) csrxclr(t, out, csr, in1); else csrread(t, out, csr); break;
	}
}

void execute(struct hart *t, uint_least32_t i) {
	switch (opcode(i)) {
	case 0x03: ldr(t, i); break;
	case 0x0F: mem(t, i); break;
	case 0x13: aluint(t, i); break;
	case 0x17: lui(t, i); break;
#if XWORD_BIT > 32
	case 0x1B: alwint(t, i); break;
#endif
	case 0x23: str(t, i); break;
	case 0x33: alu(t, i); break;
	case 0x37: lui(t, i); break;
#if XWORD_BIT > 32
	case 0x3B: alw(t, i); break;
#endif
	case 0x63: bcc(t, i); break;
	case 0x67: jalr(t, i); break;
	case 0x6F: jal(t, i); break;
	case 0x73: sys(t, i); break;
	default: abort(); /* FIXME */
	}
}
