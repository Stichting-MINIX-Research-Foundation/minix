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
all build:	sb16	

sb16:	sb16.o mixer.o audio_fw.o
	$(CC) -o $@ $(LDFLAGS) sb16.o mixer.o audio_fw.o $(LIBS)

audio_fw.o:	../framework/audio_fw.c ../framework/audio_fw.h
	$(CC) -c ../framework/audio_fw.c

# install with other drivers
install:	/usr/sbin/sb16
/usr/sbin/sb16:	sb16
	install -o root -S 512k -c $? $@

# clean up local files
clean:
	rm -f *.o *.bak sb16 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

