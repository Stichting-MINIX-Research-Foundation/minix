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
LIBS = -lsys -lsysutil

OBJ = memory.o imgrd.o
LIBDRIVER = $d/libdriver/driver.o


# build local binary
all build:	$(DRIVER)

$(DRIVER):	ramdisk_image $(OBJ) $(LIBDRIVER)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBDRIVER) $(LIBS)
	install -S 1024w $(DRIVER)

imgrd.o:        ramdisk/image.c

$(LIBDRIVER): 
	cd $d/libdriver && $(MAKE) 

ramdisk_image:
	cd ramdisk && make
	binsizes big

# install with other drivers
install: /usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak 
	cd ramdisk && make clean

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c ../libdriver/*.c > .depend
	cd ramdisk && make depend

# Include generated dependencies.
include .depend

