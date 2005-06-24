# Makefile for system library implementation

# Directories
u = /usr
i = $u/include

# Programs, flags, etc.
CC =	exec cc
CPP =	$l/cpp
LD =	$(CC) -.o
CFLAGS = -I$i
LDFLAGS = -i

SYS = 	clock.o copying.o debugging.o devio.o irqctl.o proctl.o \
	sysctl.o misc.o sigctl.o tracing.o 

# What to make.
all build: $(SYS)

clean:
	rm -f *.a *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend



