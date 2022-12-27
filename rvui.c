#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf.h"
#include "riscv.h"

static const char *name;
static unsigned char *restrict elf; static size_t len;

static unsigned char elfb(size_t off) {
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

static size_t elfz(size_t off) {
	xword_t x = elfx(off);
	if (x > SIZE_MAX) {
		fprintf(stderr, "%s: ELF too large\n", name);
		exit(EXIT_FAILURE);
	}
	return x;
}

struct cpu {
	struct hart hart;
	unsigned char *image;
	xword_t base, size;
};

/* FIXME should use when loading */
unsigned char *map(struct hart *t, xword_t addr, xword_t size) {
	struct cpu *c = (struct cpu *)t;
	if (addr & size - 1) abort(); /* FIXME */
	if (addr - c->base >= c->size || (addr - c->base + size & XWORD_MAX) >= c->size) abort();
	return &c->image[addr - c->base];
}

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
			if (siz < lim || !(mem = realloc(elf, siz))) goto oom;
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

	xword_t entry = elfx(EI_NIDENT + 8);
	size_t  phoff = elfz(EI_NIDENT + 8 + XWORD_BIT/8),
	        shoff = elfz(EI_NIDENT + 8 + XWORD_BIT/8 * 2);
	enum { OFFSET = EI_NIDENT + 12 + XWORD_BIT/8 * 3 };
	size_t ehsize    = elfh(OFFSET),
	       phentsize = elfh(OFFSET +  2),
	       phnum     = elfh(OFFSET +  4),
	       shentsize = elfh(OFFSET +  6),
	       shnum     = elfh(OFFSET +  8);
	err = "bad ELF header";
	if (ehsize < OFFSET + 12) goto bad;

	struct cpu c = {0};
	c.hart.pc = entry;

	for (size_t i = 0; i < phnum; i++) {
		size_t base = phoff + i*phentsize;
		uint_least32_t type = elfw(base);
		size_t  offset = elfz(base + XWORD_BIT/8);
		xword_t vaddr  = elfx(base + XWORD_BIT/8 * 2);
		size_t  filesz = elfz(base + XWORD_BIT/8 * 4),
		        memsz  = elfz(base + XWORD_BIT/8 * 5);

		err = "bad ELF segment type";
		if (type == PT_NULL || type == PT_NOTE || type == PT_RISCV_ATTRIBUTES)
			continue;
		if (type != PT_LOAD && type != PT_PHDR)
			goto bad;

		if (!c.base) {
			c.base = vaddr; c.size = memsz;
			if (!(c.image = malloc(c.size))) goto oom;
		}
		if (c.base > vaddr) {
			c.base = vaddr; c.size += c.base - vaddr;
			if (!(c.image = realloc(c.image, c.size))) goto oom;
		}
		if (c.base + c.size < vaddr + memsz) {
			c.size = vaddr + memsz - c.base;
			if (!(c.image = realloc(c.image, c.size))) goto oom;
		}
		if (filesz) {
			elfb(offset + filesz - 1); /* probe */
			memcpy(&c.image[vaddr-c.base], &elf[offset], filesz);
		}
		memset(&c.image[vaddr-c.base+filesz], 0, memsz - filesz);
	}

	size_t symoff, symsize = 0, symentsize, strndx = 0;
	for (size_t i = 0; i < shnum; i++) {
		size_t base = shoff + i*shentsize;
		if (elfw(base + 4) == SHT_SYMTAB) {
			symoff     = elfz(base + 8 + XWORD_BIT/8 * 2);
			symsize    = elfz(base + 8 + XWORD_BIT/8 * 3);
			strndx     = elfw(base + 8 + XWORD_BIT/8 * 4);
			symentsize = elfz(base + 16 + XWORD_BIT/8 * 5);
			break;
		}
	}

	size_t stroff, strsize = 0;
	if (strndx && strndx < shnum) {
		size_t base = shoff + strndx*shentsize;
		err = "bad ELF string table";
		if (elfw(base + 4) != SHT_STRTAB) goto bad;
		stroff  = elfz(base + 8 + XWORD_BIT/8 * 2);
		strsize = elfz(base + 8 + XWORD_BIT/8 * 3);
		if (elfb(stroff + strsize - 1)) goto bad; /* FIXME overflow */
	}

	xword_t sigbeg = 0, sigend = 0;
	for (size_t base = symoff; base < symoff + symsize; base += symentsize) {
		uint32_t name = elfw(base);
		xword_t value = elfx(base + XWORD_BIT/8);
		err = "bad ELF symbol table";
		if (name >= strsize) goto bad;
		if (!strcmp((char *)&elf[stroff+name], "begin_signature"))
			sigbeg = value;
		else if (!strcmp((char *)&elf[stroff+name], "end_signature"))
			sigend = value;
	}

	for (;;) {
		xword_t pc = c.hart.pc;
		unsigned char *ip = map(&c.hart, pc, 4);
		uint_least32_t x = ip[0]       |
		   (uint_least16_t)ip[1] <<  8 |
		   (uint_least32_t)ip[2] << 16 |
		   (uint_least32_t)ip[3] << 24;
		c.hart.nextpc = pc + 4 & XWORD_MAX;

		struct insn i = decode(x);
		execute(&c.hart, &i);
		if (c.hart.nextpc & 3) abort(); /* FIXME imprecise */
		c.hart.pc = c.hart.nextpc;
	}

	(void)sigbeg; (void)sigend; /* FIXME unreachable */

	return EXIT_SUCCESS;

bad:
	fprintf(stderr, "%s: %s\n", name, err);
	return EXIT_FAILURE;
oom:
	perror(argv[0]);
	return EXIT_FAILURE;
}
