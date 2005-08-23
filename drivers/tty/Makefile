# Makefile for terminal driver (TTY)
DRIVER = tty

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
LIBS = -lsys -lsysutil -ltimers

OBJ = tty.o console.o vidcopy.o keyboard.o pty.o rs232.o 

# build local binary 
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) 
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 256w $(DRIVER)

# install with other drivers
install:	/usr/sbin/$(DRIVER)
	cd keymaps && $(MAKE) -$(MAKEFLAGS) install
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	cd keymaps && $(MAKE) -$(MAKEFLAGS) $@
	rm -f $(DRIVER) *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend


