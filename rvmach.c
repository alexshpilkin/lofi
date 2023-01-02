#include <stdint.h>

#include "riscv.h"

__attribute__((visibility("hidden"), weak))
extern char __mies[] __asm__("__mies"), __misa[] __asm__("__misa");

void ecall(struct hart *t) {
	trap(t, MECALL, 0);
}

void mret(struct hart *t) {
	struct mhart *m = (struct mhart *)t;
	m->hart.nextpc = m->mepc;
	if (m->mstatus & MSTATUS_MPIE)
		m->mstatus |=  MSTATUS_MIE;
	else
		m->mstatus &= ~MSTATUS_MIE;
	m->mstatus |= MSTATUS_MPIE;
}

void trap(struct hart *t, xword_t cause, xword_t value) {
	struct mhart *m = (struct mhart *)t;
	m->mcause = cause; m->mtval = value;
	m->mepc = m->hart.pc; m->hart.nextpc = m->mtvec;
	if (m->mstatus & MSTATUS_MIE)
		m->mstatus |=  MSTATUS_MPIE;
	else
		m->mstatus &= ~MSTATUS_MPIE;
	m->mstatus &= ~MSTATUS_MIE;
}

void csrread(struct hart *t, xword_t *out, unsigned csr) {
	struct mhart *m = (struct mhart *)t;
	switch (csr) {
	case MSTATUS:    *out = m->mstatus | MSTATUS_MPP; break;
	case MISA:       *out = XWORD_BIT / XWORD_C(32) << XWORD_BIT - 2 | (uintptr_t)&__misa; break;
	case MIE:        *out = m->mie; break;
	case MTVEC:      *out = m->mtvec; break;
#if XWORD_BIT < 64
	case MSTATUSH:   *out = 0; break;
#endif
	case MSCRATCH:   *out = m->mscratch; break;
	case MEPC:       *out = xalign(t, m->mepc); break;
	case MCAUSE:     *out = m->mcause; break;
	case MTVAL:      *out = m->mtval; break;
	case MIP:        *out = m->pending; break;
	case MVENDORID:  *out = 0; break;
	case MARCHID:    *out = 0; break;
	case MIMPID:     *out = 0; break;
	case MHARTID:    *out = 0; break;
	case MCONFIGPTR: *out = 0; break;
	default:         trap(t, ILLINS, 0); break;
	}
}

void csrwrite(struct hart *t, unsigned csr, xword_t val) {
	struct mhart *m = (struct mhart *)t;
	switch (csr) {
	case MSTATUS:  m->mstatus = val & (MSTATUS_MIE | MSTATUS_MPIE);
	case MISA:     break;
	case MIE:      m->mie = val & (uintptr_t)&__mies; break;
	case MTVEC:    m->mtvec = val & ~XWORD_C(3); break;
#if XWORD_BIT < 64
	case MSTATUSH: break;
#endif
	case MSCRATCH: m->mscratch = val; break;
	case MEPC:     m->mepc = val; break;
	case MCAUSE:   m->mcause = val; break;
	case MTVAL:    m->mtval = val; break;
	case MIP:      break; /* see funky RMW semantics before changing */
	default:       trap(t, ILLINS, 0); return;
	}
}

void csrxchg(struct hart *t, xword_t *out, unsigned csr, xword_t val) {
	csrread(t, out, csr); csrwrite(t, csr, val);
}
void csrxset(struct hart *t, xword_t *out, unsigned csr, xword_t val) {
	csrread(t, out, csr); csrwrite(t, csr, *out | val);
}
void csrxclr(struct hart *t, xword_t *out, unsigned csr, xword_t val) {
	csrread(t, out, csr); csrwrite(t, csr, *out & ~val);
}
