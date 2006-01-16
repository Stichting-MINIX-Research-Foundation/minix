#Generated from ./end/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./end ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../obj-ack/end.a

../obj-ack/end.a: ../obj-ack/end.a(edata.o)
../obj-ack/end.a: ../obj-ack/end.a(em_end.o)
../obj-ack/end.a: ../obj-ack/end.a(end.o)
../obj-ack/end.a: ../obj-ack/end.a(etext.o)

../obj-ack/end.a:
	ar cr ../obj-ack/end.a ../obj-ack/./end/*.o
	rm ../obj-ack/./end/*.o

../obj-ack/end.a(edata.o): edata.s
	cc  -c -o ../obj-ack/./end/edata.o edata.s
../obj-ack/end.a(em_end.o): em_end.s
	cc  -c -o ../obj-ack/./end/em_end.o em_end.s
../obj-ack/end.a(end.o): end.s
	cc  -c -o ../obj-ack/./end/end.o end.s
../obj-ack/end.a(etext.o): etext.s
	cc  -c -o ../obj-ack/./end/etext.o etext.s

all-gnu: ../obj-gnu/end.a

../obj-gnu/end.a: ../obj-gnu/./end/edata.o
../obj-gnu/end.a: ../obj-gnu/./end/em_end.o
../obj-gnu/end.a: ../obj-gnu/./end/end.o
../obj-gnu/end.a: ../obj-gnu/./end/etext.o

../obj-gnu/end.a:
	gar cr ../obj-gnu/end.a $?

../obj-gnu/./end/edata.o: edata.s
	gcc  -E -x assembler-with-cpp -I. edata.s | asmconv -mi386 ack gnu > ../obj-gnu/./end/edata.s.gnu || true
	gas -o ../obj-gnu/./end/edata.o ../obj-gnu/./end/edata.s.gnu

../obj-gnu/./end/em_end.o: em_end.s
	gcc  -E -x assembler-with-cpp -I. em_end.s | asmconv -mi386 ack gnu > ../obj-gnu/./end/em_end.s.gnu || true
	gas -o ../obj-gnu/./end/em_end.o ../obj-gnu/./end/em_end.s.gnu

../obj-gnu/./end/end.o: end.s
	gcc  -E -x assembler-with-cpp -I. end.s | asmconv -mi386 ack gnu > ../obj-gnu/./end/end.s.gnu || true
	gas -o ../obj-gnu/./end/end.o ../obj-gnu/./end/end.s.gnu

../obj-gnu/./end/etext.o: etext.s
	gcc  -E -x assembler-with-cpp -I. etext.s | asmconv -mi386 ack gnu > ../obj-gnu/./end/etext.s.gnu || true
	gas -o ../obj-gnu/./end/etext.o ../obj-gnu/./end/etext.s.gnu




clean::
	rm -f ../obj-ack/./end/*
	rm -f ../obj-gnu/./end/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
