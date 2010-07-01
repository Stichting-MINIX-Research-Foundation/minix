# Makefile for ramdisk image

PROGRAMS=at_wini bios_wini cdprobe dev2name floppy loadramdisk newroot \
	pci sh service sysenv mfs
EXTRA=system.conf passwd rs.single

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
	$(MAKE) -C ../at_wini

bios_wini: ../bios_wini/bios_wini
	install -s ../$@/$@ $@

../bios_wini/bios_wini:
	$(MAKE) -C ../bios_wini

floppy: ../floppy/floppy
	install -s ../$@/$@ $@

../floppy/floppy:
	$(MAKE) -C ../floppy

pci: ../pci/pci
	install -s ../$@/$@ $@

../pci/pci:
	$(MAKE) -C ../pci

cdprobe:  ../../commands/cdprobe/cdprobe
	install -s ../../commands/$@/$@ $@

../../commands/cdprobe:
	$(MAKE) -C ../../commands/cdprobe

dev2name:  ../../commands/dev2name/dev2name
	install -s ../../commands/$@/$@ $@

../../commands/dev2name/dev2name:
	$(MAKE) -C ../../commands/dev2name

loadramdisk:  ../../commands/loadramdisk/loadramdisk
	install -s ../../commands/$@/$@ $@

../../commands/loadramdisk/loadramdisk:
	$(MAKE) -C ../../commands/loadramdisk

newroot:  ../../commands/newroot/newroot
	install -s ../../commands/$@/$@ $@

../../commands/newroot/newroot:
	$(MAKE) -C ../../commands/newroot

sysenv:  ../../commands/sysenv/sysenv
	install -s ../../commands/$@/$@ $@

../../commands/sysenv:
	$(MAKE) -C ../../commands/sysenv

sh:	../../commands/ash/sh
	install -s ../../commands/ash/$@ $@

../../commands/ash/sh:
	$(MAKE) -C ../../commands/ash sh

service: ../../commands/service/service
	install -s ../../commands/service/$@ $@

../../commands/service:
	$(MAKE) -C ../../commands/service

mfs: ../../servers/mfs/mfs
	install -s ../../servers/mfs/$@ $@

../../servers/mfs/mfs:
	$(MAKE) -C ../../servers/mfs

system.conf: ../../etc/system.conf
	install -s ../../etc/$@ $@

passwd: ../../etc/passwd
	install -s ../../etc/$@ $@

rs.single: ../../etc/rs.single
	install -s ../../etc/$@ $@

proto.gen: $(MAKEDEV) proto.sh proto
	sh -e proto.sh >proto.gen

.include <bsd.prog.mk>
