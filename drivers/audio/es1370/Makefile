# Makefile for the ES1371 sounddriver (SB16)

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
gen_drv_dir = ../../gen_drivers/cyclic_dma

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys

# build local binary
all:	es1370

es1370:	es1370.o ak4531.o audio_fw.o liveupdate.o pci_helper.o
	$(CC) -o $@ $(LDFLAGS) es1370.o ak4531.o audio_fw.o liveupdate.o pci_helper.o $(LIBS)

audio_fw.o liveupdate.o: ../framework/audio_fw.c ../framework/liveupdate.c ../framework/audio_fw.h
	$(CC) -c ../framework/audio_fw.c ../framework/liveupdate.c
	
install: /usr/sbin/es1370
/usr/sbin/es1370:	es1370
	install -o root -S 1024k -c $? $@

# clean up local files
clean:
	rm -f *.o *.bak core es1370

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

