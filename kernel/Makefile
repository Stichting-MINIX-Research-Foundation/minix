# Makefile for kernel

# Directories
u = /usr
i = $u/include
l = $u/lib
s = system

# Programs, flags, etc.
CC =	exec cc
CPP =	$l/cpp
LD =	$(CC) -.o
CFLAGS = -I$i 
LDFLAGS = -i 

HEAD =	mpx.o
OBJS =	start.o protect.o klib.o table.o main.o proc.o \
	i8259.o exception.o system.o clock.o utility.o debug.o
SYSTEM = system.a
LIBS = -ltimers 


# What to make.
all: build 
kernel build install: $(HEAD) $(OBJS) 
	cd system && $(MAKE) -$(MAKEFLAGS) $@
	$(LD) $(CFLAGS) $(LDFLAGS) -o kernel \
	$(HEAD) $(OBJS) \
	$(SYSTEM) $(LIBS)
	install -S 0 kernel

clean:
	cd system && $(MAKE) -$(MAKEFLAGS) $@
	rm -f *.a *.o *~ *.bak kernel

depend: 
	cd system && $(MAKE) -$(MAKEFLAGS) $@
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c *.s > .depend

# Include generated dependencies.
klib.o:	klib386.s klib88.s
mpx.o:	mpx386.s mpx88.s
include .depend

