# Makefile for the CMOS driver 
DRIVER = cmos

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

OBJ = cmos.o 
LIBDRIVER = $d/libdriver/driver.o


# build local binary
all build:	$(DRIVER) 
$(DRIVER):	$(OBJ) $(LIBDRIVER) 
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBDRIVER) $(LIBS)
	install -S 1024w $(DRIVER)

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

