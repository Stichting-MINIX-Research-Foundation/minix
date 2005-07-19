# Makefile for PCI bus library

# Directories
u = /usr
i = $u/include
s = $i/sys
b = $i/ibm
m = $i/minix

# Programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lsysutil

OBJECTS = pci.o pci_table.o 

all build install: $(OBJECTS)	

# $(CC) -c $@ $(LDFLAGS) $(OBJ) $(LIBS)

clean:
	rm -f *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

