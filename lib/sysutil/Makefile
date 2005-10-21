# Makefile for lib/utils.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE

LIBRARIES = libsysutil

libsysutil_OBJECTS	= \
	kmalloc.o \
	kprintf.o \
	kputc.o \
	tickdelay.o \
	getuptime.o \
	env_get_prm.o \
	env_parse.o \
	env_panic.o \
	env_prefix.o \
	fkey_ctl.o \
	panic.o \
	report.o \
	taskcall.o \

include ../Makefile.inc
