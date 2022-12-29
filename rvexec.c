#include <stdint.h>

#include "riscv.h"
#include "rvexec.h"
#include "rvinsn.h"

__attribute__((weak)) execute_t OPCODE(exec);

void execute(struct hart *t, uint_least32_t i) {
	static execute_t *const exec[0x20] = { OPCODE(&exec) };
	execute_t *impl = (i & 3) != 3 ? 0 /* FIXME */ : exec[opcode(i) >> 2];
	(*(impl ? impl : &illins))(t, i);
}
