# Makefile for memory driver (MEMORY)
DRIVER = memory

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
LIBS = -lsys -lutils

OBJ = memory.o 
LIBDRIVER = $d/libdriver/driver.o


# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) $(LIBDRIVER)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBDRIVER) $(LIBS)
	install -S 64w $(DRIVER)

$(LIBDRIVER): 
	cd $d/libdriver && $(MAKE) 

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
l =	$d/libdriver/driver.h $d/libdriver/driver.c $m/partition.h $m/u64.h 


memory.o:	$a $l	

$(LIBDRIVER):	$a $l
$(LIBDRIVER):	$s/ioc_disk.h

