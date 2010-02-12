# Makefile for the floppy disk driver (FLOPPY)
DRIVER = floppy

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
CFLAGS = -I$i $(CPROFILE)
LDFLAGS = -i -L../libdriver
LIBS = -ldriver -lsys  -ltimers

OBJ = floppy.o liveupdate.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 16k $(DRIVER)

# install with other drivers
install:	/sbin/$(DRIVER)
/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c ../libdriver/*.c > .depend

# Include generated dependencies.
include .depend

