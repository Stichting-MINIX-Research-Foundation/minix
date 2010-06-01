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
world: mkfiles includes depend libraries install
.elif ${COMPILER_TYPE} == "gnu"
world: mkfiles includes depend gnu-libraries install
.endif

mkfiles:
	cp etc/mk/*.mk /etc/mk/

includes:
	cd include && $(MAKE) includes

libraries:
	cd lib && sh ack_build.sh obj depend all install

MKHEADERS411=/usr/gnu/libexec/gcc/i386-pc-minix/4.1.1/install-tools/mkheaders
MKHEADERS443=/usr/gnu/libexec/gcc/i686-pc-minix/4.4.3/install-tools/mkheaders
gnu-includes: includes
	SHELL=/bin/sh; if [ -f $(MKHEADERS411) ] ; then sh -e $(MKHEADERS411) ; fi
	SHELL=/bin/sh; if [ -f $(MKHEADERS443) ] ; then sh -e $(MKHEADERS443) ; fi

gnu-libraries:
	cd lib && sh gnu_build.sh obj depend all install

commands:
	cd commands && $(MAKE) all

depend::
	cd boot && $(MAKE) depend
	cd commands && $(MAKE) depend
	cd kernel && $(MAKE) depend
	cd servers && $(MAKE) depend
	cd drivers && $(MAKE) depend

etcfiles::
	cd etc && $(MAKE) install

all::
	cd boot && $(MAKE) all
	cd commands && $(MAKE) all
	cd tools && $(MAKE) all

install::
	cd boot && $(MAKE) install
	cd man && $(MAKE) install makedb
	cd commands && $(MAKE) install
	cd share && $(MAKE) install
	cd tools && $(MAKE) install

clean::
	cd boot && $(MAKE) clean
	cd commands && $(MAKE) clean
	cd tools && $(MAKE) clean
	cd lib && sh ack_build.sh clean
	cd lib && sh gnu_build.sh clean
	cd test && $(MAKE) clean

cleandepend::
	cd boot && $(MAKE) cleandepend
	cd commands && $(MAKE) cleandepend
	cd tools && $(MAKE) cleandepend
