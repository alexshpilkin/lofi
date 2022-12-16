.PHONY: all clean

CFLAGS = -g -O2

all: riscv
clean:
	rm -f riscv riscv.o
install:
	mkdir -p $(DESTDIR)$(prefix)/bin
	install -m 755 riscv $(DESTDIR)$(prefix)/bin
