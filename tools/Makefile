# Makefile for the kernel image.
.include <bsd.own.mk>
.include "nbsd.config"

.include <bsd.own.mk>

u=/usr
MDEC=	/usr/mdec
GEN_FILES=	*.bak image kernel *.iso *.iso.gz cdfdimage rootimage src

# Specify the programs that are part of the system image.
KERNEL= kernel
# PROGRAMS are in the order they should be loaded by boot
PROGRAMS= \
	../servers/ds/ds \
	../servers/rs/rs \
	../servers/pm/pm \
	../servers/sched/sched \
	../servers/vfs/vfs \
	../drivers/memory/memory \
	../drivers/log/log \
	../drivers/tty/tty \
	../servers/mfs/mfs \
	../servers/vm/vm \
	../servers/pfs/pfs \
	../servers/init/init

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
	@echo "	make fdboot     # Make image, and install to floppy disk" >&2
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
	$(MAKE) -C .. includes

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
	exec su root mkboot bootable

hdboot:
	rm -rf ${DESTDIR}/boot/minix/.temp/
	${INSTALL_DIR} {DESTDIR}/boot/minix/.temp
# mod_0 is used to make alphabetical order equal to the boot order
	for i in ${PROGRAMS};					\
	do							\
	let n=n+1 >/dev/null;					\
	[ "$$n" -ge 10 ] && prefix="mod" || prefix="mod0";	\
	newname="${DESTDIR}/boot/minix/.temp/$${prefix}$${n}_`basename $$i`"; \
	cp $$i $$newname;					\
	strip -s $$newname;					\
	gzip $$newname;						\
	done
	cp ../kernel/kernel ${DESTDIR}/boot/minix/.temp/
	strip -s ${DESTDIR}/boot/minix/.temp/kernel
	sh mkboot $@
	sh ../commands/update_bootcfg/update_bootcfg.sh

fdboot:
	exec su root mkboot $@
	@sync

install:
	if [ ${DESTDIR}X != X ];			\
	then	${MAKE} cross_install;			\
	else	${MAKE} includes services hdboot;	\
	fi

cross_install:
	${INSTALL_DIR} ${DESTDIR}/multiboot
#ext2 might be needed
	for i in ${PROGRAMS} ../servers/ext2/ext2;	\
	do								\
	let n=n+1 >/dev/null;						\
	[ "$$n" -ge 10 ] && prefix="mod" || prefix="mod0";		\
	newname="${DESTDIR}/multiboot/$${prefix}$${n}_`basename $$i`";	\
	${INSTALL} $$i $$newname;						\
	strip -s $$newname;						\
	gzip -f $$newname; 						\
	done
	${INSTALL} ../kernel/kernel ${DESTDIR}/multiboot/
	strip -s ${DESTDIR}/multiboot/kernel

# download and update NetBSD reference sources.
nbsd_fetch:
	export CVS_RSH=ssh; 							\
	echo "retrieving hierarchies from ${NBSD_CVSROOT}";			\
	IFS=,;									\
	cat nbsd_ports | grep -v '^#' | while read port ;			\
	do	set $$port;							\
		date=$$1; minixpath=$$2; origpath=$$3;				\
		if [ $$# -lt 3 ]; then origpath=$$2; fi;			\
		echo "retrieving $$origpath ..";				\
		cd ${MINIXSRCDIR} && cvs -q -d ${NBSD_CVSROOT} co -N -D "$$date UTC" -d nbsdsrc "src/$$origpath" ; \
	done

nbsd_diff:
	find ${MINIXSRCDIR} -name minix-port.patch | xargs rm
	cat nbsd_ports | grep -v '^#' | \
	( cd ${MINIXSRCDIR} && awk -F, '{ minixpath=$$2; origpath=$$3; if(NF < 3) { origpath=$$2; } system("sh tools/nbsd_diff.sh " \
	   "nbsdsrc/src/"origpath" "minixpath" "minixpath"/minix-port.patch");}' )
	find ${MINIXSRCDIR} -name minix-port.patch | xargs wc -l | sort -n


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
