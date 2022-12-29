#include <stdint.h>

#include "riscv.h"
#include "rvexec.h"
#include "rvinsn.h"

static void super(struct hart *t, uint_least32_t i) {
	switch (iimm(i)) {
	case 0x000: if (!rs1(i) && !rd(i)) { ecall(t); break; }
	case 0x302: if (!rs1(i) && !rd(i)) { /* FIXME mret */ break; }
	default: illins(t, i);
	}
}

static void hyper(struct hart *t, uint_least32_t i) {
	illins(t, i);
}

__attribute__((alias("sys"))) execute_t exec1C;

static void sys(struct hart *t, uint_least32_t i) {
	xword_t in1 = funct3(i) & 4 ? rs1(i) : t->ireg[rs1(i)],
	        nul = 0, *out = rd(i) ? &t->ireg[rd(i)] : &nul;
	unsigned csr = iimm(i) & 0xFFF;
	switch (funct3(i)) {
	case 0: super(t, i); break;
	case 4: hyper(t, i); break;
	case 1: case 5: csrxchg(t, out, csr, in1); break;
	case 2: case 6: if (rs1(i)) csrxset(t, out, csr, in1); else csrread(t, out, csr); break;
	case 3: case 7: if (rs1(i)) csrxclr(t, out, csr, in1); else csrread(t, out, csr); break;
	}
}
