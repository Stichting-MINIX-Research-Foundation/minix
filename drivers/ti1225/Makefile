# Makefile for the Texas Instruments PCI1225 PC Card controller driver (ti1225)
DRIVER = ti1225

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
LIBS = -lsys -ltimers

OBJ = ti1225.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) 
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 8k $(DRIVER)

# install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f *.o *.bak $(DRIVER)

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

