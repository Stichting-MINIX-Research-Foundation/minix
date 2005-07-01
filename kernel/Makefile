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
OBJS =	start.o protect.o klibc.o klib.o table.o main.o proc.o \
	i8259.o exception.o system.o clock.o misc.o 
SYS_OBJS = $s/proctl.o $s/copying.o $s/devio.o $s/sysctl.o $s/misc.o \
           $s/sigctl.o $s/tracing.o $s/clock.o $s/irqctl.o $s/debugging.o \
           $s/priority.o
LIBS = -ltimers


# What to make.
kernel build: $(HEAD) $(OBJS) $(SYS_OBJS) 
	$(LD) $(CFLAGS) $(LDFLAGS) -o kernel $(HEAD) $(OBJS) $(SYS_OBJS) $(LIBS)
	install -S 0 kernel

$(SYS_OBJS):
	cd system && $(MAKE)

all install:
	cd system && $(MAKE) -$(MAKEFLAGS) $@

clean:
	cd system && $(MAKE) -$(MAKEFLAGS) $@
	rm -f *.o *.bak kernel

depend: 
	cd system && $(MAKE) -$(MAKEFLAGS) $@
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

