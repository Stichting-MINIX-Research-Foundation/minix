# Makefile for Centronics printer driver (PRINTER)
DRIVER = printer

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
LIBS = -lsys -lutils

OBJ = printer.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 64w $(DRIVER)

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
	$m/utils.h $m/devio.h

printer.o:	$a

