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

static void aluint(struct hart *t, const struct insn *i) {
	unsigned reg = i->opcode & 0x20;
	xword_t in1 = t->ireg[i->rs1],
	        in2 = reg ? t->ireg[i->rs2] : i->iimm,
	        out;
	switch (i->funct3) {
	case 0: out = reg && i->funct7 ? in1 - in2 : in1 + in2; break;
	case 1: mbz(i->funct7); out = in1 << (in2 & XWORD_BIT - 1); break;
	case 2: mbz(reg && i->funct7); out = (in1 ^ SIGN) < (in2 ^ SIGN); break;
	case 3: mbz(reg && i->funct7); out = in1 < in2; break;
	case 4: mbz(reg && i->funct7); out = in1 ^ in2; break;
	case 5: mbz(i->funct7 & ~0x20u);
	        out = i->funct7 && in1 & SIGN
	            ? ~(~in1 >> (in2 & XWORD_BIT - 1))
	            :    in1 >> (in2 & XWORD_BIT - 1);
	        break;
	case 6: mbz(reg && i->funct7); out = in1 | in2; break;
	case 7: mbz(reg && i->funct7); out = in1 & in2; break;
	}
	if (i->rd) t->ireg[i->rd] = out & XWORD_MAX;
}

static void alu(struct hart *t, const struct insn *i) {
	switch (i->funct7) {
	case 0x00: case 0x20: aluint(t, i); break;
	default: abort(); /* FIXME */
	}
}

static void lui(struct hart *t, const struct insn *i) {
	xword_t out = i->opcode & 0x20 ? i->uimm : t->pc + i->uimm & XWORD_MAX;
	if (i->rd) t->ireg[i->rd] = out;
}

static void execute(struct hart *t, const struct insn *i) {
	switch (i->opcode) {
	case 0x13: aluint(t, i); break;
	case 0x17: lui(t, i); break;
	case 0x33: alu(t, i); break;
	case 0x37: lui(t, i); break;
	default: abort(); /* FIXME */
	}
}

/* FIXME move to a separate frontend */
#include <inttypes.h>
#include <stdio.h>

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
	struct hart t = {0};

	for (;;) {
		printf("  pc=0x%" PRIXXWORD "\n", t.pc);
		for (size_t i = 0; i < sizeof irname / sizeof irname[0]; i += 4) {
			printf("%*.*s=0x%" PRIXXWORD " %*.*s=0x%" PRIXXWORD " "
			       "%*.*s=0x%" PRIXXWORD " %*.*s=0x%" PRIXXWORD "\n",
			       (int)sizeof irname[0], (int)sizeof irname[0],
			       irname[i],   t.ireg[i],
			       (int)sizeof irname[0], (int)sizeof irname[0],
			       irname[i+1], t.ireg[i+1],
			       (int)sizeof irname[0], (int)sizeof irname[0],
			       irname[i+2], t.ireg[i+2],
			       (int)sizeof irname[0], (int)sizeof irname[0],
			       irname[i+3], t.ireg[i+3]);
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

		execute(&t, &i);
	}
}
