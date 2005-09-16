# Makefile for the Sound Blaster 16 driver (SB16)

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


# build local binary
all build:	sb16_dsp sb16_mixer
sb16_dsp:	sb16.o sb16_dsp.o
	$(CC) -o $@ $(LDFLAGS) sb16.o sb16_dsp.o $(LIBS)
sb16_mixer:	sb16.o sb16_mixer.o
	$(CC) -o $@ $(LDFLAGS) sb16.o sb16_mixer.o $(LIBS)

# install with other drivers
install:	/usr/sbin/sb16_dsp /usr/sbin/sb16_mixer
/usr/sbin/sb16_dsp:	sb16_dsp
	install -o root -c $? $@
/usr/sbin/sb16_mixer:	sb16_mixer
	install -o root -c $? $@

# clean up local files
clean:
	rm -f *.o *.bak sb16 sb16_dsp sb16_mixer

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

