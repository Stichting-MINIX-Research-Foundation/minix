# Makefile for the kernel image.
.include <bsd.own.mk>

GEN_FILES=	*.bak image kernel *.iso *.iso.gz cdfdimage rootimage src

# LSC detect where were built the objects files
PROGROOT:= ..
.if "${MAKEOBJDIR:S,${.CURDIR},,}" != ""
PROGROOT:= ${MAKEOBJDIR:S,releasetools,,}
.endif

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

all usage help:
	@echo " " >&2
	@echo "Master Makefile to create new MINIX configuration." >&2
	@echo "Root privileges are required." >&2
	@echo " " >&2
	@echo "Usage:" >&2
	@echo "	make includes   # Install include files" >&2
	@echo "	make depend     # Generate dependency files" >&2
	@echo "	make services   # Compile and install all services" >&2
	@echo "	make hdboot     # Make image, and install to hard disk" >&2
	@echo "	make clean      # Remove all compiler results" >&2
	@echo " " >&2
	@echo "To create a fresh MINIX configuration, try:" >&2
	@echo "	make clean install      # new boot image" >&2
	@echo " " >&2

.gitignore: Makefile
	echo ${GEN_FILES} | tr ' ' '\n' >.gitignore

includes:
	${MAKE} -C ${NETBSDSRCDIR} includes

depend: includes .gitignore
	${MAKE} -C ${NETBSDSRCDIR} depend

libraries: includes
	${MAKE} -C ${NETBSDSRCDIR} do-lib

kernel: libraries
	${MAKE} -C ${NETBSDSRCDIR}/minix/kernel

servers: libraries
	${MAKE} -C ${NETBSDSRCDIR}/minix/fs all install
	${MAKE} -C ${NETBSDSRCDIR}/minix/net all install
	${MAKE} -C ${NETBSDSRCDIR}/minix/servers all install

sbin: libraries
	${MAKE} -C ${NETBSDSRCDIR}/sbin all install
	${MAKE} -C ${NETBSDSRCDIR}/minix/sbin all install

drivers: libraries
	${MAKE} -C ${NETBSDSRCDIR}/minix/drivers all install

services: kernel servers drivers sbin

do-hdboot: 
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
	gzip ${DESTDIR}/boot/minix/.temp/mod* ;			\
	${HOST_SH} mkboot hdboot ${DESTDIR};			\
	${HOST_SH} ../minix/commands/update_bootcfg/update_bootcfg.sh;\
	fi

hdboot: services .WAIT do-hdboot

clean:
	${MAKE} -C ${NETBSDSRCDIR}/lib $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/kernel $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/fs $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/net $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/servers $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/drivers $@
	${MAKE} -C ${NETBSDSRCDIR}/sbin $@
	${MAKE} -C ${NETBSDSRCDIR}/minix/sbin $@
	rm -rf ${GEN_FILES}

# LSC: For STRIP and HOST_SH
.include <bsd.sys.mk>
