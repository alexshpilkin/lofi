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
	xword_t ireg[32], pc, nextpc, lrsc;
	unsigned lr;
};

struct mhart {
	struct hart hart;
	xword_t mstatus, mie, mtvec, mscratch, mepc, mcause, mtval;
	xword_t pending;
#define UMODE ((xword_t)1) /* hijacked bit in pending */
};

void execute(struct hart *, uint_least32_t);

enum { MAPR, MAPX, MAPA, MAPW };
unsigned char *map(struct hart *, xword_t, xword_t, int);
void unmap(struct hart *);

enum {
	XALIGN, XACCES, ILLINS, EBREAK, RALIGN, RACCES, WALIGN, WACCES,
	UECALL, SECALL,   MECALL = 0xB, XPAGED, RPAGED,   WPAGED = 0xF,
/* this may not fit in an int */
#define INTRPT (XWORD_C(1) << XWORD_BIT - 1)
/* these need to be accessible in assembler */
#define SSI 1
#define MSI 3
#define STI 5
#define MTI 7
#define SEI 11
#define MEI 13
};
void trap(struct hart *, xword_t, xword_t);

void ecall(struct hart *);
void mret(struct hart *);
xword_t xalign(struct hart *, xword_t);

enum {
	MSTATUS    = 0x300,
#define MSTATUS_MIE  (XWORD_C(1) <<  3)
#define MSTATUS_MPIE (XWORD_C(1) <<  7)
#define MSTATUS_MPP  (XWORD_C(3) << 11)
	MISA       = 0x301,
	MIE        = 0x304,
	MTVEC      = 0x305,
#if XWORD_BIT < 64
	MSTATUSH   = 0x310,
#endif
	MSCRATCH   = 0x340,
	MEPC       = 0x341,
	MCAUSE     = 0x342,
	MTVAL      = 0x343,
	MIP        = 0x344,
	MVENDORID  = 0xF11,
	MARCHID    = 0xF12,
	MIMPID     = 0xF13,
	MHARTID    = 0xF14,
	MCONFIGPTR = 0xF15,
};
void csrread(struct hart *, xword_t *, unsigned);
void csrxchg(struct hart *, xword_t *, unsigned, xword_t);
void csrxset(struct hart *, xword_t *, unsigned, xword_t);
void csrxclr(struct hart *, xword_t *, unsigned, xword_t);
