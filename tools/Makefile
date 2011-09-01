# Makefile for the kernel image.
.include <bsd.own.mk>
.include "nbsd.config"

.include <bsd.own.mk>

u=/usr
MDEC=	/usr/mdec
GEN_FILES=	*.bak image kernel *.iso *.iso.gz cdfdimage rootimage src

# Specify the programs that are part of the system image.
.if ${BUILDAVFS} == "yes"
VFS= avfs
PFS= apfs
.else
VFS= vfs
PFS= pfs
.endif
KERNEL= kernel
PROGRAMS= \
	../servers/ds/ds \
	../servers/rs/rs \
	../servers/pm/pm \
	../servers/sched/sched \
	../servers/${VFS}/vfs \
	../drivers/memory/memory \
	../drivers/log/log \
	../drivers/tty/tty \
	../servers/mfs/mfs \
	../servers/vm/vm \
	../servers/${PFS}/pfs \
	../servers/init/init

usage:	
	@echo " " >&2
	@echo "Master Makefile to create new MINIX configuration." >& 2
	@echo "Root privileges are required." >&2
	@echo " " >&2
	@echo "Usage:" >&2
	@echo "	make includes   # Install include files" >&2
	@echo "	make depend     # Generate dependency files" >&2
	@echo "	make libraries  # Make system libraries" >&2
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

# Pad the kernel text only if ACK compiler is used. padtext does not work for
# gnu-like compilers as they generate common I&D and a linker script can do the
# same in a nicer way
.if ${COMPILER_TYPE} == "gnu"
PAD_KERNEL_TEXT := cp ../kernel/kernel kernel
.else
PAD_KERNEL_TEXT := padtext ../kernel/kernel kernel
.endif

.if ${OBJECT_FMT} == "ELF"
image: includes services
	${PAD_KERNEL_TEXT}
	mkimage $(PROGRAMS)
	cp ${KERNEL} ${PROGRAMS} /boot
	strip -s ${KERNEL:T:S/^/\/boot\//} ${PROGRAMS:T:S/^/\/boot\//}
.else
image:  includes services
	${PAD_KERNEL_TEXT}
	installboot -image $@ $(KERNEL) $(PROGRAMS)
.endif

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
	$(MAKE) -C ../servers all

drivers: includes servers
	$(MAKE) -C ../drivers all install

libraries: includes
	cd ../lib && sh ack_build.sh clean obj depend all install

# make bootable and place system images
bootable:
	exec su root mkboot bootable

.if ${OBJECT_FMT} == "ELF"
hdboot: image
.else
hdboot:	image
	exec sh mkboot $@
	@sync
.endif

fdboot:	image
	exec su root mkboot $@
	@sync

install: includes services hdboot

# download and update NetBSD reference sources.
nbsd_fetch:
	export CVS_RSH=ssh; \
	cd ${MINIXSRCDIR} && awk '{print $$2;}' < tools/nbsd_ports \
		| xargs -t cvs -d ${NBSD_CVSROOT} co -N -d nbsdsrc ${NBSD_REF}

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
