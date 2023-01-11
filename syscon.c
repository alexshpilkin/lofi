#include <stdint.h>
#include <stdlib.h>

#include "riscv.h"
#include "rvexec.h"

extern wbk_t *wbk; /* FIXME */
static unsigned char syscon[4];

static void sysconwbk(struct hart *t) {
	xword_t v = syscon[0]        |
	  ((xword_t)syscon[1] <<  8) |
	  ((xword_t)syscon[2] << 16) |
	  ((xword_t)syscon[3] << 24);

	syscon[0] = syscon[1] = syscon[2] = syscon[3] = 0;

	switch (v) {
	case 0x5555: exit(EXIT_SUCCESS);
	case 0x7777: /* FIXME restart */
	default:     abort();
	}
}

static unsigned char *sysconmap(struct hart *t, xword_t addr, xword_t size, int type) {
	addr &= 0xFFFFF;
	if (addr != 0x00000 || size != 4) return 0;
	syscon[0] = syscon[1] = syscon[2] = syscon[3] = 0;
	if (type >= MAPA) wbk = &sysconwbk;
	return &syscon[0];
}

__attribute__((alias("sysconmap"))) map_t meg111;
