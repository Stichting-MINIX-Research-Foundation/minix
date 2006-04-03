# Makefile for ramdisk image

PROGRAMS=at_wini bios_wini cdprobe dev2name floppy loadramdisk newroot \
	pci sh service sysenv

MAKEDEV=/usr/bin/MAKEDEV

all:	image.c image.s

clean:
	rm -rf $(PROGRAMS) bintoc image image.c t proto.gen

image.c:	bintoc image
	./bintoc -o $@ image

image.s:	image.c
	sed < image.c > $@ 's/^/.data1 /;s/,$$//' || { rm -f $@; false; }

# Note for cross compilation: this executable has to be compiled for the
# host system
bintoc:	bintoc.c
	$(CC) -o $@ bintoc.c

image:	proto.gen mtab rc $(PROGRAMS)
	mkfs -B 2048 image proto.gen || { rm -f image; false; }

at_wini: ../../at_wini/at_wini
	install -s ../../$@/$@ $@

../../at_wini/at_wini:
	cd ../../at_wini && make

bios_wini: ../../bios_wini/bios_wini
	install -s ../../$@/$@ $@

../../bios_wini/bios_wini:
	cd ../../bios_wini && make

floppy: ../../floppy/floppy
	install -s ../../$@/$@ $@

../../floppy/floppy:
	cd ../../floppy && make

pci: ../../pci/pci
	install -s ../../$@/$@ $@

../../pci/pci:
	cd ../../pci && make

cdprobe:  ../../../commands/simple/cdprobe
	install -s ../../../commands/simple/$@ $@

../../../commands/simple/cdprobe: 
	cd ../../../commands/simple && make cdprobe

dev2name:  ../../../commands/simple/dev2name
	install -s ../../../commands/simple/$@ $@

../../../commands/simple/dev2name: 
	cd ../../../commands/simple && make dev2name

loadramdisk:  ../../../commands/simple/loadramdisk
	install -s ../../../commands/simple/$@ $@

../../../commands/simple/loadramdisk: 
	cd ../../../commands/simple && make loadramdisk

newroot:  ../../../commands/simple/newroot
	install -s ../../../commands/simple/$@ $@

../../../commands/simple/newroot: 
	cd ../../../commands/simple && make newroot

sysenv:  ../../../commands/simple/sysenv
	install -s ../../../commands/simple/$@ $@

../../../commands/simple/sysenv: 
	cd ../../../commands/simple && make sysenv

sh:	../../../commands/ash/sh
	install -s ../../../commands/ash/$@ $@

../../../commands/ash/sh: 
	cd ../../../commands/ash && make sh

service: ../../../servers/rs/service
	install -s ../../../servers/rs/$@ $@

../../../servers/rs/service: 
	cd ../../../servers/rs && make service

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

proto.gen: $(MAKEDEV) proto.sh proto
	sh -e proto.sh >proto.gen

# Include generated dependencies.
include .depend

