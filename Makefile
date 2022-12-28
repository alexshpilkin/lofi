.PHONY: all check clean

CFLAGS = -g -O2
MFLAGS = SPIKE=$(SPIKE) TESTS=$(TESTS) XLEN=$(XLEN)
SPIKE  = spike
TESTS  = riscv-tests
XLEN   = 32

all: repl rvui
repl: repl.o riscv.o
rvui: rvui.o riscv.o
rvui.o: elf.h
repl.o riscv.o rvui.o: riscv.h
repl.o riscv.o: rvinsn.h
check: check.mk rvui
	+$(MAKE) $(MFLAGS) -f check.mk $@
check.mk: check.sh $(TESTS)
	+$(SHELL) ./check.sh $(TESTS) $(XLEN) > $@
clean: clean-check
	rm -f repl rvui repl.o riscv.o rvui.o check.mk
clean-check: check.mk
	+$(MAKE) $(MFLAGS) -f check.mk clean
install:
	mkdir -p $(DESTDIR)$(prefix)/bin
	install -m 755 repl rvui $(DESTDIR)$(prefix)/bin

.c.o:
	$(CC) -DXWORD_BIT=$(XLEN) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
