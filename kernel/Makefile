# Makefile for kernel

include /etc/make.conf

# Directories
u = /usr
i = $u/include
l = $u/lib
s = system
a = arch/$(ARCH)

# Programs, flags, etc.
CC =	exec cc
CPP =	$l/cpp
LD =	$(CC) -.o
CPPFLAGS=-I$i -I$a/include
CFLAGS=$(CPROFILE) $(CPPFLAGS) $(EXTRA_OPTS)
LDFLAGS=-i 

# first-stage, arch-dependent startup code
HEAD =	head.o
FULLHEAD = $a/$(HEAD)

OBJS =	start.o table.o main.o proc.o \
	system.o clock.o utility.o debug.o profile.o interrupt.o
SYSTEM = system.a
ARCHLIB = $a/$(ARCH).a
LIBS = -ltimers -lsysutil

# What to make.
all: build 
kernel build install: $(HEAD) $(OBJS) 
	cd system && $(MAKE) $@
	cd $a && $(MAKE) $@
	$(LD) $(CFLAGS) $(LDFLAGS) -o kernel $(FULLHEAD) $(OBJS) \
		$(SYSTEM) $(ARCHLIB) $(LIBS)
	install -S 0 kernel

clean:
	cd system && $(MAKE) -$(MAKEFLAGS) $@
	cd $a && $(MAKE) -$(MAKEFLAGS) $@
	rm -f *.a *.o *~ *.bak kernel

depend: 
	cd system && $(MAKE) -$(MAKEFLAGS) $@
	cd $a && $(MAKE) $@
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# How to build it
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

$(HEAD): 
	cd $a && make HEAD=$(HEAD) $(HEAD)

# Include generated dependencies.
include .depend
