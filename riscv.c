#include <stdint.h>

typedef uint_least32_t xword_t;

#define MASK(L, H)    ((UINT32_C(1) << (H) - 1 << 1) - (UINT32_C(1) << (L)))
#define BITS(X, L, H) (((X) & MASK(L, H)) >> (L))

struct insn {
	uint_least8_t opcode, funct3, funct7, rs1, rs2, rd;
	xword_t iimm, simm, bimm, uimm;
};

static struct insn decode(uint_least32_t x) {
	struct insn i = {0};

	i.opcode = BITS(x,  0,  7); i.uimm = x & ~MASK(0, 12);
	i.rd     = BITS(x,  7, 12);
	i.funct3 = BITS(x, 12, 15);
	i.rs1    = BITS(x, 15, 20);
	i.rs2    = BITS(x, 20, 25); i.iimm = (BITS(x, 20, 32) ^ 1 << 11) - (1 << 11);
	i.funct7 = BITS(x, 25, 32);

	i.simm = i.iimm & ~MASK(0, 5) | i.rd;
	i.bimm = i.simm & ~(1 | MASK(11, 12)) | (i.simm & 1) << 11;

	return i;
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
	uint_least32_t x;
	while (scanf("%" SCNxLEAST32, &x) >= 1) {
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
		 */

		printf("%s %s %s rd=%u (%.*s) rs1=%u (%.*s) rs2=%u (%.*s)\n"
		       "\tI = 0x%" PRIXLEAST32 "\n"
		       "\tS = 0x%" PRIXLEAST32 "\n"
		       "\tB = 0x%" PRIXLEAST32 "\n"
		       "\tU = 0x%" PRIXLEAST32 "\n",
		       binary(opcode, sizeof opcode, i.opcode),
		       binary(funct3, sizeof funct3, i.funct3),
		       binary(funct7, sizeof funct7, i.funct7),
		       (unsigned)i.rd,  (int)sizeof irname[0], irname[i.rd],
		       (unsigned)i.rs1, (int)sizeof irname[0], irname[i.rs1],
		       (unsigned)i.rs2, (int)sizeof irname[0], irname[i.rs2],
		       i.iimm, i.simm, i.bimm, i.uimm);
	}
}
