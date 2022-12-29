.PHONY: all check clean

CFLAGS = -g -O2
MFLAGS = SPIKE=$(SPIKE) TESTS=$(TESTS) XLEN=$(XLEN)
SPIKE  = spike
TESTS  = riscv-tests
XLEN   = 32

all: repl rvui
repl: repl.o rvbase.o rvexec.o rvmult.o
rvui: rvui.o rvbase.o rvexec.o rvmult.o
rvui.o: elf.h
repl.o rvbase.o rvexec.o rvmult.o rvui.o: riscv.h
rvbase.o rvexec.o rvmult.o: rvexec.h
repl.o rvbase.o rvexec.o rvmult.o: rvinsn.h
check: check.mk rvui
	+$(MAKE) $(MFLAGS) -f check.mk $@
check.mk: check.sh $(TESTS)
	+$(SHELL) ./check.sh $(TESTS) $(XLEN) > $@
clean: clean-check
	rm -f repl rvui repl.o rvbase.o rvexec.o rvmult.o rvui.o check.mk
clean-check: check.mk
	+$(MAKE) $(MFLAGS) -f check.mk clean
install:
	mkdir -p $(DESTDIR)$(prefix)/bin
	install -m 755 repl rvui $(DESTDIR)$(prefix)/bin

.c.o:
	$(CC) -DXWORD_BIT=$(XLEN) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
