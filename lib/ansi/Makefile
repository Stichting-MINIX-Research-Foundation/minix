# Makefile for lib/ansi.

# This Makefile compiles part of the C library, the functions required by the
# ANSI C standard.  This Makefile, and those in the other subdirectories use
# a little known feature of make, the ability to refer to a file within a
# library.  The construct abs.o' names the file 'abs.o' contained
# in 'libc.a'.  So the rule
#
#	abs.o:	abs.c
#		cc -c abs.c
#		aal cr libc.a abs.o
#		rm abs.o
#
# compiles abs.c and installs the result abs.o in libc.a if abs.c is newer
# than the abs.o in the library.  This Makefile does not work like this
# precisely, it first compiles all changed source files and than installs
# them all in one 'aal' command.

# Many of the string functions in this directory are not used, because the
# have fast assembly implementations.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -D__USG

LIBRARIES = libc
libc_OBJECTS = \
	abort.o \
	abs.o \
	asctime.o \
	assert.o \
	atexit.o \
	atof.o \
	atoi.o \
	atol.o \
	bsearch.o \
	calloc.o \
	chartab.o \
	clock.o \
	ctime.o \
	difftime.o \
	div.o \
	errlist.o \
	exit.o \
	ext_comp.o \
	getenv.o \
	gmtime.o \
	isalnum.o \
	isalpha.o \
	isascii.o \
	iscntrl.o \
	isdigit.o \
	isgraph.o \
	islower.o \
	isprint.o \
	ispunct.o \
	isspace.o \
	isupper.o \
	isxdigit.o \
	labs.o \
	ldiv.o \
	localeconv.o \
	localtime.o \
	malloc.o \
	mblen.o \
	mbstowcs.o \
	mbtowc.o \
	misc.o \
	mktime.o \
	qsort.o \
	raise.o \
	rand.o \
	setlocale.o \
	sigmisc.o \
	signal.o \
	strcoll.o \
	strcspn.o \
	strerror.o \
	strftime.o \
	strpbrk.o \
	strspn.o \
	strstr.o \
	strtok.o \
	strtol.o \
	strxfrm.o \
	system.o \
	tolower.o \
	toupper.o \
	tzset.o \
	wcstombs.o \
	wctomb.o
		
include ../Makefile.inc
