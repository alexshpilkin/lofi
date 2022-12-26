#include <stdint.h>
#include <stdlib.h> /* FIXME */

#include "riscv.h"

#define MASK(L, H)    ((XWORD_C(1) << (H) - 1 << 1) - (XWORD_C(1) << (L)))
#define BITS(X, L, H) (((X) & MASK(L, H)) >> (L))
#define SIGN          (XWORD_C(1) << XWORD_BIT - 1)

struct insn decode(uint_least32_t x) {
	struct insn i = {0};

	i.opcode = BITS(x,  0,  7); i.uimm = (x & MASK(12, 32) ^ XWORD_C(1) << 31) - (XWORD_C(1) << 31);
	i.rd     = BITS(x,  7, 12);
	i.funct3 = BITS(x, 12, 15);
	i.rs1    = BITS(x, 15, 20);
	i.rs2    = BITS(x, 20, 25); i.iimm = (BITS(x, 20, 32) ^ 1 << 11) - (1 << 11);
	i.funct7 = BITS(x, 25, 32);

	i.simm = i.iimm & ~MASK(0, 5) | i.rd;
	i.bimm = i.simm & ~(1 | MASK(11, 12)) | (i.simm & 1) << 11;
	i.jimm = i.iimm & ~(1 | MASK(11, 20)) | (i.iimm & 1) << 11 | i.uimm & MASK(12, 20);

	return i;
}

static void mbz(unsigned x) {
	if (x) abort(); /* FIXME */
}

/* FIXME alu/alw duplication */

static void aluint(struct hart *t, const struct insn *i) {
	enum { MASK7 = XWORD_BIT / 32 - 1 }; /* shamt bleeds into funct7 */
	unsigned reg = i->opcode & 0x20;
	xword_t in1 = t->ireg[i->rs1],
	        in2 = reg ? t->ireg[i->rs2] : i->iimm,
	        out;
	switch (i->funct3) {
	case 0: out = reg && i->funct7 ? in1 - in2 : in1 + in2; break;
	case 1: mbz(i->funct7 & ~(unsigned)MASK7);
	        out = in1 << (in2 & XWORD_BIT - 1);
	        break;
	case 2: mbz(reg && i->funct7); out = (in1 ^ SIGN) < (in2 ^ SIGN); break;
	case 3: mbz(reg && i->funct7); out = in1 < in2; break;
	case 4: mbz(reg && i->funct7); out = in1 ^ in2; break;
	case 5: mbz(i->funct7 & ~(unsigned)MASK7 & ~0x20u);
	        out = i->funct7 & ~(unsigned)MASK7 && in1 & SIGN
	            ? ~(~in1 >> (in2 & XWORD_BIT - 1))
	            :    in1 >> (in2 & XWORD_BIT - 1);
	        break;
	case 6: mbz(reg && i->funct7); out = in1 | in2; break;
	case 7: mbz(reg && i->funct7); out = in1 & in2; break;
	}
	if (i->rd) t->ireg[i->rd] = out & XWORD_MAX;
}

#if XWORD_BIT > 32
static void alwint(struct hart *t, const struct insn *i) {
	unsigned reg = i->opcode & 0x20;
	xword_t in1 = t->ireg[i->rs1],
	        in2 = reg ? t->ireg[i->rs2] : i->iimm,
	        out;
	switch (i->funct3) {
	case 0: out = reg && i->funct7 ? in1 - in2 : in1 + in2; break;
	case 1: mbz(i->funct7); out = in1 << (in2 & 31); break;
	case 5: mbz(i->funct7 & ~0x20u);
	        out = i->funct7 && in1 & XWORD_C(1) << 31
	            ? ~(~in1 >> (in2 & 31))
	            :    in1 >> (in2 & 31);
	        break;
	default: abort(); /* FIXME */
	}
	out &= (XWORD_C(1) << 32) - 1;
	out = (out ^ XWORD_C(1) << 31) - (XWORD_C(1) << 31);
	if (i->rd) t->ireg[i->rd] = out & XWORD_MAX;
}
#endif

static xword_t mulhu(xword_t x, xword_t y) {
	xword_t a = x >> XWORD_BIT / 2,
	        b = x & (XWORD_C(1) << XWORD_BIT / 2) - 1,
	        c = y >> XWORD_BIT / 2,
	        d = y & (XWORD_C(1) << XWORD_BIT / 2) - 1,
	        p = a * d, q = b * c,
	        z = a * c + (p >> XWORD_BIT / 2) + (q >> XWORD_BIT / 2);
	p &= (XWORD_C(1) << XWORD_BIT / 2) - 1;
	q &= (XWORD_C(1) << XWORD_BIT / 2) - 1;
	return z + (p + q + (b * d >> XWORD_BIT / 2) >> XWORD_BIT / 2);
}

static void alumul(struct hart *t, const struct insn *i) {
	xword_t in1 = t->ireg[i->rs1],
	        in2 = t->ireg[i->rs2],
	        neg = 0,
	        out;
	switch (i->funct3) {
	case 0: out = in1 * in2; break;
	case 1: neg = (in1 ^ in2) & SIGN;
	        in2 = in2 & SIGN ? -in2 & XWORD_MAX : in2;
	        goto mulhsu;
	case 2: neg = in1 & SIGN;
	mulhsu: in1 = in1 & SIGN ? -in1 & XWORD_MAX : in1;
	case 3: out = mulhu(in1, in2); break;
	case 4: neg = (in1 ^ in2) & SIGN;
	        in1 = in1 & SIGN ? -in1 & XWORD_MAX : in1;
	        in2 = in2 & SIGN ? -in2 & XWORD_MAX : in2;
	case 5: out = in2 ? in1 / in2 : neg ? 1 : -1; break;
	case 6: neg = in1 & SIGN;
	        in1 = in1 & SIGN ? -in1 & XWORD_MAX : in1;
	        in2 = in2 & SIGN ? -in2 & XWORD_MAX : in2;
	case 7: out = in2 ? in1 % in2 : in1; break;
	}
	if (i->rd) t->ireg[i->rd] = (neg ? -out : out) & XWORD_MAX;
}

#if XWORD_BIT > 32
static void alwmul(struct hart *t, const struct insn *i) {
	xword_t in1 = t->ireg[i->rs1],
	        in2 = t->ireg[i->rs2],
	        neg = 0,
	        out;
	switch (i->funct3) {
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
	out &= (XWORD_C(1) << 32) - 1;
	out = (out ^ XWORD_C(1) << 31) - (XWORD_C(1) << 31);
	if (i->rd) t->ireg[i->rd] = (neg ? -out : out) & XWORD_MAX;
}
#endif

static void alu(struct hart *t, const struct insn *i) {
	switch (i->funct7) {
	case 0x00: case 0x20: aluint(t, i); break;
	case 0x01: alumul(t, i); break;
	default: abort(); /* FIXME */
	}
}

#if XWORD_BIT > 32
static void alw(struct hart *t, const struct insn *i) {
	switch (i->funct7) {
	case 0x00: case 0x20: alwint(t, i); break;
	case 0x01: alwmul(t, i); break;
	default: abort(); /* FIXME */
	}
}
#endif

static void lui(struct hart *t, const struct insn *i) {
	xword_t out = i->opcode & 0x20 ? i->uimm : t->pc + i->uimm & XWORD_MAX;
	if (i->rd) t->ireg[i->rd] = out;
}

static void bcc(struct hart *t, const struct insn *i) {
	xword_t in1 = t->ireg[i->rs1],
	        in2 = t->ireg[i->rs2];
	int flg;
	switch (i->funct3) {
	case 0: flg = in1 == in2; break;
	case 1: flg = in1 != in2; break;
	case 4: flg = (in1 ^ SIGN) <  (in2 ^ SIGN); break;
	case 5: flg = (in1 ^ SIGN) >= (in2 ^ SIGN); break;
	case 6: flg = in1 <  in2; break;
	case 7: flg = in1 >= in2; break;
	default: abort(); /* FIXME */
	}
	if (flg) t->pc = t->pc + i->bimm & XWORD_MAX; /* FIXME - 4 ? */
}

static void jal(struct hart *t, const struct insn *i) {
	if (i->rd) t->ireg[i->rd] = t->pc + 4 & XWORD_MAX;
	t->pc = t->pc + i->jimm & XWORD_MAX; /* FIXME - 4 ? */
}

static void jalr(struct hart *t, const struct insn *i) {
	xword_t in = t->ireg[i->rs1];
	if (i->rd) t->ireg[i->rd] = t->pc + 4 & XWORD_MAX;
	t->pc = in + i->iimm & XWORD_MAX - 1; /* FIXME - 4 ? */
}

static void ldr(struct hart *t, const struct insn *i) {
	unsigned char *restrict m;
	xword_t a = t->ireg[i->rs1] + i->iimm & XWORD_MAX,
	        x = 0,
	        s = 0;
	switch (i->funct3) {
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
	if (i->rd) t->ireg[i->rd] = (x ^ s) - s & XWORD_MAX;
}

static void str(struct hart *t, const struct insn *i) {
	xword_t a = t->ireg[i->rs1] + i->simm & XWORD_MAX,
	        x = t->ireg[i->rs2];
	unsigned char *restrict m;
	switch (i->funct3) {
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

static void mem(struct hart *t, const struct insn *i) {
	switch (i->funct3) {
	case 0: /* FENCE */ break;
	case 1: /* FENCE.I */ break;
	default: abort(); /* FIXME */
	}
}

void execute(struct hart *t, const struct insn *i) {
	switch (i->opcode) {
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
	default: abort(); /* FIXME */
	}
}
