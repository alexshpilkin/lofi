.PHONY: all clean

CFLAGS = -g -O2

all: repl
repl: repl.o riscv.o
repl.o riscv.o: riscv.h
clean:
	rm -f repl repl.o riscv.o
install:
	mkdir -p $(DESTDIR)$(prefix)/bin
	install -m 755 repl $(DESTDIR)$(prefix)/bin
