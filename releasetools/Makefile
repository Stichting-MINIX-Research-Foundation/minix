# Makefile for the kernel image.
.include <bsd.own.mk>
.include "nbsd.config"

.include <bsd.own.mk>
.include <bsd.sys.mk>

u=/usr
MDEC=	/usr/mdec
GEN_FILES=	*.bak image kernel *.iso *.iso.gz cdfdimage rootimage src

# LSC detect where were built the objects files
PROGROOT:= ..
.if "${MAKEOBJDIR:S,${.CURDIR},,}" != ""
PROGROOT:= ${MAKEOBJDIR:S,releasetools,,}
.endif

# Specify the programs that are part of the system image.
KERNEL= ${PROGROOT}/kernel/kernel
# PROGRAMS are in the order they should be loaded by boot
PROGRAMS+= ${PROGROOT}/servers/ds/ds
PROGRAMS+= ${PROGROOT}/servers/rs/rs
PROGRAMS+= ${PROGROOT}/servers/pm/pm
PROGRAMS+= ${PROGROOT}/servers/sched/sched
PROGRAMS+= ${PROGROOT}/servers/vfs/vfs
PROGRAMS+= ${PROGROOT}/drivers/memory/memory
.if ${MACHINE_ARCH} == "i386"
PROGRAMS+= ${PROGROOT}/drivers/log/log
.endif
PROGRAMS+= ${PROGROOT}/drivers/tty/tty
PROGRAMS+= ${PROGROOT}/servers/mfs/mfs
PROGRAMS+= ${PROGROOT}/servers/vm/vm
PROGRAMS+= ${PROGROOT}/servers/pfs/pfs
PROGRAMS+= ${PROGROOT}/servers/init/init

usage:	
	@echo " " >&2
	@echo "Master Makefile to create new MINIX configuration." >& 2
	@echo "Root privileges are required." >&2
	@echo " " >&2
	@echo "Usage:" >&2
	@echo "	make includes   # Install include files" >&2
	@echo "	make depend     # Generate dependency files" >&2
	@echo "	make services   # Compile and install all services" >&2
	@echo "	make install    # Make image, and install to hard disk" >&2
	@echo "	make hdboot     # Make image, and install to hard disk" >&2
	@echo "	make bootable   # Make hard disk bootable" >&2
	@echo "	make nbsd_fetch	# Download current NetBSD reference sources" >&2
	@echo "	make nbsd_diff	# Update minix-port.patch in NetBSD sources" >&2
	@echo "	make clean      # Remove all compiler results, except libs" >&2
	@echo " " >&2
	@echo "To create a fresh MINIX configuration, try:" >&2
	@echo "	make clean install      # new boot image" >&2
	@echo "	make fresh install      # new everything" >&2
	@echo " " >&2

all: services

# rebuild the program or system libraries
includes:
	$(MAKE) -C ../ includes

depend: includes .gitignore
	$(MAKE) -C ../ depend

.gitignore: Makefile
	echo $(GEN_FILES) | tr ' ' '\n' >.gitignore

services: includes kernel servers .WAIT drivers

kernel: includes
	$(MAKE) -C ../kernel 

servers: includes
	$(MAKE) -C ../servers all install

drivers: includes servers
	$(MAKE) -C ../drivers all install

# make bootable and place system images
bootable:
	exec su root mkboot bootable ${DESTDIR}

hdboot: services .WAIT do-hdboot

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
	${INSTALL} $$i $$newname;				\
	done
	@cp ${PROGROOT}/kernel/kernel ${DESTDIR}/boot/minix/.temp/
	@if [ "${MKINSTALLBOOT:Uno}" != "no" ] ; then		\
	${STRIP} -s ${DESTDIR}/boot/minix/.temp/* ;		\
	gzip ${DESTDIR}/boot/minix/.temp/mod* ;			\
	${HOST_SH} mkboot hdboot ${DESTDIR};			\
	${HOST_SH} ../commands/update_bootcfg/update_bootcfg.sh;\
	else							\
	${INSTALL_DIR} ${DESTDIR}/multiboot;			\
	${INSTALL} ${DESTDIR}/boot/minix/.temp/* ${DESTDIR}/multiboot;	\
	fi

install:
	${MAKE} includes services hdboot

# download and update NetBSD reference sources.
nbsd_fetch:
	export CVS_RSH=ssh; 							\
	export OLDPWD=`pwd`;							\
	echo "retrieving hierarchies from ${NBSD_CVSROOT}";			\
	IFS=,;									\
	cd ..;									\
	cat releasetools/nbsd_ports | grep -v '^#' | while read port ;		\
	do	set $$port;							\
		date=$$1; minixpath=$$2; origpath=$$3;				\
		if [ $$# -lt 3 ]; then origpath=$$2; fi;			\
		echo "retrieving $$origpath ..";				\
		cvs -q -d ${NBSD_CVSROOT} co -N -D "$$date UTC" -d nbsdsrc "src/$$origpath" ; \
	done;									\
	cd $${OLDPWD};

nbsd_diff:
	find .. -name minix-port.patch | xargs rm
	cat nbsd_ports | grep -v '^#' | \
	( cd .. && awk -F, '{ minixpath=$$2; origpath=$$3; if(NF < 3) { origpath=$$2; } system("sh releasetools/nbsd_diff.sh " \
	   "nbsdsrc/src/"origpath" "minixpath" "minixpath"/minix-port.patch");}' )
	find .. -name minix-port.patch | xargs wc -l | sort -n


# clean up compile results
clean:
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers $@
	$(MAKE) -C ../drivers $@
	rm -rf $(GEN_FILES)

cleandepend::
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers  $@
	$(MAKE) -C ../drivers  $@
