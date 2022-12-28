/* requires stdint.h, riscv.h */

#define BITS(X, L, H) (((X) & (XWORD_C(1) << ((H) - 1) << 1) - 1) >> (L))

static inline unsigned opcode(uint_least32_t x) { return BITS(x,  0,  7); }
static inline unsigned rd    (uint_least32_t x) { return BITS(x,  7, 12); }
static inline unsigned funct3(uint_least32_t x) { return BITS(x, 12, 15); }
static inline unsigned rs1   (uint_least32_t x) { return BITS(x, 15, 20); }
static inline unsigned rs2   (uint_least32_t x) { return BITS(x, 20, 25); }
static inline unsigned funct7(uint_least32_t x) { return BITS(x, 25, 32); }

static inline xword_t uimm(uint_least32_t x) {
	xword_t u = x & ~((XWORD_C(1) << 12) - 1);
	return (u ^ XWORD_C(1) << 31) - (XWORD_C(1) << 31);
}

static inline xword_t iimm(uint_least32_t x) {
	return (BITS(x, 20, 32) ^ 1 << 11) - (1 << 11);
}

static inline xword_t simm(uint_least32_t x) {
	return iimm(x) & ~XWORD_C(31) | rd(x);
}

static inline xword_t bimm(uint_least32_t x) {
	xword_t s = simm(x);
	return s & ~(1 | XWORD_C(1) << 11) | (s & 1) << 11;
}

static inline xword_t jimm(uint_least32_t x) {
	xword_t i = iimm(x);
	return i & ~(1 | (XWORD_C(1) << 20) - (1 << 11)) | (i & 1) << 11 |
	       x &       (XWORD_C(1) << 20) - (1 << 11);
}

#undef BITS
