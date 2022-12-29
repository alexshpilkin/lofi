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

struct hart {
	xword_t ireg[32], pc, nextpc;
};

void execute(struct hart *, uint_least32_t);

enum { MAPR, MAPW, MAPX };
unsigned char *map(struct hart *, xword_t, xword_t, int);
/* FIXME unmap? */

enum {
	XALIGN, XACCES, ILLINS, EBREAK, RALIGN, RACCES, WALIGN, WACCES,
	UECALL, SECALL,   MECALL = 0xB, XPAGED, RPAGED,   WPAGED = 0xF,
};
void trap(struct hart *, xword_t, xword_t);

void ecall(struct hart *);

void csrread(struct hart *, xword_t *, unsigned);
void csrxchg(struct hart *, xword_t *, unsigned, xword_t);
void csrxset(struct hart *, xword_t *, unsigned, xword_t);
void csrxclr(struct hart *, xword_t *, unsigned, xword_t);
