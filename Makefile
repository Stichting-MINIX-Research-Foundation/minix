# Master Makefile to compile everything in /usr/src except the system.

MAKE	= exec make -$(MAKEFLAGS)

usage:
	@echo "" 
	@echo "Master Makefile for MINIX commands and utilities." 
	@echo "Root privileges are required for some actions." 
	@echo "" 
	@echo "Usage:" 
	@echo "	make world      # Compile everything (libraries & commands)" 
	@echo "	make includes   # Install include files from src/" 
	@echo "	make libraries  # Compile and install libraries" 
	@echo "	make cmds       # Compile all, commands, but don't install" 
	@echo "	make install    # Compile and install commands" 
	@echo "	make depend     # Generate required .depend files" 
	@echo "	make clean      # Remove all compiler results" 
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
world: includes depend libraries cmds install postinstall

includes:
	cd include && $(MAKE) install gcc

libraries:
	cd lib && $(MAKE) all install

cmds:
	if [ -f commands/Makefile ] ; then cd commands && $(MAKE) all; fi

install::
	if [ -f commands/Makefile ] ; then cd commands && $(MAKE) install; fi

depend::
	mkdep kernel
	mkdep servers
	mkdep drivers
	cd kernel && $(MAKE) $@
	cd servers && $(MAKE) $@
	cd drivers && $(MAKE) $@


clean::
	cd lib && $(MAKE) $@
	test ! -f commands/Makefile || { cd commands && $(MAKE) $@; }

etcfiles::
	cd etc && $(MAKE) install

clean::
	cd test && $(MAKE) $@

all install clean::
	cd boot && $(MAKE) $@
	cd man && $(MAKE) $@	# First manpages, then commands
	test ! -f commands/Makefile || { cd commands && $(MAKE) $@; }
	cd tools && $(MAKE) $@
	cd servers && $(MAKE) $@

postinstall:
	cd etc && $(MAKE) $@
