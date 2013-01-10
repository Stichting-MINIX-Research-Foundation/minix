# Makefile for ramdisk image

.include <bsd.own.mk>

# Remove "drivers/ramdisk" component from path
PROGROOT:= ${.OBJDIR:S,drivers/ramdisk,,}

TOOL_PWD_MKDB?= ${NETBSDSRCDIR}/usr.sbin/pwd_mkdb/pwd_mkdb
install:

all:

#
# Add a few defines we are going to use during the image
# creation to determine what features and binaries to include
# in the final image
# ACPI            do we have/include the acpi binary
# RAMDISK_SMALL   is the script called with MKSMALL=yes
# DYNAMIC         does the ramdisk contain dynamic binaries?
RAMDISK_INC_ACPI=0
RAMDISK_SMALL=0
RAMDISK_DYNAMIC=0

# the name of the proto file to use
PROTO= proto

PROG_DRIVERS=at_wini floppy pci
PROG_COMMANDS=cdprobe loadramdisk mount fsck.mfs sysenv sh \
	service 
PROG_SERVERS=mfs procfs
PROG_USRSBIN=pwd_mkdb

.if ${LDSTATIC} == "-dynamic"
RAMDISK_DYNAMIC=1
PROG_LIBEXEC+=ld.elf_so
PROG_LIBS+=libc libminlib libcompat_minix libterminfo
.endif

PROGRAMS=${PROG_DRIVERS} ${PROG_COMMANDS} ${PROG_SERVERS} ${PROG_USRSBIN} ${PROG_LIBEXEC}

EXTRA=system.conf master.passwd rs.single 
PROTO_FILES=proto.common.etc proto.common.dynamic proto.dev

.if ${MKSMALL} != "yes"
RAMDISK_SMALL=1
PROG_DRIVERS+= ahci
PROG_SERVERS+= ext2
.endif


.if ${MKACPI} != "no"
PROG_DRIVERS+= acpi
RAMDISK_INC_ACPI=1
.endif

CPPFLAGS+= -I${NETBSDSRCDIR}/servers
CLEANFILES += ${PROGRAMS} ${SCRIPTS} ${EXTRA} image image.c t proto.gen
CLEANFILES += ${LIBRARIES}
CLEANFILES += ${PROG_LIBEXEC}
CLEANFILES += ${.OBJDIR}/etc/*

image:	proto.gen ${EXTRA} mtab rc pwd.db spwd.db passwd
	${_MKMSG_CREATE} "Generating ramdisk image"
	${TOOL_MKFSMFS} image proto.gen || { rm -f image; false; }
#	if fsck.mfs -s image | grep -q CLEAN;	\
#	then	: ;	\
#	else	echo "CLEAN sanity check of image failed."	;	\
#		echo "(Perhaps install current mkfs and fsck.)" ;	\
#		rm -f image; false;             \
#	fi

# LSC ramdisk rc file != /etc/rc
# mtab not empty!, force execution of the rule to ensure installation
# Conditionnally execute the command if the target file already exists
# I have to place .PHONY for the target to work as expected, but I cannot
# execute the command during an in-tree build (= make xxx)
.for file in rc mtab ${PROTO} ${PROTO_FILES}
${file}: ${.CURDIR}/${file} .PHONY
	[ -e $@ ] || ${INSTALL} $> $@
.endfor

.for driver in ${PROG_DRIVERS}
${driver}: ${PROGROOT}/drivers/${driver}/${driver}
	${INSTALL} $> $@

${PROGROOT}/drivers/${driver}/${driver}:
	${MAKE} -C ${NETBSDSRCDIR}/drivers/${driver} all
.endfor

.for lib in ${PROG_LIBS}
PROGRAMS+=${lib}.so.0
CLEANFILES += ${lib}.so.0
${lib}.so.0:${PROGROOT}/lib/${lib}/${lib}.so.0
	${INSTALL} $> $@

${PROGROOT}/lib/${lib}/${lib}.so.0:
	${MAKE} -C ${NETBSDSRCDIR}/lib/${lib} all
.endfor

.for libexec in ${PROG_LIBEXEC}
${libexec}:${PROGROOT}/libexec/${libexec}/${libexec}
	${INSTALL} $> $@

${PROGROOT}/libexec/${libexec}/${libexec}:
	${MAKE} -C ${NETBSDSRCDIR}/libexec/${libexec} all
.endfor


.for cmd in ${PROG_COMMANDS}
.if ${cmd} == sh
dir.${cmd}=ash
.else
dir.${cmd}=${cmd}
.endif
${cmd}: ${PROGROOT}/commands/${dir.${cmd}}/${cmd}
	${INSTALL} $> $@

${PROGROOT}/commands/$(dir.${cmd})/${cmd}:
	${MAKE} -C ${NETBSDSRCDIR}/commands/${dir.${cmd}} all
.endfor

.for etc in ${EXTRA}
${etc}: ${NETBSDSRCDIR}/etc/${etc}
	${INSTALL} $> $@
.endfor

.for cmd in ${PROG_USRSBIN}
${cmd}: ${PROGROOT}/usr.sbin/${cmd}/${cmd}
	${INSTALL} $> $@

${PROGROOT}/usr.sbin/${cmd}/${cmd}:
	${MAKE} -C ${NETBSDSRCDIR}/usr.sbin/${cmd} all
.endfor

.for server in ${PROG_SERVERS}
${server}: ${PROGROOT}/servers/${server}/${server}
	${INSTALL} $> $@

${PROGROOT}/servers/${server}/${server}:
	${MAKE} -C ${NETBSDSRCDIR}/servers/${server} all
.endfor

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
	${TOOL_CAT} ${PROTO} | ${HOST_CC} \
		-DRAMDISK_INC_ACPI=${RAMDISK_INC_ACPI} \
		-DRAMDISK_SMALL=${RAMDISK_SMALL} \
		-DRAMDISK_DYNAMIC=${RAMDISK_DYNAMIC} \
		-traditional-cpp -E - | grep -v "^$$" | grep -v "#" >${.OBJDIR}/${.TARGET}

.include <minix.service.mk>
