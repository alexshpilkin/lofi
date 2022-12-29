#include <stdint.h>

#include "riscv.h"
#include "rvexec.h"
#include "rvinsn.h"

#define SIGN (XWORD_C(1) << XWORD_BIT - 1)

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

__attribute__((alias("xmul"))) execute_t xops01;

static void xmul(struct hart *t, uint_least32_t i) {
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
__attribute__((alias("wmul"))) execute_t wops01;

static void wmul(struct hart *t, uint_least32_t i) {
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
	default: trap(t, ILLINS, i); return;
	}
	out = (neg ? -out : out) & (XWORD_C(1) << 32) - 1;
	out = (out ^ XWORD_C(1) << 31) - (XWORD_C(1) << 31);
	if (rd(i)) t->ireg[rd(i)] = out & XWORD_MAX;
}
#endif
