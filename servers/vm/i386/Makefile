
include /etc/make.conf

OBJ = vm.o pagetable.o arch_pagefaults.o util.o

CPPFLAGS=-I../../../kernel/arch/$(ARCH)/include -I.
CFLAGS = $(CPROFILE) $(CPPFLAGS)

all:	$(OBJ)

clean:
	rm -f $(OBJ)

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

