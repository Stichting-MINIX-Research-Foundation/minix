# Makefile for the Sound Blaster 16 driver (SB16)
DRIVER = sb16_dsp

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = ..

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lsysutil

OBJ = sb16_dsp.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
#	install -S 256w $(DRIVER)

# install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -c $? $@
#	install -o root -cs $? $@

# clean up local files
clean:
	rm -f *.o *.bak $(DRIVER)

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

