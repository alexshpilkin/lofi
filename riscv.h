/* requires stdint.h */

#ifndef XWORD_BIT
#define XWORD_BIT 32
#endif

#if XWORD_BIT == 32

typedef uint_least32_t xword_t;
#define XWORD_C UINT32_C
#define PRIXXWORD ".8" PRIXLEAST32

#elif XWORD_BIT == 64

typedef uint_least64_t xword_t;
#define XWORD_C UINT64_C
#define PRIXXWORD ".16" PRIXLEAST64

#else

#error "Unsupported XWORD_BIT value"

#endif

#define XWORD_MAX ((XWORD_C(1) << XWORD_BIT - 1 << 1) - 1)

struct insn {
	unsigned opcode : 7, funct3 : 3, funct7 : 7, rs1 : 5, rs2 : 5, rd : 5;
	xword_t iimm, simm, bimm, uimm, jimm;
};

struct insn decode(xword_t);

struct hart {
	xword_t pc, ireg[32];
};

void execute(struct hart *, const struct insn *);

uint_least8_t *map(struct hart *t, xword_t a, xword_t n);
/* FIXME unmap? */
