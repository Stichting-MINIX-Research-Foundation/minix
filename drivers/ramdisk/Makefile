# Makefile for ramdisk image

PROGRAMS=at_wini bios_wini cdprobe dev2name floppy loadramdisk newroot \
	pci sh service sysenv mfs
EXTRA=system.conf passwd

CPPFLAGS+= -I${MINIXSRCDIR}/servers
MAKEDEV=/usr/bin/MAKEDEV

install: all

all:	image.c

clean:
	rm -rf $(PROGRAMS) $(EXTRA) bintoc image image.c t proto.gen

image.c:	bintoc image
	./bintoc -o $@ image

# Note for cross compilation: this executable has to be compiled for the
# host system
bintoc:	bintoc.c
	$(CC) -o $@ bintoc.c

image:	proto.gen mtab rc $(PROGRAMS) $(EXTRA)
	mkfs image proto.gen || { rm -f image; false; }

at_wini: ../at_wini/at_wini
	install -s ../$@/$@ $@

../at_wini/at_wini:
	cd ../at_wini && make

bios_wini: ../bios_wini/bios_wini
	install -s ../$@/$@ $@

../bios_wini/bios_wini:
	cd ../bios_wini && make

floppy: ../floppy/floppy
	install -s ../$@/$@ $@

../floppy/floppy:
	cd ../floppy && make

pci: ../pci/pci
	install -s ../$@/$@ $@

../pci/pci:
	cd ../pci && make

cdprobe:  ../../commands/cdprobe/cdprobe
	install -s ../../commands/$@/$@ $@

../../commands/cdprobe:
	cd ../../commands/cdprobe && make

dev2name:  ../../commands/dev2name/dev2name
	install -s ../../commands/$@/$@ $@

../../commands/dev2name/dev2name:
	cd ../../commands/dev2name && make

loadramdisk:  ../../commands/loadramdisk/loadramdisk
	install -s ../../commands/$@/$@ $@

../../commands/loadramdisk/loadramdisk:
	cd ../../commands/loadramdisk && make

newroot:  ../../commands/newroot/newroot
	install -s ../../commands/$@/$@ $@

../../commands/newroot/newroot:
	cd ../../commands/newroot && make

sysenv:  ../../commands/sysenv/sysenv
	install -s ../../commands/$@/$@ $@

../../commands/sysenv:
	cd ../../commands/sysenv && make

sh:	../../commands/ash/sh
	install -s ../../commands/ash/$@ $@

../../commands/ash/sh:
	cd ../../commands/ash && make sh

service: ../../commands/service/service
	install -s ../../commands/service/$@ $@

../../commands/service:
	cd ../../commands/service && make

mfs: ../../servers/mfs/mfs
	install -s ../../servers/mfs/$@ $@

../../servers/mfs/mfs:
	cd ../../servers/mfs && make

system.conf: ../../etc/system.conf
	install -s ../../etc/$@ $@

passwd: ../../etc/passwd
	install -s ../../etc/$@ $@

proto.gen: $(MAKEDEV) proto.sh proto
	sh -e proto.sh >proto.gen

.include <minix.prog.mk>
