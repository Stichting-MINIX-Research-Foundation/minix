# Makefile for the kernel image.

u=/usr
MDEC=	/usr/mdec

# Specify the programs that are part of the system image.
PROGRAMS=	kernel \
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
	@echo "	make libraries  # Make system libraries" >&2
	@echo "	make services   # Compile and install all services" >&2
	@echo "	make image      # Make needed services and create boot image" >&2
	@echo "	make install    # Make image, and install to hard disk" >&2
	@echo "	make hdboot     # Make image, and install to hard disk" >&2
	@echo "	make fdboot     # Make image, and install to floppy disk" >&2
	@echo "	make bootable   # Make hard disk bootable" >&2
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

# for fast complie kernel and generate image, skip servers and drivers
image_mb: includes
	cd ../kernel && $(MAKE)
	${PAD_KERNEL_TEXT}
	installboot -image $@ $(PROGRAMS)
	
image:  includes services
	${PAD_KERNEL_TEXT}
	installboot -image $@ $(PROGRAMS)

# rebuild the program or system libraries
includes:
	$(MAKE) -C .. includes

depend: includes
	$(MAKE) -C ../ depend

services: includes kernel servers .WAIT drivers

kernel: includes
	$(MAKE) -C ../kernel 

servers: includes
	$(MAKE) -C ../servers all install

drivers: includes servers
	$(MAKE) -C ../drivers all install

libraries: includes
	cd ../lib && sh ack_build.sh clean obj depend all install

# make bootable and place system images
bootable:
	exec su root mkboot bootable

hdboot:	image
	exec sh mkboot $@
	@sync

fdboot:	image
	exec su root mkboot $@
	@sync

install: includes services hdboot

# clean up compile results
clean:
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers $@
	$(MAKE) -C ../drivers $@
	rm -rf *.bak image kernel *.iso *.iso.gz cdfdimage rootimage src

cleandepend::
	$(MAKE) -C ../kernel $@
	$(MAKE) -C ../servers  $@
	$(MAKE) -C ../drivers  $@
