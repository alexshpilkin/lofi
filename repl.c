#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "riscv.h"
#include "rvinsn.h"

struct cpu {
	struct hart hart;
	unsigned char *restrict image;
	size_t size;
};

unsigned char *map(struct hart *t, xword_t addr, xword_t size, int type) {
	struct cpu *c = (struct cpu *)t;
	if (addr & size - 1) switch (type) {
	case MAPR: trap(t, RALIGN, addr); return 0;
	case MAPW: case MAPA: trap(t, WALIGN, addr); return 0;
	case MAPX: break; /* handled by higher-level code */
	}
	if (addr >= c->size || size > c->size - addr) switch (type) {
	case MAPR: trap(t, RACCES, addr); return 0;
	case MAPW: case MAPA: trap(t, WACCES, addr); return 0;
	case MAPX: trap(t, XACCES, addr); return 0;
	}
	return &c->image[addr];
}

void unmap(struct hart *t) {
	(void)t;
}

void trap(struct hart *t, xword_t cause, xword_t value) {
	abort(); /* FIXME */
}

void ecall(struct hart *t) {
	abort(); /* FIXME */
}

void mret(struct hart *t) {
	abort(); /* FIXME */
}

void csrread(struct hart *t, xword_t *out, unsigned csr) {
	abort(); /* FIXME */
}
void csrxchg(struct hart *t, xword_t *out, unsigned csr, xword_t val) {
	abort(); /* FIXME */
}
void csrxset(struct hart *t, xword_t *out, unsigned csr, xword_t val) {
	abort(); /* FIXME */
}
void csrxclr(struct hart *t, xword_t *out, unsigned csr, xword_t val) {
	abort(); /* FIXME */
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
	unsigned char image[128] = {0};
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

		uint_least32_t i;
		if (scanf("%" SCNxLEAST32, &i) < 1) break;

		char op[8], f3[4], f7[8];

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
		       binary(op, sizeof op, opcode(i)),
		       binary(f3, sizeof f3, funct3(i)),
		       binary(f7, sizeof f7, funct7(i)),
		       (unsigned)rd(i),  (int)sizeof irname[0], irname[rd(i)],
		       (unsigned)rs1(i), (int)sizeof irname[0], irname[rs1(i)],
		       (unsigned)rs2(i), (int)sizeof irname[0], irname[rs2(i)],
		       iimm(i), simm(i), bimm(i), uimm(i), jimm(i));

		xword_t nextpc = c.hart.nextpc;
		execute(&c.hart, i);
		if (c.hart.lr) c.hart.ireg[c.hart.lr] = nextpc;
		unmap(&c.hart);
		c.hart.lr = 0;
		c.hart.pc = c.hart.nextpc;
	}
}
