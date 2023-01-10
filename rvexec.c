#include <stdint.h>

#include "riscv.h"
#include "rvexec.h"
#include "rvinsn.h"

static void illins(struct hart *t, uint_least32_t i) {
	trap(t, ILLINS, i);
}

__attribute__((alias("illins"), weak)) execute_t HEX5(exec);

void execute(struct hart *t, uint_least32_t i) {
	static execute_t *const exec[0x20] = { HEX5(&exec) };
	execute_t *impl = (i & 3) != 3 ? &illins /* FIXME */ : exec[opcode(i) >> 2];
	(*impl)(t, i);
}
