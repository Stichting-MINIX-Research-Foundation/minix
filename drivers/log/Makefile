# Makefile for log driver
DRIVER = log

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
d = ..

# programs, flags, etc.
MAKE = exec make
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lsysutil

OBJ = log.o diag.o kputc.o
LIBDRIVER = $d/libdriver/driver.o


# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) $(LIBDRIVER)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBDRIVER) $(LIBS)
	install -S 4kb $(DRIVER)

$(LIBDRIVER): 
	cd $d/libdriver && $(MAKE) 

# install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak 


depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c ../libdriver/*.c > .depend

# Include generated dependencies.
include .depend

