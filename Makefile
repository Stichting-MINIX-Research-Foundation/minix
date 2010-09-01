# Master Makefile to compile everything in /usr/src except the system.

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
	@echo "	make depend        # Generate required .depend files"
	@echo "	make gnu-includes  # Install include files for GCC"
	@echo "	make gnu-libraries # Compile and install libraries for GCC"
	@echo "	make clang-libraries # Compile and install libraries for GCC with clang"
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
.if ${COMPILER_TYPE} == "ack"
world: mkfiles includes depend libraries install etcforce
.elif ${COMPILER_TYPE} == "gnu"
world: mkfiles includes depend gnu-libraries install etcforce
.endif

mkfiles:
	make -C share/mk install

includes:
	$(MAKE) -C include includes
	$(MAKE) -C lib includes

libraries: includes
	$(MAKE) -C lib build_ack

MKHEADERS411=/usr/gnu/libexec/gcc/i386-pc-minix/4.1.1/install-tools/mkheaders
MKHEADERS443=/usr/gnu/libexec/gcc/i686-pc-minix/4.4.3/install-tools/mkheaders
MKHEADERS443_PKGSRC=/usr/pkg/gcc44/libexec/gcc/i686-pc-minix/4.4.3/install-tools/mkheaders
gnu-includes: includes
	SHELL=/bin/sh; if [ -f $(MKHEADERS411) ] ; then sh -e $(MKHEADERS411) ; fi
	SHELL=/bin/sh; if [ -f $(MKHEADERS443) ] ; then sh -e $(MKHEADERS443) ; fi
	SHELL=/bin/sh; if [ -f $(MKHEADERS443_PKGSRC) ] ; then sh -e $(MKHEADERS443_PKGSRC) ; fi

gnu-libraries: gnu-includes
	$(MAKE) -C lib build_gnu

clang-libraries: includes
	$(MAKE) -C lib build_clang

commands: includes libraries
	$(MAKE) -C commands all

depend::
	$(MAKE) -C boot depend
	$(MAKE) -C commands depend
	$(MAKE) -C kernel depend
	$(MAKE) -C servers depend
	$(MAKE) -C drivers depend

etcfiles::
	$(MAKE) -C etc install

etcforce::
	$(MAKE) -C etc installforce

all::
	$(MAKE) -C boot all
	$(MAKE) -C commands all
	$(MAKE) -C tools all

install::
	$(MAKE) -C boot install
	$(MAKE) -C man install makedb
	$(MAKE) -C commands install
	$(MAKE) -C share install
	$(MAKE) -C tools install

clean::
	$(MAKE) -C boot clean
	$(MAKE) -C commands clean
	$(MAKE) -C tools clean
	$(MAKE) -C lib clean_gnu
	$(MAKE) -C lib clean_ack
	$(MAKE) -C test clean

cleandepend::
	$(MAKE) -C boot cleandepend
	$(MAKE) -C commands cleandepend
	$(MAKE) -C tools cleandepend
