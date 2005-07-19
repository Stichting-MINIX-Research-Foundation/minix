# Makefile for the Realtek RTL8139 ethernet driver (RTL8139)
DRIVER = rtl8139

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = ..

# programs, flags, etc.
MAKE = exec make
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lsysutil -ltimers

OBJ = rtl8139.o 
LIBPCI = $d/libpci/pci.o $d/libpci/pci_table.o


# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) $(PCI)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBPCI) $(LIBS)
	install -S 50kw $(DRIVER)

$(PCI): 
	cd $d/libpci && $(MAKE) 

# install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c ../libpci/*.c > .depend

# Include generated dependencies.
include .depend

