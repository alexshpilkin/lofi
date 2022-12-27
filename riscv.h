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
	xword_t ireg[32], pc, nextpc;
};

void execute(struct hart *, const struct insn *);

unsigned char *map(struct hart *t, xword_t a, xword_t n);
/* FIXME unmap? */

void ecall(struct hart *);

void csrread(struct hart *, xword_t *, unsigned);
void csrxchg(struct hart *, xword_t *, unsigned, xword_t);
void csrxset(struct hart *, xword_t *, unsigned, xword_t);
void csrxclr(struct hart *, xword_t *, unsigned, xword_t);
