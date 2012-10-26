# Makefile for ramdisk image

.include <bsd.own.mk>

# Remove "drivers/ramdisk" component from path
PROGROOT:= ${.OBJDIR:S,drivers/ramdisk,,}

TOOL_PWD_MKDB?= ${NETBSDSRCDIR}/usr.sbin/pwd_mkdb/pwd_mkdb
install:

all:

PROG_DRIVERS=at_wini floppy pci
PROG_COMMANDS=cdprobe loadramdisk mount fsck.mfs sysenv sh \
	service 
PROG_SERVERS=mfs procfs
PROG_USRSBIN=pwd_mkdb
PROGRAMS=${PROG_DRIVERS} ${PROG_COMMANDS} ${PROG_SERVERS} ${PROG_USRSBIN}
EXTRA=system.conf master.passwd rs.single 
PROTO_FILES=proto.common.etc proto.common.dynamic proto.sh proto.dev

.if ${MKSMALL} != "yes"
PROG_DRIVERS+= ahci
PROG_SERVERS+= ext2
PROTO= proto
.else
PROTO= proto.small
.endif

.if ${MKACPI} != "no"
PROG_DRIVERS+= acpi
.endif

CPPFLAGS+= -I${NETBSDSRCDIR}/servers
CLEANFILES += ${PROGRAMS} ${SCRIPTS} ${EXTRA} image image.c t proto.gen
CLEANFILES += ${LIBRARIES}
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
	${MAKE} -C ${NETBSDSRCDIR}/drivers/${driver} ${driver}
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
	${MAKE} -C ${NETBSDSRCDIR}/commands/${dir.${cmd}} ${cmd}
.endfor

.for etc in ${EXTRA}
${etc}: ${NETBSDSRCDIR}/etc/${etc}
	${INSTALL} $> $@
.endfor

.for cmd in ${PROG_USRSBIN}
${cmd}: ${PROGROOT}/usr.sbin/${cmd}/${cmd}
	${INSTALL} $> $@

${PROGROOT}/usr.sbin/${cmd}/${cmd}:
	${MAKE} -C ${NETBSDSRCDIR}/usr.sbin/${cmd} ${cmd}
.endfor

.for server in ${PROG_SERVERS}
${server}: ${PROGROOT}/servers/${server}/${server}
	${INSTALL} $> $@

${PROGROOT}/servers/${server}/${server}:
	${MAKE} -C ${NETBSDSRCDIR}/servers/${server} ${server}
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
	${HOST_SH} -e proto.sh ${PROTO} >${.OBJDIR}/${.TARGET}

.include <minix.service.mk>
