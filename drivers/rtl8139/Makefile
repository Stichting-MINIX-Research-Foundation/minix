# Makefile for the Realtek RTL8139 ethernet driver (RTL8139)
DRIVER = rtl8139

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = $u/src/drivers

# programs, flags, etc.
MAKE = exec make
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lutils -ltimers

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
install:	/usr/sbin/drivers/$(DRIVER)
/usr/sbin/drivers/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak 


# dependencies
a = 	$d/drivers.h $b/interrupt.h $b/bios.h \
	$i/ansi.h $i/string.h $i/limits.h $i/stddef.h $i/errno.h \
	$m/config.h $m/type.h $m/com.h $m/callnr.h $m/const.h $s/types.h \
	$m/syslib.h $s/types.h \
	$m/utils.h $m/devio.h
l =	$d/libpci/pci.h $d/libpci/pci.c $d/libpci/pci_table.c 


rtl8139.o:	$a $l	

$(LIBPCI):	$a $l

