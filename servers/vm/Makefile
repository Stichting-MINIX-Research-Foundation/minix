# Makefile for VM server
SERVER = vm

include /etc/make.conf

OBJ = main.o alloc.o utility.o exec.o exit.o fork.o break.o \
	signal.o vfs.o mmap.o slaballoc.o region.o pagefaults.o addravl.o \
	physravl.o rs.o queryexit.o map_mem.o
ARCHOBJ =  $(ARCH)/vm.o $(ARCH)/pagetable.o $(ARCH)/arch_pagefaults.o $(ARCH)/util.o 

CPPFLAGS=-I../../kernel/arch/$(ARCH)/include -I$(ARCH)
CFLAGS = $(CPROFILE) $(CPPFLAGS)

# build local binary

all build install:	$(SERVER)

$(SERVER):	$(OBJ) phony
	cd $(ARCH) && $(MAKE)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(ARCHOBJ)  -lsys  

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 
	cd $(ARCH) && $(MAKE) $@

depend: 
	cd $(ARCH) && $(MAKE) $@
	mkdep "$(CC) -E $(CPPFLAGS)" *.c $(ARCH)/*.c > .depend

phony:
	@:

# Include generated dependencies.
include .depend

