#Generated from ./ack/liby/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../generate.sh ./ack/liby ../../obj-ack ../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../../obj-ack/liby.a

../../obj-ack/liby.a: ../../obj-ack/liby.a(main.o)
../../obj-ack/liby.a: ../../obj-ack/liby.a(yyerror.o)

../../obj-ack/liby.a:
	ar cr ../../obj-ack/liby.a ../../obj-ack/./ack/liby/*.o
	rm ../../obj-ack/./ack/liby/*.o

../../obj-ack/liby.a(main.o): main.c
	cc -O -D_MINIX -D_POSIX_SOURCE -wo -c -o ../../obj-ack/./ack/liby/main.o main.c
../../obj-ack/liby.a(yyerror.o): yyerror.c
	cc -O -D_MINIX -D_POSIX_SOURCE -wo -c -o ../../obj-ack/./ack/liby/yyerror.o yyerror.c



clean::
	rm -f ../../obj-ack/./ack/liby/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
