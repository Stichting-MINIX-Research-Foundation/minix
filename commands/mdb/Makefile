#
# Makefile for mdb
#
#
# Edit as indicated below.
#
USR		=/usr
#
# (1) For Compiler and target system:
#
#
# For ANSI C and Minix 1.7.x 32-bit 
#
CC		=exec cc 
LD		=exec cc
LDFLAGS		=-i
TARGET		=mdb
STACK		=100000
#
# (2) If kernel and mm are not in "/usr/src" change this
#
SYSTEM	=$(USR)/src
#
# (3) Select Options
# 
# i)   For GNU_EXEC Support, uncomment:
#
#FOR_GNU		=gnu_sym.o gnu_load.o
#DEF_GNU		=-DGNU_SUPPORT
# 
# ii)  For tracing of syscalls, uncomment:
#
#FOR_SYSCALLS 	=syscalls.o decode.o ioctl.o
#DEF_SYSCALLS	=-DSYSCALLS_SUPPORT
#
# iii) For no debugging of mdb, uncomment:
#
#DEF_DEBUG	=-DNDEBUG

EXTRA_OBJS	=$(FOR_GNU) $(FOR_SYSCALLS)
EXTRA_DEFS	=$(DEF_GNU) $(DEF_SYSCALLS) $(DEF_DEBUG)

all:	$(TARGET)

CFLAGS	=-I$(SYSTEM) -I$(SYSTEM)/servers -I$(INCLUDE) -D_MINIX -D_POSIX_SOURCE $(EXTRA_DEFS)

# For various included files or system files
# 
INCLUDE		=$(USR)/include
KERNEL		=$(SYSTEM)/kernel
PTRACE		=$(INCLUDE)/sys/ptrace.h


# Header files from pm (used by core.c) 
#
MMFILES=	$(SYSTEM)/servers/pm/const.h \
		$(SYSTEM)/servers/pm/type.h \
		$(SYSTEM)/servers/pm/mproc.h

# Header files from system and kernel in "mdb.h" 
#
SYSFILES=	$(INCLUDE)/minix/config.h \
		$(INCLUDE)/minix/const.h \
		$(INCLUDE)/ansi.h \
		$(INCLUDE)/minix/type.h \
		$(INCLUDE)/limits.h \
		$(INCLUDE)/errno.h \
		$(INCLUDE)/sys/types.h \
		$(KERNEL)/const.h \
		$(KERNEL)/type.h \
		$(KERNEL)/proc.h

# Common objects 
#
OBJCOMMON	=mdb.o mdbexp.o kernel.o sym.o trace.o core.o misc.o io.o

# Common source
#
SRCCOMMON	=mdb.c mdbexp.c kernel.o sym.c trace.c core.c misc.c io.c

# Object files for PC
#
OBJPC		=$(OBJCOMMON) mdbdis86.o 

# Source file
#
SRCPC		=$(SRCCOMMON) mdbdis86.c


mdb:	$(OBJPC) $(EXTRA_OBJS)
	$(LD) $(LDFLAGS) -o mdb $(OBJPC) $(EXTRA_OBJS)
	install -S $(STACK) mdb

#
# Dependencies for objects
#
mdb.o:		mdb.c mdb.h $(SYSFILES) proto.h
mdbdis86.o:	mdbdis86.c mdb.h $(SYSFILES) proto.h
mdbexp.o:	mdbexp.c mdb.h $(SYSFILES) proto.h
sym.o:		sym.c mdb.h $(SYSFILES) proto.h
trace.o:	trace.c mdb.h $(PTRACE) $(SYSFILES) proto.h
core.o:		core.c mdb.h $(MMFILES) $(SYSFILES) proto.h
misc.o:		misc.c mdb.h  $(SYSFILES) proto.h
io.o:		io.c mdb.h $(SYSFILES) proto.h

syscalls.o:	syscalls.c mdb.h $(SYSFILES) proto.h
decode.o:	decode.c mdb.h $(INCLUDE)/minix/callnr.h $(SYSFILES) proto.h
ioctl.o:	ioctl.c mdb.h  $(SYSFILES) proto.h

gnu_sym.o:	gnu_sym.c mdb.h $(INCLUDE)/gnu/a.out.h $(SYSFILES) proto.h
gnu_load.o:	gnu_load.c $(INCLUDE)/gnu/a.out.h $(SYSFILES) proto.h

$(INCLUDE)/gnu/a.out.h:	a.out.h
		install -c -o bin a.out.h $(INCLUDE)/gnu


#
# install
#

install:	mdb 
		install -cs -o bin mdb /usr/bin

install_man:	mdb.1 ptrace.2
		install -c -o bin mdb.1 /usr/man/man1
		install -c -o bin ptrace.2 /usr/man/man2
clean:
		rm -f *.o mdb

