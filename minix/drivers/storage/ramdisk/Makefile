# Makefile for ramdisk image
.include <bsd.own.mk>

USE_BITCODE:=no

install:

all:

# Add a few defines we are going to use during the image
# creation to determine what features and binaries to include
# in the final image
# ACPI            do we have/include the acpi binary
# RAMDISK_SMALL   is the script called with MKSMALL=yes
# DYNAMIC         does the ramdisk contain dynamic binaries?
RAMDISK_INC_ACPI=	0
RAMDISK_SMALL=		0
RAMDISK_DYNAMIC=	0

RAMDISK_DEFINES= \
	-DRAMDISK_INC_ACPI=${RAMDISK_INC_ACPI} \
	-DRAMDISK_SMALL=${RAMDISK_SMALL} \
	-DRAMDISK_DYNAMIC=${RAMDISK_DYNAMIC}

# The name of the proto file to use
PROTO=			proto

# Common to all architectures
ETC=			system.conf group
EXTRA=			rc
PROTO_FILES=		proto.common.etc
PROTO_FILES+=		proto.common.dynamic
PROGRAMS=		# defined
PROGRAMS+=		fsck_mfs
dir.fsck_mfs:=	minix/commands/fsck.mfs
PROGRAMS+=		grep
dir.grep:=	minix/commands/grep
PROGRAMS+=		input
dir.input:=	minix/servers/input
PROGRAMS+=		loadramdisk
dir.loadramdisk:=	minix/commands/loadramdisk
PROGRAMS+=		mfs
dir.mfs:=	minix/fs/mfs
PROGRAMS+=		mount
dir.mount:=	minix/commands/mount
PROGRAMS+=		procfs
dir.procfs:=	minix/fs/procfs
PROGRAMS+=		service
dir.service:=	minix/commands/service
PROGRAMS+=		sh
dir.sh:=	bin/sh
PROGRAMS+=		sysenv
dir.sysenv:=	minix/commands/sysenv
PROGRAMS+=		umount
dir.umount:=	minix/commands/umount

.if ${MKSMALL} != "yes"
RAMDISK_SMALL=		1
.endif

.if ${MACHINE_ARCH} == "i386"
ETC+=			rs.single
PROGRAMS+=		at_wini
dir.at_wini:=	minix/drivers/storage/at_wini
PROGRAMS+=		floppy
dir.floppy:=	minix/drivers/storage/floppy
PROGRAMS+=		pci
dir.pci:=	minix/drivers/bus/pci
PROGRAMS+=		pckbd
dir.pckbd:=	minix/drivers/hid/pckbd
PROGRAMS+=		cdprobe
dir.cdprobe:=	minix/commands/cdprobe
PROGRAMS+=		pwd_mkdb
dir.pwd_mkdb:=	usr.sbin/pwd_mkdb

.if ${MKSMALL} != "yes"
PROGRAMS+=		ahci
dir.ahci:=	minix/drivers/storage/ahci
PROGRAMS+=		virtio_blk
dir.virtio_blk:= minix/drivers/storage/virtio_blk
PROGRAMS+=		ext2
dir.ext2:=	minix/fs/ext2
.endif

.if ${MKACPI} != "no"
RAMDISK_INC_ACPI= 1
PROGRAMS+=		acpi
dir.acpi:=	minix/drivers/power/acpi
.endif
.endif # ${MACHINE_ARCH} == "i386"

.if ${MACHINE_ARCH} == "earm"
PROGRAMS+=		mmc
dir.mmc:=	minix/drivers/storage/mmc
.endif # ${MACHINE_ARCH} == "earm"

.if ${LDSTATIC} == "-dynamic"
RAMDISK_DYNAMIC=	 1
PROGRAMS+=		ld.elf_so
PROG_LIBS+=		libc
PROG_MINIXLIBS+=	libterminfo
.endif

CPPFLAGS+= -I${NETBSDSRCDIR}/servers

# LSC We have to take care of not erasing the source file, so never add EXTRA
# to CLEANFILES
CLEANFILES += ${PROGRAMS} ${SCRIPTS} ${ETC} image image.c
CLEANFILES += proto.gen proto.dev proto.dev.mtree
CLEANFILES += ${PROG_LIBEXEC}
CLEANFILES += ${.OBJDIR}/etc/*

#############################################################
# LSC Below this point the rules should not be modified
#############################################################

# Tool to bootstrap the password db
TOOL_PWD_MKDB?= ${NETBSDSRCDIR}/usr.sbin/pwd_mkdb/pwd_mkdb

# Remove "drivers/storage/ramdisk" component from path
PROGROOT:= ${.OBJDIR:S,/minix/drivers/storage/ramdisk,,}

# Generate dependencies rules for config files
.for etc in ${ETC}
etc/${etc}: ${NETBSDSRCDIR}/etc/${etc}
	mkdir -p ${.OBJDIR}/etc
	${INSTALL} $> $@
.endfor

# LSC Force the regeneration of the proto file as it seems sometimes 
# they are not copied over as needed.
# LSC ramdisk rc file != /etc/rc
# BJG if ${.CURDIR} == ${.OBJDIR}, we are building in-tree and install
# shouldn't try to install the originals over the originals. Otherwise
# we are building out-of-tree and the contents should be copied
# unconditionally in case the contents have changed after the first copy.
.if ${.CURDIR} != ${.OBJDIR}
.for file in ${EXTRA} ${PROTO} ${PROTO_FILES}
CLEANFILES += ${.OBJDIR}/${file}
${file}: ${NETBSDSRCDIR}/minix/drivers/storage/ramdisk/${file} .PHONY
	${INSTALL} $> $@
.endfor
.endif

# Generate dependencies rules for dynamic libraries, if needed
.for lib in ${PROG_LIBS}
PROGRAMS+= ${lib}.so.0
CLEANFILES += ${lib}.so.0
${lib}.so.0:${PROGROOT}/lib/${lib}/${lib}.so.0
	${INSTALL} $> $@

# BJG - Don't invoke parallel Makes
#${PROGROOT}/lib/${lib}/${lib}.so.0:
#	${MAKE} -C ${NETBSDSRCDIR}/lib/${lib} all
.endfor

.for lib in ${PROG_MINIXLIBS}
PROGRAMS+= ${lib}.so.0
CLEANFILES += ${lib}.so.0
${lib}.so.0:${PROGROOT}/minix/lib/${lib}/${lib}.so.0
	${INSTALL} $> $@

# BJG - Don't invoke parallel Makes
#${PROGROOT}/minix/lib/${lib}/${lib}.so.0:
#	${MAKE} -C ${NETBSDSRCDIR}/minix/lib/${lib} all
.endfor

# Generate dependencies rules for binaries
.for prog in ${PROGRAMS}
${prog}: ${PROGROOT}/${dir.${prog}}/${prog}
	${INSTALL} $> $@

# BJG - don't invoke parallel Makes
#${PROGROOT}/${dir.${prog}}/${prog}:
#	${MAKE} -C ${NETBSDSRCDIR}/${dir.${prog}} all
.endfor # prog

realall image:	proto.gen ${ETC:C/^/etc\//} ${EXTRA} \
		etc/master.passwd etc/pwd.db etc/spwd.db etc/passwd etc/group
	${_MKMSG_CREATE} "Generating ramdisk image"
	${TOOL_MKFSMFS} image proto.gen || { rm -f image; false; }

etc/pwd.db etc/spwd.db etc/passwd: etc/master.passwd

etc/master.passwd: ${NETBSDSRCDIR}/etc/master.passwd
	mkdir -p ${.OBJDIR}/etc
	${INSTALL} $> $@
	${TOOL_PWD_MKDB} -V 0 -p -d . etc/master.passwd

proto.dev.mtree:
	@echo ". type=dir uname=root gname=operator mode=0755" \
		>${.TARGET}.tmp && \
	echo "./dev type=dir uname=root gname=operator mode=0755" \
		>>${.TARGET}.tmp && \
	${NETBSDSRCDIR}/minix/commands/MAKEDEV/MAKEDEV.sh -m -r \
		>> ${.TARGET}.tmp && mv ${.TARGET}.tmp ${.TARGET}

# We have to remove the two first entries of the generated proto file, as
# well as the two last ones (closing $).
# Do not forget to double $ so that make doesn't try to expand it.
proto.dev: proto.dev.mtree etc/pwd.db etc/spwd.db etc/passwd etc/group
	${TOOL_MTREE} -f ${.TARGET}.mtree -N ${.OBJDIR}/etc -C -K device | \
		${TOOL_TOPROTO} | ${TOOL_SED} -e '1,4d' | \
		${TOOL_SED} -e '$$d' |${TOOL_SED} -e '$$d' > ${.TARGET}.tmp
	grep console ${.TARGET}.tmp	# sanity check; is there a console entry?
	mv ${.TARGET}.tmp ${.TARGET}

proto.gen: ${PROTO} ${PROTO_FILES} proto.dev ${PROGRAMS}
	${STRIP} ${PROGRAMS}
	# We are using the c preprocessor to generate proto.gen
	# used in the mkfs tool.
	${TOOL_CAT} ${PROTO} | ${CC} \
	    	${RAMDISK_DEFINES} \
		-E - | grep -v "^$$" | grep -v "#" >${.TARGET}

.include <minix.service.mk>
