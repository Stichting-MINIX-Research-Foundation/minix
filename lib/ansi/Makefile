# Makefile for lib/ansi.

# This Makefile compiles part of the C library, the functions required by the
# ANSI C standard.  This Makefile, and those in the other subdirectories use
# a little known feature of make, the ability to refer to a file within a
# library.  The construct 'libc.a(abs.o)' names the file 'abs.o' contained
# in 'libc.a'.  So the rule
#
#	libc.a(abs.o):	abs.c
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

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libc.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(abort.o) \
	$(LIBRARY)(abs.o) \
	$(LIBRARY)(asctime.o) \
	$(LIBRARY)(assert.o) \
	$(LIBRARY)(atexit.o) \
	$(LIBRARY)(atof.o) \
	$(LIBRARY)(atoi.o) \
	$(LIBRARY)(atol.o) \
	$(LIBRARY)(bsearch.o) \
	$(LIBRARY)(calloc.o) \
	$(LIBRARY)(chartab.o) \
	$(LIBRARY)(clock.o) \
	$(LIBRARY)(ctime.o) \
	$(LIBRARY)(difftime.o) \
	$(LIBRARY)(div.o) \
	$(LIBRARY)(errlist.o) \
	$(LIBRARY)(exit.o) \
	$(LIBRARY)(ext_comp.o) \
	$(LIBRARY)(getenv.o) \
	$(LIBRARY)(gmtime.o) \
	$(LIBRARY)(isalnum.o) \
	$(LIBRARY)(isalpha.o) \
	$(LIBRARY)(isascii.o) \
	$(LIBRARY)(iscntrl.o) \
	$(LIBRARY)(isdigit.o) \
	$(LIBRARY)(isgraph.o) \
	$(LIBRARY)(islower.o) \
	$(LIBRARY)(isprint.o) \
	$(LIBRARY)(ispunct.o) \
	$(LIBRARY)(isspace.o) \
	$(LIBRARY)(isupper.o) \
	$(LIBRARY)(isxdigit.o) \
	$(LIBRARY)(labs.o) \
	$(LIBRARY)(ldiv.o) \
	$(LIBRARY)(localeconv.o) \
	$(LIBRARY)(localtime.o) \
	$(LIBRARY)(malloc.o) \
	$(LIBRARY)(mblen.o) \
	$(LIBRARY)(mbstowcs.o) \
	$(LIBRARY)(mbtowc.o) \
	$(LIBRARY)(misc.o) \
	$(LIBRARY)(mktime.o) \
	$(LIBRARY)(qsort.o) \
	$(LIBRARY)(raise.o) \
	$(LIBRARY)(rand.o) \
	$(LIBRARY)(setlocale.o) \
	$(LIBRARY)(sigmisc.o) \
	$(LIBRARY)(signal.o) \
	$(LIBRARY)(strcoll.o) \
	$(LIBRARY)(strcspn.o) \
	$(LIBRARY)(strerror.o) \
	$(LIBRARY)(strftime.o) \
	$(LIBRARY)(strpbrk.o) \
	$(LIBRARY)(strspn.o) \
	$(LIBRARY)(strstr.o) \
	$(LIBRARY)(strtok.o) \
	$(LIBRARY)(strtol.o) \
	$(LIBRARY)(strxfrm.o) \
	$(LIBRARY)(system.o) \
	$(LIBRARY)(tolower.o) \
	$(LIBRARY)(toupper.o) \
	$(LIBRARY)(tzset.o) \
	$(LIBRARY)(wcstombs.o) \
	$(LIBRARY)(wctomb.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(abort.o):	abort.c
	$(CC1) abort.c

$(LIBRARY)(abs.o):	abs.c
	$(CC1) abs.c

$(LIBRARY)(asctime.o):	asctime.c
	$(CC1) asctime.c

$(LIBRARY)(assert.o):	assert.c
	$(CC1) assert.c

$(LIBRARY)(atexit.o):	atexit.c
	$(CC1) atexit.c

$(LIBRARY)(atof.o):	atof.c
	$(CC1) atof.c

$(LIBRARY)(atoi.o):	atoi.c
	$(CC1) atoi.c

$(LIBRARY)(atol.o):	atol.c
	$(CC1) atol.c

$(LIBRARY)(bsearch.o):	bsearch.c
	$(CC1) bsearch.c

$(LIBRARY)(calloc.o):	calloc.c
	$(CC1) calloc.c

$(LIBRARY)(chartab.o):	chartab.c
	$(CC1) chartab.c

$(LIBRARY)(clock.o):	clock.c
	$(CC1) clock.c

$(LIBRARY)(ctime.o):	ctime.c
	$(CC1) ctime.c

$(LIBRARY)(difftime.o):	difftime.c
	$(CC1) difftime.c

$(LIBRARY)(div.o):	div.c
	$(CC1) div.c

$(LIBRARY)(errlist.o):	errlist.c
	$(CC1) errlist.c

$(LIBRARY)(exit.o):	exit.c
	$(CC1) exit.c

$(LIBRARY)(ext_comp.o):	ext_comp.c
	$(CC1) ext_comp.c

$(LIBRARY)(getenv.o):	getenv.c
	$(CC1) getenv.c

$(LIBRARY)(gmtime.o):	gmtime.c
	$(CC1) gmtime.c

$(LIBRARY)(isalnum.o):	isalnum.c
	$(CC1) isalnum.c

$(LIBRARY)(isalpha.o):	isalpha.c
	$(CC1) isalpha.c

$(LIBRARY)(isascii.o):	isascii.c
	$(CC1) isascii.c

$(LIBRARY)(iscntrl.o):	iscntrl.c
	$(CC1) iscntrl.c

$(LIBRARY)(isdigit.o):	isdigit.c
	$(CC1) isdigit.c

$(LIBRARY)(isgraph.o):	isgraph.c
	$(CC1) isgraph.c

$(LIBRARY)(islower.o):	islower.c
	$(CC1) islower.c

$(LIBRARY)(isprint.o):	isprint.c
	$(CC1) isprint.c

$(LIBRARY)(ispunct.o):	ispunct.c
	$(CC1) ispunct.c

$(LIBRARY)(isspace.o):	isspace.c
	$(CC1) isspace.c

$(LIBRARY)(isupper.o):	isupper.c
	$(CC1) isupper.c

$(LIBRARY)(isxdigit.o):	isxdigit.c
	$(CC1) isxdigit.c

$(LIBRARY)(labs.o):	labs.c
	$(CC1) labs.c

$(LIBRARY)(ldiv.o):	ldiv.c
	$(CC1) ldiv.c

$(LIBRARY)(localeconv.o):	localeconv.c
	$(CC1) localeconv.c

$(LIBRARY)(localtime.o):	localtime.c
	$(CC1) localtime.c

$(LIBRARY)(malloc.o):	malloc.c
	$(CC1) malloc.c

$(LIBRARY)(mblen.o):	mblen.c
	$(CC1) mblen.c

$(LIBRARY)(mbstowcs.o):	mbstowcs.c
	$(CC1) mbstowcs.c

$(LIBRARY)(mbtowc.o):	mbtowc.c
	$(CC1) mbtowc.c

$(LIBRARY)(memchr.o):	memchr.c
	$(CC1) memchr.c

$(LIBRARY)(memcmp.o):	memcmp.c
	$(CC1) memcmp.c

$(LIBRARY)(memcpy.o):	memcpy.c
	$(CC1) memcpy.c

$(LIBRARY)(memmove.o):	memmove.c
	$(CC1) memmove.c

$(LIBRARY)(memset.o):	memset.c
	$(CC1) memset.c

$(LIBRARY)(misc.o):	misc.c
	$(CC1) misc.c

$(LIBRARY)(mktime.o):	mktime.c
	$(CC1) mktime.c

$(LIBRARY)(qsort.o):	qsort.c
	$(CC1) qsort.c

$(LIBRARY)(raise.o):	raise.c
	$(CC1) raise.c

$(LIBRARY)(rand.o):	rand.c
	$(CC1) rand.c

$(LIBRARY)(setlocale.o):	setlocale.c
	$(CC1) setlocale.c

$(LIBRARY)(sigmisc.o):	sigmisc.c
	$(CC1) sigmisc.c

$(LIBRARY)(signal.o):	signal.c
	$(CC1) signal.c

$(LIBRARY)(strcat.o):	strcat.c
	$(CC1) strcat.c

$(LIBRARY)(strchr.o):	strchr.c
	$(CC1) strchr.c

$(LIBRARY)(strcmp.o):	strcmp.c
	$(CC1) strcmp.c

$(LIBRARY)(strcoll.o):	strcoll.c
	$(CC1) strcoll.c

$(LIBRARY)(strcpy.o):	strcpy.c
	$(CC1) strcpy.c

$(LIBRARY)(strcspn.o):	strcspn.c
	$(CC1) strcspn.c

$(LIBRARY)(strerror.o):	strerror.c
	$(CC1) strerror.c

$(LIBRARY)(strftime.o):	strftime.c
	$(CC1) strftime.c

$(LIBRARY)(strlen.o):	strlen.c
	$(CC1) strlen.c

$(LIBRARY)(strncat.o):	strncat.c
	$(CC1) strncat.c

$(LIBRARY)(strncmp.o):	strncmp.c
	$(CC1) strncmp.c

$(LIBRARY)(strncpy.o):	strncpy.c
	$(CC1) strncpy.c

$(LIBRARY)(strpbrk.o):	strpbrk.c
	$(CC1) strpbrk.c

$(LIBRARY)(strrchr.o):	strrchr.c
	$(CC1) strrchr.c

$(LIBRARY)(strspn.o):	strspn.c
	$(CC1) strspn.c

$(LIBRARY)(strstr.o):	strstr.c
	$(CC1) strstr.c

$(LIBRARY)(strtok.o):	strtok.c
	$(CC1) strtok.c

$(LIBRARY)(strtol.o):	strtol.c
	$(CC1) strtol.c

$(LIBRARY)(strxfrm.o):	strxfrm.c
	$(CC1) strxfrm.c

$(LIBRARY)(system.o):	system.c
	$(CC1) system.c

$(LIBRARY)(tolower.o):	tolower.c
	$(CC1) tolower.c

$(LIBRARY)(toupper.o):	toupper.c
	$(CC1) toupper.c

$(LIBRARY)(tzset.o):	tzset.c
	$(CC1) tzset.c

$(LIBRARY)(wcstombs.o):	wcstombs.c
	$(CC1) wcstombs.c

$(LIBRARY)(wctomb.o):	wctomb.c
	$(CC1) wctomb.c
