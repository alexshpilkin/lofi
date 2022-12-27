.PHONY: all clean

CFLAGS = -g -O2

all: repl rvui
repl: repl.o riscv.o
rvui: rvui.o riscv.o
rvui.o: elf.h
repl.o riscv.o rvui.o: riscv.h
clean:
	rm -f repl rvui repl.o riscv.o rvui.o
install:
	mkdir -p $(DESTDIR)$(prefix)/bin
	install -m 755 repl rvui $(DESTDIR)$(prefix)/bin
