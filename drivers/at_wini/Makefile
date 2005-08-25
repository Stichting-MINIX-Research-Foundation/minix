# Makefile for the AT disk driver (AT_WINI)
DRIVER = at_wini

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = ..
p = ../libpci

# programs, flags, etc.
MAKE = exec make
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsysutil -lsys -ltimers

OBJ = at_wini.o 
LIBDRIVER = $d/libdriver/driver.o $d/libdriver/drvlib.o 
LIBPCI = $p/pci.o $p/pci_table.o


# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) $(LIBDRIVER) $(LIBPCI)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBDRIVER) $(LIBS) $(LIBPCI)
	install -S 8k $(DRIVER)

$(LIBPCI):
	cd $p && $(MAKE)

$(LIBDRIVER): 
	cd $d/libdriver && $(MAKE) 

# install with other drivers
install:	/sbin/$(DRIVER)
/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c ../libdriver/*.c > .depend

# Include generated dependencies.
include .depend

