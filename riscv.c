#include <stdint.h>

#define MASK(L, H)    ((UINT32_C(1) << (H) - 1 << 1) - (UINT32_C(1) << (L)))
#define BITS(X, L, H) ((X) & MASK(L, H) >> (L))

struct insn {
	uint_least8_t opcode;
};

static struct insn decode(uint_least32_t x) {
	struct insn i = {0};
	i.opcode = BITS(x, 0, 7);
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

int main(int argc, char **argv) {
	uint_least32_t x;
	while (scanf("%" SCNxLEAST32, &x) >= 1) {
		struct insn i = decode(x);
		char opcode[8];
		printf("%s\n", binary(opcode, sizeof opcode, i.opcode));
	}
}
