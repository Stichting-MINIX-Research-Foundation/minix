# Makefile for the AT disk driver (AT_WINI)
DRIVER = at_wini

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = ..
p = ../libpci

# programs, flags, etc.
MAKE = exec make
CC =	exec cc
CFLAGS = -I$i $(CPROFILE)
LDFLAGS = -i -L../libdriver
LIBS = -ldriver -lsys -ltimers 

OBJ = at_wini.o liveupdate.o


# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 32k $(DRIVER)

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

