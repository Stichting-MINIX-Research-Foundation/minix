# Makefile for Intel Pro/100 driver (FXP)
DRIVER = fxp

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = $u/src/drivers

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i 
LIBS = -lsys -lutils -ltimers

OBJ = fxp.o mii.o
LIBPCI = $d/libpci/pci.o $d/libpci/pci_table.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) $(LIBPCI)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBPCI) $(LIBS)
	install -S 64w $(DRIVER)

$(LIBPCI): 
	cd $d/libpci && $(MAKE) 

# install with other drivers
install:	/usr/sbin/drivers/$(DRIVER)
/usr/sbin/drivers/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f *.o *.bak $(DRIVER)

# Dependencies
a = 	$d/drivers.h $b/interrupt.h $b/bios.h \
	$i/ansi.h $i/string.h $i/limits.h $i/stddef.h $i/errno.h \
	$m/config.h $m/type.h $m/com.h $m/callnr.h $m/const.h $s/types.h \
	$m/syslib.h $s/types.h \
	$m/utils.h $m/serverassert.h $m/devio.h

fxp.o:	$a

