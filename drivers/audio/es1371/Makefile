# Makefile for the ES1371 sounddriver (SB16)

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
pci_dir = ../../libpci
gen_drv_dir = ../../gen_drivers/cyclic_dma

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lsysutil 
PCI = $(pci_dir)/pci.o $(pci_dir)/pci_table.o

# build local binary
all:	es1371

es1371:	es1371.o SRC.o codec.o wait.o audio_fw.o $(PCI)
	$(CC) -o $@ $(LDFLAGS) es1371.o SRC.o codec.o wait.o audio_fw.o $(PCI) $(LIBS)

audio_fw.o: ../framework/audio_fw.c ../framework/audio_fw.h
	$(CC) -c ../framework/audio_fw.c
	
install: /usr/sbin/es1371
/usr/sbin/es1371:	es1371
	install -o root -S 1024k -c $? $@

$(PCI): 
	cd $(pci_dir) && $(MAKE)

# clean up local files
clean:
	rm -f *.o *.bak core es1371

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

