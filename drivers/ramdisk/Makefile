# Makefile for ramdisk image

.include <bsd.own.mk>

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
PROTO=	proto

# Common to all architectures
ETC=		system.conf master.passwd
EXTRA=		rc
PROTO_FILES=	proto.common.etc proto.common.dynamic proto.dev
PROG_DRIVERS=
PROG_COMMANDS=	mount fsck.mfs sh service loadramdisk sysenv
PROG_SERVERS=	mfs procfs
PROG_BIN=
PROG_SBIN=
PROG_USR.BIN=
PROG_USR.SBIN=

.if ${MKSMALL} != "yes"
RAMDISK_SMALL=	1
.endif

.if ${MACHINE_ARCH} == "i386"
ETC+=		rs.single
EXTRA+=		
PROG_DRIVERS+=	at_wini floppy pci
PROG_COMMANDS+=	cdprobe 
PROG_USR.SBIN+=	pwd_mkdb

.if ${MKSMALL} != "yes"
PROG_DRIVERS+=	ahci
PROG_DRIVERS+=	virtio_blk
PROG_SERVERS+=	ext2
.endif

.if ${MKACPI} != "no"
RAMDISK_INC_ACPI= 1
PROG_DRIVERS+=	acpi
.endif
.endif # ${MACHINE_ARCH} == "i386"

.if ${MACHINE_ARCH} == "earm"
PROG_DRIVERS+=	mmc
.endif # ${MACHINE_ARCH} == "earm"

.if ${LDSTATIC} == "-dynamic"
RAMDISK_DYNAMIC= 1
PROG_LIBEXEC+=	ld.elf_so
PROG_LIBS+=	libc libminlib libcompat_minix libterminfo
.endif

PROGRAMS=${PROG_COMMANDS} \
	 ${PROG_BIN} ${PROG_SBIN} ${PROG_USR.BIN} ${PROG_USR.SBIN} \
	 ${PROG_DRIVERS} ${PROG_SERVERS} \
	 ${PROG_LIBEXEC}

CPPFLAGS+= -I${NETBSDSRCDIR}/servers

# LSC We have to take care of not erasing the source file, so never add EXTRA
# to CLEANFILES
CLEANFILES += ${PROGRAMS} ${SCRIPTS} ${ETC} image image.c t proto.gen
CLEANFILES += ${LIBRARIES}
CLEANFILES += ${PROG_LIBEXEC}
CLEANFILES += ${.OBJDIR}/etc/*

#############################################################
# LSC Below this point the rules should not be modified
#############################################################

# Tool to bootstrap the password db
TOOL_PWD_MKDB?= ${NETBSDSRCDIR}/usr.sbin/pwd_mkdb/pwd_mkdb

# Remove "drivers/ramdisk" component from path
PROGROOT:= ${.OBJDIR:S,drivers/ramdisk,,}

# Generate dependencies rules for config files
.for etc in ${ETC}
${etc}: ${NETBSDSRCDIR}/etc/${etc}
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
${file}: ${NETBSDSRCDIR}/drivers/ramdisk/${file} .PHONY
	${INSTALL} $> $@
.endfor
.endif

# Generate dependencies rules for dynamic libraries, if needed
.for lib in ${PROG_LIBS}
PROGRAMS+= ${lib}.so.0
CLEANFILES += ${lib}.so.0
${lib}.so.0:${PROGROOT}/lib/${lib}/${lib}.so.0
	${INSTALL} $> $@

${PROGROOT}/lib/${lib}/${lib}.so.0:
	${MAKE} -C ${NETBSDSRCDIR}/lib/${lib} all
.endfor

# Generate dependencies rules for binaries
dir.sh:= ash
.for srcdir in bin sbin commands drivers libexec servers usr.bin usr.sbin
.for prog in ${PROG_${srcdir:tu}}
${prog}: ${PROGROOT}/${srcdir}/${dir.${prog}:U${prog}}/${prog}
	${INSTALL} $> $@

${PROGROOT}/${srcdir}/${dir.${prog}:U${prog}}/${prog}:
	${MAKE} -C ${NETBSDSRCDIR}/${srcdir}/${dir.${prog}:U${prog}} all
.endfor # prog
.endfor # srcdir

image:	proto.gen ${ETC} ${EXTRA} pwd.db spwd.db passwd
	${_MKMSG_CREATE} "Generating ramdisk image"
	${TOOL_MKFSMFS} image proto.gen || { rm -f image; false; }
#	if fsck.mfs -s image | grep -q CLEAN;	\
#	then	: ;	\
#	else	echo "CLEAN sanity check of image failed."	;	\
#		echo "(Perhaps install current mkfs and fsck.)" ;	\
#		rm -f image; false;             \
#	fi

etc/master.passwd: ${NETBSDSRCDIR}/etc/master.passwd
	rm -rf ${.OBJDIR}/etc/
	mkdir -p ${.OBJDIR}/etc
	${INSTALL} $> $@
	${TOOL_PWD_MKDB} -V 0 -p -d . etc/master.passwd

# LSC We use @F because some version of make still output a full path for @,
#     even when the target is just a file name
pwd.db spwd.db passwd: etc/master.passwd
	${INSTALL} etc/${@F} ${@F}

proto.gen: ${PROTO} ${PROTO_FILES} ${PROGRAMS}
	${STRIP} ${PROGRAMS}
	# We are using the c preprocessor to generate proto.gen
	# used in the mkfs tool.
	${TOOL_CAT} ${PROTO} | ${CC} \
	    	${RAMDISK_DEFINES} \
		-E - | grep -v "^$$" | grep -v "#" >${.OBJDIR}/${.TARGET}

.include <minix.service.mk>
