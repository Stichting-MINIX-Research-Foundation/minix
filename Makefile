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
	@echo "	make libraries     # Compile and install libraries"
	@echo "	make commands      # Compile all, commands, but don't install"
	@echo "	make install       # Compile and install commands"
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
world: mkfiles etcfiles includes libraries dep-all install etcforce

mkfiles:
	make -C share/mk install

includes:
	$(MAKE) -C include includes
	$(MAKE) -C lib includes NBSD_LIBC=yes

MKHEADERSS=/usr/pkg/gcc*/libexec/gcc/*/*/install-tools/mkheaders
gnu-includes: includes
	SHELL=/bin/sh; for d in $(MKHEADERSS); do if [ -f $$d ] ; then sh -e $$d ; fi; done

libraries: includes
	$(MAKE) -C lib dependall install

commands: includes libraries
	$(MAKE) -C commands all
	$(MAKE) -C bin all
	$(MAKE) -C sbin all
	$(MAKE) -C usr.bin all
	$(MAKE) -C libexec all
	$(MAKE) -C usr.sbin all

dep-all:
	$(MAKE) -C sys dependall
	$(MAKE) -C commands dependall
	$(MAKE) -C bin dependall
	$(MAKE) -C sbin dependall
	$(MAKE) -C usr.bin dependall
	$(MAKE) -C libexec dependall
	$(MAKE) -C usr.sbin dependall
	$(MAKE) -C kernel dependall
	$(MAKE) -C servers dependall
	$(MAKE) -C drivers dependall

etcfiles:
	$(MAKE) -C etc install

etcforce:
	$(MAKE) -C etc installforce

all:
	$(MAKE) -C sys all
	$(MAKE) -C commands all
	$(MAKE) -C bin all
	$(MAKE) -C sbin all
	$(MAKE) -C usr.bin all
	$(MAKE) -C libexec all
	$(MAKE) -C usr.sbin all
	$(MAKE) -C tools all

install:
	$(MAKE) -C sys install
	$(MAKE) -C libexec install
	$(MAKE) -C man install makedb
	$(MAKE) -C commands install
	$(MAKE) -C bin install
	$(MAKE) -C sbin install
	$(MAKE) -C usr.bin install
	$(MAKE) -C usr.sbin install
	$(MAKE) -C servers install
	$(MAKE) -C share install
	$(MAKE) -C tools install

clean: mkfiles
	$(MAKE) -C sys clean
	$(MAKE) -C commands clean
	$(MAKE) -C bin clean
	$(MAKE) -C sbin clean
	$(MAKE) -C usr.bin clean
	$(MAKE) -C libexec clean
	$(MAKE) -C usr.sbin clean
	$(MAKE) -C share clean
	$(MAKE) -C tools clean
	$(MAKE) -C lib clean
	$(MAKE) -C test clean

cleandepend: mkfiles
	$(MAKE) -C lib cleandepend
	$(MAKE) -C sys cleandepend
	$(MAKE) -C commands cleandepend
	$(MAKE) -C bin cleandepend
	$(MAKE) -C sbin cleandepend
	$(MAKE) -C usr.bin cleandepend
	$(MAKE) -C libexec cleandepend
	$(MAKE) -C usr.sbin cleandepend
	$(MAKE) -C tools cleandepend

# Warn usage change
elf-libraries:
	echo "That target is just libraries now."
	false
