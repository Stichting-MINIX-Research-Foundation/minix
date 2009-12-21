# Makefile for Centronics printer driver (PRINTER)
DRIVER = printer

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = ..

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i $(CPROFILE)
LDFLAGS = -i
LIBS =  -lsys

OBJ = printer.o liveupdate.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install $(DRIVER)

# install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -c $? $@
#	install -o root -cs $? $@

# clean up local files
clean:
	rm -f *.o *.bak $(DRIVER)

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

