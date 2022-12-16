#include <stdint.h>
#include <stdlib.h> /* FIXME */

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

#define MASK(L, H)    ((XWORD_C(1) << (H) - 1 << 1) - (XWORD_C(1) << (L)))
#define BITS(X, L, H) (((X) & MASK(L, H)) >> (L))
#define SIGN          (XWORD_C(1) << XWORD_BIT - 1)

struct insn {
	uint_least8_t opcode, funct3, funct7, rs1, rs2, rd;
	xword_t iimm, simm, bimm, uimm, jimm;
};

static struct insn decode(uint_least32_t x) {
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

struct hart {
	xword_t pc, ireg[32];
};

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

static void alu(struct hart *t, const struct insn *i) {
	switch (i->funct7) {
	case 0x00: case 0x20: aluint(t, i); break;
	default: abort(); /* FIXME */
	}
}

#if XWORD_BIT > 32
static void alw(struct hart *t, const struct insn *i) {
	switch (i->funct7) {
	case 0x00: case 0x20: alwint(t, i); break;
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

uint_least8_t *map(struct hart *t, xword_t a, xword_t n);
/* FIXME unmap? */

static void ldr(struct hart *t, const struct insn *i) {
	uint_least8_t *restrict m;
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
	uint_least8_t *restrict m;
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

static void execute(struct hart *t, const struct insn *i) {
	switch (i->opcode) {
	case 0x03: ldr(t, i); break;
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

/* FIXME move to a separate frontend */
#include <inttypes.h>
#include <stdio.h>

struct cpu {
	struct hart hart;
	uint_least8_t *restrict image;
	size_t size;
};

uint_least8_t *map(struct hart *t, xword_t addr, xword_t size) {
	struct cpu *c = (struct cpu *)t;
	if (addr & size - 1) abort(); /* FIXME */
	if (addr >= c->size || (addr + size & XWORD_MAX) >= c->size) abort(); /* FIXME */
	return &c->image[addr];
}

static char *binary(char *s, size_t n, uint_least32_t w) {
	s[n-1] = '\0';
	for (size_t i = n - 2; i < n; i--)
		s[i] = '0' + (w & 1), w >>= 1;
	return s;
}

const char irname[][4] = {
	"zero","ra",  "sp",  "gp",  "tp",  "t0",  "t1",  "t2",
	"s0",  "s1",  "a0",  "a1",  "a2",  "a3",  "a4",  "a5",
	"a6",  "a7",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",
	"s8",  "s9",  "s10", "s11", "t3",  "t4",  "t5",  "t6",
};

int main(int argc, char **argv) {
	uint_least8_t image[128] = {0};
	struct cpu c = {0};
	c.image = image; c.size = sizeof image / sizeof image[0];

	for (;;) {
		printf("  pc=0x%" PRIXXWORD "\n", c.hart.pc);
		for (size_t i = 0; i < sizeof irname / sizeof irname[0]; i += 4) {
			printf("%*.*s=0x%" PRIXXWORD " %*.*s=0x%" PRIXXWORD " "
			       "%*.*s=0x%" PRIXXWORD " %*.*s=0x%" PRIXXWORD "\n",
			       (int)sizeof irname[0], (int)sizeof irname[0],
			       irname[i],   c.hart.ireg[i],
			       (int)sizeof irname[0], (int)sizeof irname[0],
			       irname[i+1], c.hart.ireg[i+1],
			       (int)sizeof irname[0], (int)sizeof irname[0],
			       irname[i+2], c.hart.ireg[i+2],
			       (int)sizeof irname[0], (int)sizeof irname[0],
			       irname[i+3], c.hart.ireg[i+3]);
		}
		for (size_t i = 0; i < c.size; i += 16) {
			printf("0x%04zX  ", i);
			printf("%.2" PRIXLEAST8 " %.2" PRIXLEAST8 " "
			       "%.2" PRIXLEAST8 " %.2" PRIXLEAST8 " "
			       "%.2" PRIXLEAST8 " %.2" PRIXLEAST8 " "
			       "%.2" PRIXLEAST8 " %.2" PRIXLEAST8 "  ",
			       c.image[i],   c.image[i+1],
			       c.image[i+2], c.image[i+3],
			       c.image[i+4], c.image[i+5],
			       c.image[i+6], c.image[i+7]);
			printf("%.2" PRIXLEAST8 " %.2" PRIXLEAST8 " "
			       "%.2" PRIXLEAST8 " %.2" PRIXLEAST8 " "
			       "%.2" PRIXLEAST8 " %.2" PRIXLEAST8 " "
			       "%.2" PRIXLEAST8 " %.2" PRIXLEAST8 "\n",
			       c.image[i+ 8], c.image[i+ 9],
			       c.image[i+10], c.image[i+11],
			       c.image[i+12], c.image[i+13],
			       c.image[i+14], c.image[i+15]);
		}

		uint_least32_t x;
		if (scanf("%" SCNxLEAST32, &x) < 1) break;

		struct insn i = decode(x);
		char opcode[8], funct3[4], funct7[8];

		/* R-type: 40C5D533 sra a0, a1, a2
		           0110011 101 0100000 rd=10 rs1=11 rs2=12
		   I-type: A555C513 xori a0, a1, ~0x5AA
		           0010011 100 rd=10 rs1=11 I=0xFFFFFA55
		   S-type: AAA5A2A3 sw a0, ~0x55A(a1)
		           0100011 010 rs1=11 rs2=10 S=0xFFFFFAA5
		   B-type: DAB51263 foo: .skip 0xA5C; bne a0, a1, foo
		           1100011 001 rs1=10 rs2=11 B=0xFFFFF5A4
		           (NB: offset is from *start* of jump)
		   U-type: FEDCB537 lui a0, 0xFEDCB
		           0110111 rd=10 U=0xFEDCB000
		   J-type: A550A0EF foo: .skip 0xF55AC; jal ra, foo
		           1101111 rd=1 J=0xFFF0AA54
		           (NB: ditto, but *end* of jump in rd)
		 */

		printf("%s %s %s rd=%u (%.*s) rs1=%u (%.*s) rs2=%u (%.*s)\n"
		       "\tI = 0x%" PRIXXWORD "\n"
		       "\tS = 0x%" PRIXXWORD "\n"
		       "\tB = 0x%" PRIXXWORD "\n"
		       "\tU = 0x%" PRIXXWORD "\n"
		       "\tJ = 0x%" PRIXXWORD "\n",
		       binary(opcode, sizeof opcode, i.opcode),
		       binary(funct3, sizeof funct3, i.funct3),
		       binary(funct7, sizeof funct7, i.funct7),
		       (unsigned)i.rd,  (int)sizeof irname[0], irname[i.rd],
		       (unsigned)i.rs1, (int)sizeof irname[0], irname[i.rs1],
		       (unsigned)i.rs2, (int)sizeof irname[0], irname[i.rs2],
		       i.iimm, i.simm, i.bimm, i.uimm, i.jimm);

		execute(&c.hart, &i);
	}
}
