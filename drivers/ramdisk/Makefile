# Makefile for ramdisk image

.include <bsd.own.mk>

TOOL_PWD_MKDB?= ${NETBSDSRCDIR}/usr.sbin/pwd_mkdb/pwd_mkdb
install:

all:

PROG_DRIVERS=at_wini floppy pci
PROG_COMMANDS=cdprobe loadramdisk mount fsck.mfs sysenv sh \
	service 
PROG_SERVERS=mfs procfs
PROG_USRSBIN=pwd_mkdb
PROGRAMS=$(PROG_DRIVERS) $(PROG_COMMANDS) $(PROG_SERVERS) $(PROG_USRSBIN)
EXTRA=system.conf master.passwd rs.single 

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
CLEANFILES += $(PROGRAMS) $(SCRIPTS) $(EXTRA) image image.c t proto.gen
CLEANFILES += $(LIBRARIES) pwd.db spwd.db passwd

image:	proto.gen mtab rc $(EXTRA) pwd.db spwd.db passwd
	${TOOL_MKFSMFS} image proto.gen || { rm -f image; false; }
#	if fsck.mfs -s image | grep -q CLEAN;	\
#	then	: ;	\
#	else	echo "CLEAN sanity check of image failed."	;	\
#		echo "(Perhaps install current mkfs and fsck.)" ;	\
#		rm -f image; false;             \
#	fi

.for driver in $(PROG_DRIVERS)
$(driver): ../$(driver)/$(driver)
	${INSTALL} $> $@

../$(driver)/$(driver):
	$(MAKE) -C ../$(driver) $(driver)
.endfor

.for cmd in $(PROG_COMMANDS)
.if $(cmd) == sh
dir.$(cmd)=ash
.else
dir.$(cmd)=$(cmd)
.endif
$(cmd): ../../commands/$(dir.$(cmd))/$(cmd)
	${INSTALL} $> $@

../../commands/$(dir.$(cmd))/$(cmd):
	$(MAKE) -C ../../commands/$(dir.$(cmd)) $(cmd)
.endfor

.for etc in $(EXTRA)
$(etc): ../../etc/$(etc)
	${INSTALL} $> $@
.endfor

.for cmd in $(PROG_USRSBIN)
$(cmd): ../../usr.sbin/$(cmd)/$(cmd)
	${INSTALL} $> $@

../../usr.sbin/$(cmd)/$(cmd):
	$(MAKE) -C ../../usr.sbin/$(cmd) $(cmd)
.endfor

.for server in $(PROG_SERVERS)
$(server): ../../servers/$(server)/$(server)
	${INSTALL} $> $@

../../servers/$(server)/$(server):
	$(MAKE) -C ../../servers/$(server) $(server)
.endfor

pwd.db spwd.db passwd: ../../etc/master.passwd ../../usr.sbin/pwd_mkdb/pwd_mkdb
	rm -f ../../etc/master.passwd.orig ../../etc/passwd.orig
	rm -f ../../etc/pwd.db.tmp ../../etc/spwd.db.tmp
	${TOOL_PWD_MKDB} -V 0 -p -d ../../ ../../etc/master.passwd
	${INSTALL} ../../etc/$@ $@

proto.gen: $(PROGRAMS) proto.sh ${PROTO} proto.common.etc proto.common.dynamic proto.sh
	strip $(PROGRAMS)
	sh -e proto.sh ${PROTO} >proto.gen

.include <minix.service.mk>
