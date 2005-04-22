# Makefile for the kernel image.
#
u=/usr
CC=	exec cc
CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE
MDEC=	/usr/mdec
MAKE=	exec make -$(MAKEFLAGS)

# specify the programs that are part of the system image
PROGRAMS=	../kernel/kernel \
	../servers/mm/mm \
	../servers/fs/fs \
	../servers/is/is \
	../drivers/tty/tty \
	../drivers/memory/memory \
	../drivers/at_wini/at_wini \
	../drivers/floppy/floppy \
	../drivers/printer/printer \
	../servers/init/init \
	#bootfs.img

usage:	
	@echo " " >&2
	@echo "Master Makefile to create new MINIX configuration. Root privileges required." >&2
	@echo "Usage:" >&2
	@echo "	make libraries  # Make system libraries" >&2
	@echo "	make programs   # Compile and install all programs" >&2
	@echo "	make fresh      # Make clean, libraries, and programs" >&2
	@echo "	make image      # Make programs and create system image" >&2
	@echo "	make hdboot     # Make image, and install to hard disk" >&2
	@echo "	make fdboot     # Make image, and install to floppy disk" >&2
	@echo "	make bootable   # Make hard disk bootable" >&2
	@echo "	make clean      # Remove all compiler results, except libs" >&2
	@echo " " >&2
	@echo "To create a fresh MINIX configuration, try:" >&2
	@echo "	make fresh hdboot" >&2
	@echo " " >&2

# create a fresh configuration or system image
fresh: 
	cd ../lib && $(MAKE) clean
	$(MAKE) clean 
	$(MAKE) libraries programs

image: programs	
	installboot -image $@ $(PROGRAMS)


# rebuild the program or system libraries
programs: 
	cd ../kernel && $(MAKE) 
	cd ../servers && $(MAKE) install
	cd ../drivers && $(MAKE) install

libraries:
	cd ../lib && $(MAKE) install


# make bootable and place system images
bootable:
	exec su root mkboot bootable

hdboot:	image
	exec sh mkboot $@

fdboot:	image
	exec su root mkboot $@

install: hdboot

# clean up compile results
clean:
	cd ../kernel && $(MAKE) $@
	cd ../servers && $(MAKE) $@
	cd ../drivers && $(MAKE) $@
	rm -f *.bak image

