#include <stdint.h>

#include "riscv.h"
#include "rvexec.h"
#include "rvinsn.h"

#ifdef DEFINE_EXTENSION
DEFINE_EXTENSION(A)
#endif

/* FIXME stub */

#define SIGN (XWORD_C(1) << XWORD_BIT - 1)

static void illins(struct hart *t, uint_least32_t i) {
	trap(t, ILLINS, i);
}

static void amow(struct hart *t, uint_least32_t i) {
	unsigned op = funct7(i) >> 2;
	unsigned char *restrict m = map(t, t->ireg[rs1(i)], 4, op == 2 ? MAPR : MAPA);
	if (!m) return;
	xword_t in1 = m[0]       |
	     (xword_t)m[1] <<  8 |
	     (xword_t)m[2] << 16 |
	     (xword_t)m[3] << 24;
	xword_t in2 = t->ireg[rs2(i)], out = in1;

	switch (funct7(i) >> 2) {
	case  0: out = in1 + in2; break;
	case  1: out = in2; break;
	case  2: out = in1; t->lrsc = ~t->ireg[rs1(i)]; break;
	case  3: if (!(in1 = t->lrsc != ~t->ireg[rs1(i)])) out = in2;
	         t->lrsc = 0; break;
	case  4: out = in1 ^ in2; break;
	case  8: out = in1 | in2; break;
	case 12: out = in1 & in2; break;
	case 16: out = (in1 ^ SIGN) < (in2 ^ SIGN) ? in1 : in2; break;
	case 20: out = (in1 ^ SIGN) > (in2 ^ SIGN) ? in1 : in2; break;
	case 24: out = in1 < in2 ? in1 : in2; break;
	case 28: out = in1 > in2 ? in1 : in2; break;
	default: trap(t, ILLINS, i); return;
	}

	if (rd(i)) t->ireg[rd(i)] = in1;

	m[3] = out >> 24 & 0xFF;
	m[2] = out >> 16 & 0xFF;
	m[1] = out >>  8 & 0xFF;
	m[0] = out       & 0xFF;
}

__attribute__((alias("amow"))) execute_t amop2;

#if XWORD_BIT >= 64
static void amod(struct hart *t, uint_least32_t i) {
#warning "64-bit atomics not implemented" /* FIXME */
	trap(t, ILLINS, i);
}

__attribute__((alias("amod"))) execute_t amop3;
#endif

#define amop2 amop2_ /* defined here */
#if XWORD_BIT >= 64
#define amop3 amop3_ /* defined here */
#endif
__attribute__((alias("illins"), weak)) execute_t HEX3(amop);
#undef amop2
#if XWORD_BIT >= 64
#undef amop3
#endif

static void amo(struct hart *t, uint_least32_t i) {
	static execute_t *const amop[8] = { HEX3(&amop) };
	(*amop[funct3(i)])(t, i);
}

__attribute__((alias("amo"))) execute_t exec0B;
