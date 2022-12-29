#include <stdint.h>
#include <stdlib.h> /* FIXME */

#include "riscv.h"
#include "rvexec.h"
#include "rvinsn.h"

static void super(struct hart *t, uint_least32_t i) {
	switch (iimm(i)) {
	case 0x000: if (!rs1(i) && !rd(i)) { ecall(t); break; }
	case 0x302: if (!rs1(i) && !rd(i)) { /* FIXME mret */ break; }
	default: abort(); /* FIXME */
	}
}

static void hyper(struct hart *t, uint_least32_t i) {
	abort(); /* FIXME */
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

__attribute__((weak)) execute_t OPCODE(exec);

void execute(struct hart *t, uint_least32_t i) {
	static execute_t *const exec[0x20] = { OPCODE(&exec) };
	execute_t *impl = i & 3 != 3 ? 0 /* FIXME */ : exec[opcode(i) >> 2];
	if (impl) (*impl)(t, i); else abort(); /* FIXME */
}
