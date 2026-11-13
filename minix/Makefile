.include <bsd.own.mk>

SUBDIR+=	include .WAIT
SUBDIR+=	bin
SUBDIR+=	commands
SUBDIR+=	fs
SUBDIR+=	kernel
SUBDIR+=	lib
SUBDIR+=	llvm
SUBDIR+=	man
SUBDIR+=	net
SUBDIR+=	sbin
SUBDIR+=	servers
SUBDIR+=	share
.if ${MKATF} == "yes"
SUBDIR+=	tests
.endif
SUBDIR+=	usr.bin
SUBDIR+=	usr.sbin

# BJG - build drivers last as the ramdisk depends on some other drivers
SUBDIR+=	.WAIT drivers

.include <bsd.subdir.mk>

################################################################
# Makefile for the kernel image.
#   -- copied from releasetools/Makefile
################################################################

PROGROOT:= ${MAKEOBJDIR}/..

# Specify the programs that are part of the system image.
KERNEL= ${PROGROOT}/minix/kernel/kernel

# PROGRAMS are in the order they should be loaded by boot
PROGRAMS+= ${PROGROOT}/minix/servers/ds/ds
PROGRAMS+= ${PROGROOT}/minix/servers/rs/rs
PROGRAMS+= ${PROGROOT}/minix/servers/pm/pm
PROGRAMS+= ${PROGROOT}/minix/servers/sched/sched
PROGRAMS+= ${PROGROOT}/minix/servers/vfs/vfs
PROGRAMS+= ${PROGROOT}/minix/drivers/storage/memory/memory
PROGRAMS+= ${PROGROOT}/minix/drivers/tty/tty/tty
PROGRAMS+= ${PROGROOT}/minix/servers/mib/mib
PROGRAMS+= ${PROGROOT}/minix/servers/vm/vm
PROGRAMS+= ${PROGROOT}/minix/fs/pfs/pfs
PROGRAMS+= ${PROGROOT}/minix/fs/mfs/mfs
PROGRAMS+= ${PROGROOT}/sbin/init/init

realinstall: 
	@rm -rf ${DESTDIR}/boot/minix/.temp/
	${INSTALL_DIR} ${DESTDIR}/boot/minix/.temp
# mod_0 is used to make alphabetical order equal to the boot order
	@n=0;							\
	for i in ${PROGRAMS};					\
	do							\
	n=`expr $$n + 1`;					\
	[ "$$n" -ge 10 ] && prefix="mod" || prefix="mod0";	\
	newname="${DESTDIR}/boot/minix/.temp/$${prefix}$${n}_`basename $$i`"; \
	${INSTALL_FILE} $$i $$newname;				\
	echo ${INSTALL_FILE} $$i $$newname;			\
	done
	@${INSTALL_FILE} ${KERNEL} ${DESTDIR}/boot/minix/.temp/
	@if [ "${MKINSTALLBOOT:Uno}" != "no" ] ; then		\
	${STRIP} -s ${DESTDIR}/boot/minix/.temp/* ;		\
	fi

# LSC: For STRIP and HOST_SH
.include <bsd.sys.mk>
.include <bsd.prog.mk>
