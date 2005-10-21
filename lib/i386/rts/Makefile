# Makefile for lib/i386/rts.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE

LIBRARIES = libc

libc_OBJECTS	= \
	__sigreturn.o \
	_ipc.o \
	brksize.o \
	
TOPLEVEL_OBJECTS = \
	crtso.o \
	m2rtso.o \
	prtso.o \

include ../../Makefile.inc
