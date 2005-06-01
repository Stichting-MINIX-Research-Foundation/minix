# Makefile for terminal driver (TTY)
DRIVER = tty

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = $u/src/drivers

# programs, flags, etc.
MAKE = exec make
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lutils -ltimers

OBJ = tty.o console.o vidcopy.o keyboard.o pty.o rs232.o 

# build local binary 
all build:	$(DRIVER)
	cd keymaps && $(MAKE) -$(MAKEFLAGS) $@
$(DRIVER):	$(OBJ) 
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 256w $(DRIVER)

# install with other drivers
install:	/usr/sbin/drivers/$(DRIVER)
	cd keymaps && $(MAKE) -$(MAKEFLAGS) $@
/usr/sbin/drivers/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	cd keymaps && $(MAKE) -$(MAKEFLAGS) $@
	rm -f $(DRIVER) *.o *.bak 



# dependencies
a = 	$d/drivers.h $b/interrupt.h $b/bios.h \
	$i/ansi.h $i/string.h $i/limits.h $i/stddef.h $i/errno.h \
	$m/config.h $m/type.h $m/com.h $m/callnr.h $m/const.h $s/types.h \
	$m/syslib.h $s/types.h \
	$m/utils.h $m/devio.h

tty.o:	tty.h $a 	
console.o: tty.h $a
vidcopy.o: 	# nothing
keyboard.o: tty.h $a
rs232.o: tty.h $a $i/termios.h $i/signal.h 



