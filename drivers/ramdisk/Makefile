# Makefile for ramdisk image

.include <bsd.own.mk>

PROGRAMS= at_wini bios_wini cdprobe dev2name floppy loadramdisk mount \
	pci procfs sh service sysenv mfs fsck.mfs
SCRIPTS=newroot

.if ${MKSMALL} != "yes"
PROGRAMS+= ahci ext2
.endif

.if ${MKACPI} != "no"
PROGRAMS+= acpi
.endif

.if ${MKSMALL} != "yes"
PROTO= proto
.else
PROTO= proto.small
.endif

EXTRA=system.conf master.passwd passwd pwd.db spwd.db rs.single

CPPFLAGS+= -I${MINIXSRCDIR}/servers -I${MINIXSRCDIR}
CLEANFILES += $(PROGRAMS) $(SCRIPTS) $(EXTRA) image image.c t proto.gen

install: all

realall: image

image:	proto.gen mtab rc $(EXTRA)
	mkfs.mfs image proto.gen || { rm -f image; false; }
	if fsck.mfs -s image | grep -q CLEAN;	\
	then	: ;	\
	else	echo "CLEAN sanity check of image failed."	;	\
		echo "(Perhaps install current mkfs and fsck.)" ;	\
		rm -f image; false;             \
	fi

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

fsck.mfs:  ../../commands/fsck.mfs/fsck.mfs
	install ${STRIPFLAG} ../../commands/$@/$@ $@

../../commands/fsck.mfs/fsck.mfs:
	$(MAKE) -C ../../commands/fsck.mfs

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

passwd: ../../etc/master.passwd ../../usr.sbin/pwd_mkdb/pwd_mkdb
	rm -f ../../etc/master.passwd.orig ../../etc/passwd.orig
	rm -f ../../etc/pwd.db.tmp ../../etc/spwd.db.tmp
	../../usr.sbin/pwd_mkdb/pwd_mkdb -V 0 -p -d ../../ ../../etc/master.passwd
	install ${STRIPFLAG} ../../etc/$@ $@

../../usr.sbin/pwd_mkdb/pwd_mkdb:
	$(MAKE) -C ../../usr.sbin/pwd_mkdb

master.passwd: ../../etc/master.passwd
	install ${STRIPFLAG} ../../etc/$@ $@

pwd.db: passwd
	install ${STRIPFLAG} ../../etc/$@ $@

spwd.db: passwd
	install ${STRIPFLAG} ../../etc/$@ $@

rs.single: ../../etc/rs.single
	install ${STRIPFLAG} ../../etc/$@ $@

proto.gen: $(PROGRAMS) $(SCRIPTS) proto.sh ${PROTO}
	for p in $(PROGRAMS); do file $$p | grep -q ELF && strip -s $$p || true; done
	sh -e proto.sh ${PROTO} >proto.gen

.include <minix.service.mk>
