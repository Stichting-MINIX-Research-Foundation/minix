# Makefile for Intel Pro/100 driver (FXP)
DRIVER = fxp

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = ..

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i 
LIBS = -lsys -lsysutil -ltimers

OBJ = fxp.o mii.o
LIBPCI = $d/libpci/pci.o $d/libpci/pci_table.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) $(LIBPCI)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBPCI) $(LIBS)
	install -S 256w $(DRIVER)

$(LIBPCI): 
	cd $d/libpci && $(MAKE) 

# install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f *.o *.bak $(DRIVER)

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c ../libpci/*.c > .depend

# Include generated dependencies.
include .depend

