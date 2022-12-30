printf '.PHONY: check clean\n'
for file in "$1"/isa/rv$2[mu][im]-p-*; do
	case $file in *.*|*-breakpoint|*-ma_data|*-scall|*-zicntr) continue;; esac
	name=${file##*/}
	printf '\n'
	printf 'check: check-%s\n' $name
	printf 'check-%s: %s.ref %s.dut\n' $name $name $name
	printf '\tcmp -s %s.ref %s.dut\n' $name $name
	printf '%s.ref: $(TESTS)%s\n' $name ${file#"$1"}
	printf '\t$(SPIKE) --isa=rv%sg +signature=$@ +signature-granularity=1 %s\n' $2 "$file"
	printf '\ttouch $@\n'
	printf '%s.dut: %s rvui\n' $name "$file"
	printf '\t./rvui %s > $@ || { s=$$?; rm -f $@; exit $$s; }\n' "$file"
	printf 'clean: clean-%s\n' $name
	printf 'clean-%s:\n' $name
	printf '\trm -f %s.ref %s.dut\n' $name $name
done
