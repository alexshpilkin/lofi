.PHONY: all check clean

CFLAGS = -g -O2
MFLAGS = SPIKE=$(SPIKE) TESTS=$(TESTS) XLEN=$(XLEN)
SPIKE  = spike
TESTS  = riscv-tests
XLEN   = 32

all: mini repl rvui
mini: mini.o rvatom.o rvbase.o rvcsrs.o rvexec.o rvmach.o rvmies.ld rvmisa.ld rvmult.o syscon.o
repl: repl.o rvbase.o rvcsrs.o rvexec.o rvmult.o
rvui: rvui.o rvatom.o rvbase.o rvcsrs.o rvexec.o rvmach.o rvmisa.ld rvmult.o
rvui.o: elf.h
mini.o repl.o rvatom.o rvbase.o rvcsrs.o rvexec.o rvmach.o rvmult.o rvui.o syscon.o: riscv.h
mini.o rvatom.o rvbase.o rvcsrs.o rvexec.o rvmult.o syscon.o: rvexec.h
repl.o rvatom.o rvbase.o rvcsrs.o rvexec.o rvmult.o: rvinsn.h
check: check.mk rvui
	+$(MAKE) $(MFLAGS) -f check.mk $@
check.mk: check.sh $(TESTS)
	+$(SHELL) ./check.sh $(TESTS) $(XLEN) > $@
clean: clean-check
	rm -f mini repl rvui mini.o repl.o rvatom.o rvbase.o rvcsrs.o rvexec.o rvmach.o rvmult.o rvui.o syscon.o check.mk
clean-check: check.mk
	+$(MAKE) $(MFLAGS) -f check.mk clean
install:
	mkdir -p $(DESTDIR)$(prefix)/bin
	install -m 755 mini repl rvui $(DESTDIR)$(prefix)/bin

.c.o:
	$(CC) -DXWORD_BIT=$(XLEN) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
