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
	@echo "	make image      # Make needed services and create boot image" >&2
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

all: services image

image: includes services
	mkimage $(PROGRAMS)

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

hdboot: image
	rm -rf /boot/minix/.temp/
	mkdir -p /boot/minix/.temp
# mod_0 is used to make alphabetical order equal to the boot order
	for i in ${PROGRAMS};					\
	do							\
	let n=n+1 >/dev/null;					\
	[ "$$n" -ge 10 ] && prefix="mod" || prefix="mod0";	\
	newname="/boot/minix/.temp/$${prefix}$${n}_`basename $$i`"; \
	cp $$i $$newname;				\
	if [ -d /boot/image ];				\
	then	ln -f $$newname /boot/`basename $$i`;	\
	else	strip -s $$newname;			\
		gzip $$newname;				\
	fi						\
	done
	cp ../kernel/kernel /boot/minix/.temp/
	strip -s /boot/minix/.temp/kernel
	[ -d /boot/image ] && ln -f /boot/minix/.temp/kernel /boot/kernel || true
	sh mkboot $@ minix
	sh ../commands/update_bootcfg/update_bootcfg.sh

fdboot:	image
	exec su root mkboot $@
	@sync

install: includes services hdboot

# download and update NetBSD reference sources.
nbsd_fetch:
	export CVS_RSH=ssh; 							\
	echo "retrieving hierarchies from ${NBSD_CVSROOT}";			\
	for d in `awk '{print $$2;}' < nbsd_ports`;				\
	do	echo "retrieving $$d ..";					\
		cd ${MINIXSRCDIR} && cvs -q -d ${NBSD_CVSROOT} co -N -d nbsdsrc $$d; \
	done

nbsd_diff:
	cd ${MINIXSRCDIR} && awk '{ system("sh tools/nbsd_diff.sh " \
	   "nbsdsrc/"$$2" "$$1" "$$1"/minix-port.patch");}' < tools/nbsd_ports


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
