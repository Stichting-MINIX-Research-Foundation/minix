# Master Makefile to compile everything in /usr/src except the system.

.include <bsd.own.mk>

MAKE?=make

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
	@echo "	make clean         # Remove all compiler results"
	@echo "" 
	@echo "Run 'make' in releasetools/ to create a new MINIX configuration."
	@echo "" 

# world has to be able to make a new system, even if there
# is no complete old system. it has to install commands, for which
# it has to install libraries, for which it has to install includes,
# for which it has to install /etc (for users and ownerships).
# etcfiles also creates a directory hierarchy in its
# 'make install' target.
# 

# etcfiles has to be done first.

distribution: etcfiles includes mkfiles libraries do-libgcc .WAIT dep-all install etcforce

do-libgcc: .PHONY .MAKE
	${MAKEDIRTARGET} external/gpl3/gcc/lib/libgcc/libgcc all
	${MAKEDIRTARGET} external/gpl3/gcc/lib/libgcc/libgcc install

world: mkfiles etcfiles includes libraries dep-all install etcforce

# subdirs where userland utilities and other executables live
CMDSDIRS=commands bin sbin usr.bin usr.sbin libexec external

# subdirs where system stuff lives
SYSDIRS=sys kernel servers drivers

# combination
CMDSYSDIRS=$(CMDSDIRS) $(SYSDIRS)

etcfiles: .PHONY .MAKE
	${MAKEDIRTARGET} etc install

etcforce: .PHONY .MAKE
	${MAKEDIRTARGET} etc installforce

mkfiles: .PHONY .MAKE
	${MAKEDIRTARGET} share/mk install

includes: .PHONY .MAKE
	${MAKEDIRTARGET} include includes
	${INSTALL_DIR} ${DESTDIR}/usr/include/g++
	${MAKEDIRTARGET} lib includes
	${MAKEDIRTARGET} sys includes
	${MAKEDIRTARGET} external includes

.for dir in lib lib/csu lib/libc
do-${dir:S/\//-/g}: .PHONY .MAKE
	${MAKEDIRTARGET} ${dir} dependall
	${MAKEDIRTARGET} ${dir} install
.endfor

# libraries are built by building and installing csu, then libc, then
# the rest
libraries: includes .PHONY .MAKE do-lib-csu .WAIT do-lib-libc .WAIT do-lib

commands: includes libraries .PHONY .MAKE 
.for dir in $(CMDSDIRS)
	${MAKEDIRTARGET} ${dir} dependall
.endfor

dep-all: .PHONY .MAKE
.for dir in $(CMDSYSDIRS)
	${MAKEDIRTARGET} ${dir} dependall
.endfor

install: .PHONY .MAKE
.for dir in $(CMDSYSDIRS)
	${MAKEDIRTARGET} ${dir} install
.endfor
	${MAKEDIRTARGET} man install
	${MAKEDIRTARGET} man makedb
	${MAKEDIRTARGET} share install
	${MAKEDIRTARGET} releasetools install

clean: mkfiles .PHONY .MAKE
.for dir in $(CMDSDIRS)
	${MAKEDIRTARGET} ${dir} clean
.endfor
	${MAKEDIRTARGET} sys clean
	${MAKEDIRTARGET} releasetools clean
	${MAKEDIRTARGET} lib clean
	${MAKEDIRTARGET} test clean

cleandepend: mkfiles .PHONY .MAKE
.for dir in $(CMDSYSDIRS)
	${MAKEDIRTARGET} ${dir} cleandepend
.endfor
	${MAKEDIRTARGET} lib cleandepend

# Shorthands
all: .PHONY .MAKE dep-all
	${MAKEDIRTARGET} releasetools all

# Obsolete targets
elf-libraries: .PHONY
	echo "That target is just libraries now."
	false

gnu-includes: .PHONY
	echo "That target is obsolete."
	echo "Current MINIX GCC packages don't require it any more."
	false

