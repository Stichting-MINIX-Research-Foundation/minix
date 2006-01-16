#Generated from ./gnu/end/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../../generate.sh ./gnu/end ../../obj-ack ../../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-gnu: ../../obj-gnu/end.a

../../obj-gnu/end.a: ../../obj-gnu/./gnu/end/gnu_end.o

../../obj-gnu/end.a:
	gar cr ../../obj-gnu/end.a $?

../../obj-gnu/./gnu/end/gnu_end.o: gnu_end.gs
	gas -o ../../obj-gnu/./gnu/end/gnu_end.o gnu_end.gs




clean::
	rm -f ../../obj-gnu/./gnu/end/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
