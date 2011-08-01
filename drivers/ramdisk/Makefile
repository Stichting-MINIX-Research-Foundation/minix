# Makefile for ramdisk image

.include <bsd.own.mk>

PROGRAMS= at_wini bios_wini cdprobe dev2name floppy loadramdisk mount \
	pci procfs sh service sysenv mfs
SCRIPTS=newroot

.if ${MKSMALL} != "yes"
PROGRAMS+= ahci ext2
.endif

# acpi is not compiled with ack
.if ${COMPILER_TYPE} == "gnu" && ${MKACPI} != "no"
PROGRAMS+= acpi
.endif

.if ${MKSMALL} != "yes"
PROTO= proto
.else
PROTO= proto.small
.endif

.if ${OBJECT_FMT} == "a.out"
STRIPFLAG+= -s
.endif

EXTRA=system.conf passwd rs.single

CPPFLAGS+= -I${MINIXSRCDIR}/servers -I${MINIXSRCDIR}
CLEANFILES += $(PROGRAMS) $(SCRIPTS) $(EXTRA) bintoc image image.c t proto.gen

install: all

realall: image.c

image.c:	bintoc image
	./bintoc -o $@ image

# Note for cross compilation: this executable has to be compiled for the
# host system
bintoc:	bintoc.c
	$(CC) -o $@ bintoc.c

image:	proto.gen mtab rc $(EXTRA)
	mkfs.mfs image proto.gen || { rm -f image; false; }

ahci: ../ahci/ahci
	install ${STRIPFLAG} ../$@/$@ $@

../ahci/ahci:
	$(MAKE) -C ../ahci

at_wini: ../at_wini/at_wini
	install ${STRIPFLAG} ../$@/$@ $@

../at_wini/at_wini:
	$(MAKE) -C ../at_wini

bios_wini: ../bios_wini/bios_wini
	install ${STRIPFLAG} ../$@/$@ $@

../bios_wini/bios_wini:
	$(MAKE) -C ../bios_wini

floppy: ../floppy/floppy
	install ${STRIPFLAG} ../$@/$@ $@

../floppy/floppy:
	$(MAKE) -C ../floppy

acpi: ../acpi/acpi
	install ${STRIPFLAG} ../$@/$@ $@

../acpi/acpi:
	$(MAKE) -C ../acpi

pci: ../pci/pci
	install ${STRIPFLAG} ../$@/$@ $@

../pci/pci:
	$(MAKE) -C ../pci

cdprobe:  ../../commands/cdprobe/cdprobe
	install ${STRIPFLAG} ../../commands/$@/$@ $@

../../commands/cdprobe:
	$(MAKE) -C ../../commands/cdprobe

dev2name:  ../../commands/dev2name/dev2name
	install ${STRIPFLAG} ../../commands/$@/$@ $@

../../commands/dev2name/dev2name:
	$(MAKE) -C ../../commands/dev2name

loadramdisk:  ../../commands/loadramdisk/loadramdisk
	install ${STRIPFLAG} ../../commands/$@/$@ $@

../../commands/loadramdisk/loadramdisk:
	$(MAKE) -C ../../commands/loadramdisk

mount:  ../../commands/mount/mount
	install ${STRIPFLAG} ../../commands/$@/$@ $@

../../commands/mount/mount:
	$(MAKE) -C ../../commands/mount

newroot:  ../../commands/newroot/newroot.sh
	install ${STRIPFLAG} ../../commands/$@/$@.sh $@

../../commands/newroot/newroot:
	$(MAKE) -C ../../commands/newroot

sysenv:  ../../commands/sysenv/sysenv
	install ${STRIPFLAG} ../../commands/$@/$@ $@

../../commands/sysenv/sysenv:
	$(MAKE) -C ../../commands/sysenv

sh:	../../commands/ash/sh
	install ${STRIPFLAG} ../../commands/ash/$@ $@

../../commands/ash/sh:
	$(MAKE) -C ../../commands/ash sh

service: ../../commands/service/service
	install ${STRIPFLAG} ../../commands/service/$@ $@

../../commands/service/service:
	$(MAKE) -C ../../commands/service

mfs: ../../servers/mfs/mfs
	install ${STRIPFLAG} ../../servers/mfs/$@ $@

../../servers/mfs/mfs:
	$(MAKE) -C ../../servers/mfs

procfs: ../../servers/procfs/procfs
	install ${STRIPFLAG} ../../servers/procfs/$@ $@

../../servers/procfs/procfs:
	$(MAKE) -C ../../servers/procfs

ext2: ../../servers/ext2/ext2
	install ${STRIPFLAG} ../../servers/ext2/$@ $@

../../servers/ext2/ext2:
	$(MAKE) -C ../../servers/ext2

system.conf: ../../etc/system.conf
	install ${STRIPFLAG} ../../etc/$@ $@

passwd: ../../etc/passwd
	install ${STRIPFLAG} ../../etc/$@ $@

rs.single: ../../etc/rs.single
	install ${STRIPFLAG} ../../etc/$@ $@

proto.gen: $(PROGRAMS) $(SCRIPTS) proto.sh ${PROTO}
	sh -e proto.sh ${PROTO} >proto.gen

.if ${OBJECT_FMT} == "ELF"
proto.gen: elfstrip

.PHONY: elfstrip

elfstrip: $(PROGRAMS)
	strip -s $(PROGRAMS)
.endif

.include <minix.service.mk>
