#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "elf.h"
#include "riscv.h"

static const char *name;
static unsigned char *restrict elf; static size_t len;

static uint_least8_t elfb(size_t off) {
	if (off >= len) {
		fprintf(stderr, "%s: ELF truncated\n", name);
		exit(EXIT_FAILURE);
	}
	return elf[off] & 0xFF;
}

static uint_least16_t elfh(size_t off) {
	return elfb(off) | (uint_least16_t)elfb(off + 1) << 8;
}

static uint_least32_t elfw(size_t off) {
	return elfh(off) | (uint_least32_t)elfh(off + 2) << 16;
}

#if XWORD_BIT == 32
#define elfx elfw
#define ELFCLASS ELFCLASS32
#endif

int main(int argc, char **argv) {
	const char *err;
	FILE *fp; size_t lim = 0;

	if (argc != 2) {
		fprintf(stderr, "usage: %s ELF\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (!(fp = fopen(name = argv[1], "rb"))) {
		perror(name); return EXIT_FAILURE;
	}
	for (;;) {
		if (len == lim) {
			unsigned char *mem;
			size_t siz = lim ? lim * 2 : BUFSIZ;
			if (siz < lim || !(mem = realloc(elf, siz))) {
				perror(argv[0]);
				exit(EXIT_FAILURE);
			}
			elf = mem; lim = siz;
		}
		len += fread(elf + len, 1, lim - len, fp);
		if (len < lim) break;
	}
	if (ferror(fp)) {
		perror(name); return EXIT_FAILURE;
	}
	fclose(fp);

	err = "bad ELF magic";
	if (len < EI_NIDENT ||
	    elf[EI_MAG0] != ELFMAG0 || elf[EI_MAG1] != ELFMAG1 ||
	    elf[EI_MAG2] != ELFMAG2 || elf[EI_MAG3] != ELFMAG3)
	{ goto bad; }
	err = "bad ELF architecture";
	if (elf[EI_CLASS] != ELFCLASS ||
	    elf[EI_DATA] != ELFDATA2LSB)
	{ goto bad; }
	err = "bad ELF header version";
	if (elf[EI_VERSION] != EV_CURRENT) goto bad;
	err = "bad ELF type";
	if (elfh(EI_NIDENT) != ET_EXEC) goto bad;
	err = "bad ELF machine";
	if (elfh(EI_NIDENT + 2) != EM_RISCV) goto bad;
	err = "bad ELF version";
	if (elfw(EI_NIDENT + 4) != EV_CURRENT) goto bad;

	xword_t entry = elfx(EI_NIDENT + 8),
	        phoff = elfx(EI_NIDENT + 8 + XWORD_BIT/8),
	        shoff = elfx(EI_NIDENT + 8 + XWORD_BIT/8 * 2);
	enum { OFFSET = EI_NIDENT + 12 + XWORD_BIT/8 * 3 };
	uint_least16_t ehsize    = elfh(OFFSET),
	               phentsize = elfh(OFFSET +  2),
	               phnum     = elfh(OFFSET +  4),
	               shentsize = elfh(OFFSET +  6),
	               shnum     = elfh(OFFSET +  8);
	err = "bad ELF header";
	if (ehsize < OFFSET + 12) goto bad;

	return EXIT_SUCCESS;

bad:
	fprintf(stderr, "%s: %s\n", name, err);
	return EXIT_FAILURE;
}
