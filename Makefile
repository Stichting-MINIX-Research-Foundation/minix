# Master Makefile to compile everything in /usr/src except the system.

.include <bsd.own.mk>

MAKE=make

usage:
	@echo "" 
	@echo "Master Makefile for MINIX commands and utilities." 
	@echo "Root privileges are required for some actions." 
	@echo "" 
	@echo "Usage:" 
	@echo "	make world         # Compile everything (libraries & commands)"
	@echo "	make includes      # Install include files from src/"
	@echo "	make libraries     # Compile and install libraries (ack)"
	@echo "	make elf-libraries # Compile and install gcc/clang elf libs"
	@echo "	make commands      # Compile all, commands, but don't install"
	@echo "	make install       # Compile and install commands"
	@echo "	make depend        # Generate required .depend files"
	@echo "	make gnu-includes  # Install include files for GCC"
	@echo "	make clean         # Remove all compiler results"
	@echo "" 
	@echo "Run 'make' in tools/ to create a new MINIX configuration." 
	@echo "" 

# world has to be able to make a new system, even if there
# is no complete old system. it has to install commands, for which
# it has to install libraries, for which it has to install includes,
# for which it has to install /etc (for users and ownerships).
# etcfiles also creates a directory hierarchy in its
# 'make install' target.
# 
# etcfiles has to be done first.
world: mkfiles includes depend libraries elf-libraries install etcforce

mkfiles:
	make -C share/mk install

includes:
	$(MAKE) -C nbsd_include includes
	$(MAKE) -C include includes
	$(MAKE) -C lib includes NBSD_LIBC=yes
	$(MAKE) -C lib includes NBSD_LIBC=no

libraries: includes
	$(MAKE) -C lib build_ack

MKHEADERSS=/usr/pkg/gcc*/libexec/gcc/*/*/install-tools/mkheaders
gnu-includes: includes
	SHELL=/bin/sh; for d in $(MKHEADERSS); do if [ -f $$d ] ; then sh -e $$d ; fi; done

elf-libraries: includes
	$(MAKE) -C lib build_elf

commands: includes libraries
	$(MAKE) -C commands all
	$(MAKE) -C bin all
	$(MAKE) -C usr.bin all

depend:
	$(MAKE) CC=cc -C boot depend
	$(MAKE) -C commands depend
	$(MAKE) -C bin depend
	$(MAKE) -C usr.bin depend
	$(MAKE) -C kernel depend
	$(MAKE) -C servers depend
	$(MAKE) -C drivers depend

etcfiles:
	$(MAKE) -C etc install

etcforce:
	$(MAKE) -C etc installforce

all:
	$(MAKE) CC=cc -C boot all
	$(MAKE) -C commands all
	$(MAKE) -C bin all
	$(MAKE) -C usr.bin all
	$(MAKE) -C tools all

install:
	$(MAKE) CC=cc -C boot install
	$(MAKE) -C man install makedb
	$(MAKE) -C commands install
	$(MAKE) -C bin install
	$(MAKE) -C servers install
	$(MAKE) -C usr.bin install
	$(MAKE) -C share install
	$(MAKE) -C tools install

clean:
	$(MAKE) -C boot clean
	$(MAKE) -C commands clean
	$(MAKE) -C bin clean
	$(MAKE) -C usr.bin clean
	$(MAKE) -C tools clean
	$(MAKE) -C lib clean_all
	$(MAKE) -C test clean

cleandepend:
	$(MAKE) -C lib cleandepend_all
	$(MAKE) -C boot cleandepend
	$(MAKE) -C commands cleandepend
	$(MAKE) -C bin cleandepend
	$(MAKE) -C usr.bin cleandepend
	$(MAKE) -C tools cleandepend
