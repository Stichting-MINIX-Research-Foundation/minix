#Generated from ./ansi/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./ansi ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../obj-ack/libc.a

../obj-ack/libc.a: ../obj-ack/libc.a(abort.o)
../obj-ack/libc.a: ../obj-ack/libc.a(abs.o)
../obj-ack/libc.a: ../obj-ack/libc.a(asctime.o)
../obj-ack/libc.a: ../obj-ack/libc.a(assert.o)
../obj-ack/libc.a: ../obj-ack/libc.a(atexit.o)
../obj-ack/libc.a: ../obj-ack/libc.a(atof.o)
../obj-ack/libc.a: ../obj-ack/libc.a(atoi.o)
../obj-ack/libc.a: ../obj-ack/libc.a(atol.o)
../obj-ack/libc.a: ../obj-ack/libc.a(bsearch.o)
../obj-ack/libc.a: ../obj-ack/libc.a(calloc.o)
../obj-ack/libc.a: ../obj-ack/libc.a(chartab.o)
../obj-ack/libc.a: ../obj-ack/libc.a(clock.o)
../obj-ack/libc.a: ../obj-ack/libc.a(ctime.o)
../obj-ack/libc.a: ../obj-ack/libc.a(difftime.o)
../obj-ack/libc.a: ../obj-ack/libc.a(div.o)
../obj-ack/libc.a: ../obj-ack/libc.a(errlist.o)
../obj-ack/libc.a: ../obj-ack/libc.a(exit.o)
../obj-ack/libc.a: ../obj-ack/libc.a(ext_comp.o)
../obj-ack/libc.a: ../obj-ack/libc.a(getenv.o)
../obj-ack/libc.a: ../obj-ack/libc.a(gmtime.o)
../obj-ack/libc.a: ../obj-ack/libc.a(isalnum.o)
../obj-ack/libc.a: ../obj-ack/libc.a(isalpha.o)
../obj-ack/libc.a: ../obj-ack/libc.a(isascii.o)
../obj-ack/libc.a: ../obj-ack/libc.a(iscntrl.o)
../obj-ack/libc.a: ../obj-ack/libc.a(isdigit.o)
../obj-ack/libc.a: ../obj-ack/libc.a(isgraph.o)
../obj-ack/libc.a: ../obj-ack/libc.a(islower.o)
../obj-ack/libc.a: ../obj-ack/libc.a(isprint.o)
../obj-ack/libc.a: ../obj-ack/libc.a(ispunct.o)
../obj-ack/libc.a: ../obj-ack/libc.a(isspace.o)
../obj-ack/libc.a: ../obj-ack/libc.a(isupper.o)
../obj-ack/libc.a: ../obj-ack/libc.a(isxdigit.o)
../obj-ack/libc.a: ../obj-ack/libc.a(labs.o)
../obj-ack/libc.a: ../obj-ack/libc.a(ldiv.o)
../obj-ack/libc.a: ../obj-ack/libc.a(localeconv.o)
../obj-ack/libc.a: ../obj-ack/libc.a(localtime.o)
../obj-ack/libc.a: ../obj-ack/libc.a(malloc.o)
../obj-ack/libc.a: ../obj-ack/libc.a(mblen.o)
../obj-ack/libc.a: ../obj-ack/libc.a(mbstowcs.o)
../obj-ack/libc.a: ../obj-ack/libc.a(mbtowc.o)
../obj-ack/libc.a: ../obj-ack/libc.a(misc.o)
../obj-ack/libc.a: ../obj-ack/libc.a(mktime.o)
../obj-ack/libc.a: ../obj-ack/libc.a(qsort.o)
../obj-ack/libc.a: ../obj-ack/libc.a(raise.o)
../obj-ack/libc.a: ../obj-ack/libc.a(rand.o)
../obj-ack/libc.a: ../obj-ack/libc.a(setlocale.o)
../obj-ack/libc.a: ../obj-ack/libc.a(sigmisc.o)
../obj-ack/libc.a: ../obj-ack/libc.a(signal.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strcoll.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strcspn.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strerror.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strftime.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strpbrk.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strspn.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strstr.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strtok.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strtol.o)
../obj-ack/libc.a: ../obj-ack/libc.a(strxfrm.o)
../obj-ack/libc.a: ../obj-ack/libc.a(system.o)
../obj-ack/libc.a: ../obj-ack/libc.a(tolower.o)
../obj-ack/libc.a: ../obj-ack/libc.a(toupper.o)
../obj-ack/libc.a: ../obj-ack/libc.a(tzset.o)
../obj-ack/libc.a: ../obj-ack/libc.a(wcstombs.o)
../obj-ack/libc.a: ../obj-ack/libc.a(wctomb.o)

../obj-ack/libc.a:
	ar cr ../obj-ack/libc.a ../obj-ack/./ansi/*.o
	rm ../obj-ack/./ansi/*.o

../obj-ack/libc.a(abort.o): abort.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/abort.o abort.c
../obj-ack/libc.a(abs.o): abs.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/abs.o abs.c
../obj-ack/libc.a(asctime.o): asctime.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/asctime.o asctime.c
../obj-ack/libc.a(assert.o): assert.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/assert.o assert.c
../obj-ack/libc.a(atexit.o): atexit.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/atexit.o atexit.c
../obj-ack/libc.a(atof.o): atof.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/atof.o atof.c
../obj-ack/libc.a(atoi.o): atoi.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/atoi.o atoi.c
../obj-ack/libc.a(atol.o): atol.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/atol.o atol.c
../obj-ack/libc.a(bsearch.o): bsearch.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/bsearch.o bsearch.c
../obj-ack/libc.a(calloc.o): calloc.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/calloc.o calloc.c
../obj-ack/libc.a(chartab.o): chartab.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/chartab.o chartab.c
../obj-ack/libc.a(clock.o): clock.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/clock.o clock.c
../obj-ack/libc.a(ctime.o): ctime.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/ctime.o ctime.c
../obj-ack/libc.a(difftime.o): difftime.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/difftime.o difftime.c
../obj-ack/libc.a(div.o): div.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/div.o div.c
../obj-ack/libc.a(errlist.o): errlist.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/errlist.o errlist.c
../obj-ack/libc.a(exit.o): exit.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/exit.o exit.c
../obj-ack/libc.a(ext_comp.o): ext_comp.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/ext_comp.o ext_comp.c
../obj-ack/libc.a(getenv.o): getenv.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/getenv.o getenv.c
../obj-ack/libc.a(gmtime.o): gmtime.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/gmtime.o gmtime.c
../obj-ack/libc.a(isalnum.o): isalnum.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/isalnum.o isalnum.c
../obj-ack/libc.a(isalpha.o): isalpha.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/isalpha.o isalpha.c
../obj-ack/libc.a(isascii.o): isascii.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/isascii.o isascii.c
../obj-ack/libc.a(iscntrl.o): iscntrl.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/iscntrl.o iscntrl.c
../obj-ack/libc.a(isdigit.o): isdigit.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/isdigit.o isdigit.c
../obj-ack/libc.a(isgraph.o): isgraph.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/isgraph.o isgraph.c
../obj-ack/libc.a(islower.o): islower.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/islower.o islower.c
../obj-ack/libc.a(isprint.o): isprint.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/isprint.o isprint.c
../obj-ack/libc.a(ispunct.o): ispunct.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/ispunct.o ispunct.c
../obj-ack/libc.a(isspace.o): isspace.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/isspace.o isspace.c
../obj-ack/libc.a(isupper.o): isupper.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/isupper.o isupper.c
../obj-ack/libc.a(isxdigit.o): isxdigit.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/isxdigit.o isxdigit.c
../obj-ack/libc.a(labs.o): labs.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/labs.o labs.c
../obj-ack/libc.a(ldiv.o): ldiv.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/ldiv.o ldiv.c
../obj-ack/libc.a(localeconv.o): localeconv.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/localeconv.o localeconv.c
../obj-ack/libc.a(localtime.o): localtime.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/localtime.o localtime.c
../obj-ack/libc.a(malloc.o): malloc.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/malloc.o malloc.c
../obj-ack/libc.a(mblen.o): mblen.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/mblen.o mblen.c
../obj-ack/libc.a(mbstowcs.o): mbstowcs.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/mbstowcs.o mbstowcs.c
../obj-ack/libc.a(mbtowc.o): mbtowc.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/mbtowc.o mbtowc.c
../obj-ack/libc.a(misc.o): misc.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/misc.o misc.c
../obj-ack/libc.a(mktime.o): mktime.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/mktime.o mktime.c
../obj-ack/libc.a(qsort.o): qsort.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/qsort.o qsort.c
../obj-ack/libc.a(raise.o): raise.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/raise.o raise.c
../obj-ack/libc.a(rand.o): rand.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/rand.o rand.c
../obj-ack/libc.a(setlocale.o): setlocale.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/setlocale.o setlocale.c
../obj-ack/libc.a(sigmisc.o): sigmisc.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/sigmisc.o sigmisc.c
../obj-ack/libc.a(signal.o): signal.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/signal.o signal.c
../obj-ack/libc.a(strcoll.o): strcoll.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strcoll.o strcoll.c
../obj-ack/libc.a(strcspn.o): strcspn.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strcspn.o strcspn.c
../obj-ack/libc.a(strerror.o): strerror.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strerror.o strerror.c
../obj-ack/libc.a(strftime.o): strftime.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strftime.o strftime.c
../obj-ack/libc.a(strpbrk.o): strpbrk.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strpbrk.o strpbrk.c
../obj-ack/libc.a(strspn.o): strspn.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strspn.o strspn.c
../obj-ack/libc.a(strstr.o): strstr.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strstr.o strstr.c
../obj-ack/libc.a(strtok.o): strtok.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strtok.o strtok.c
../obj-ack/libc.a(strtol.o): strtol.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strtol.o strtol.c
../obj-ack/libc.a(strxfrm.o): strxfrm.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/strxfrm.o strxfrm.c
../obj-ack/libc.a(system.o): system.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/system.o system.c
../obj-ack/libc.a(tolower.o): tolower.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/tolower.o tolower.c
../obj-ack/libc.a(toupper.o): toupper.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/toupper.o toupper.c
../obj-ack/libc.a(tzset.o): tzset.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/tzset.o tzset.c
../obj-ack/libc.a(wcstombs.o): wcstombs.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/wcstombs.o wcstombs.c
../obj-ack/libc.a(wctomb.o): wctomb.c
	cc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-ack/./ansi/wctomb.o wctomb.c

all-gnu: ../obj-gnu/libc.a

../obj-gnu/libc.a: ../obj-gnu/./ansi/abort.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/abs.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/asctime.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/assert.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/atexit.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/atof.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/atoi.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/atol.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/bsearch.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/calloc.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/chartab.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/clock.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/ctime.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/difftime.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/div.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/errlist.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/exit.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/ext_comp.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/getenv.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/gmtime.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/isalnum.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/isalpha.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/isascii.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/iscntrl.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/isdigit.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/isgraph.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/islower.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/isprint.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/ispunct.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/isspace.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/isupper.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/isxdigit.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/labs.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/ldiv.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/localeconv.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/localtime.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/malloc.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/mblen.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/mbstowcs.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/mbtowc.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/misc.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/mktime.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/qsort.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/raise.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/rand.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/setlocale.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/sigmisc.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/signal.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strcoll.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strcspn.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strerror.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strftime.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strpbrk.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strspn.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strstr.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strtok.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strtol.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/strxfrm.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/system.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/tolower.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/toupper.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/tzset.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/wcstombs.o
../obj-gnu/libc.a: ../obj-gnu/./ansi/wctomb.o

../obj-gnu/libc.a:
	gar cr ../obj-gnu/libc.a $?

../obj-gnu/./ansi/abort.o: abort.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/abort.o abort.c

../obj-gnu/./ansi/abs.o: abs.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/abs.o abs.c

../obj-gnu/./ansi/asctime.o: asctime.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/asctime.o asctime.c

../obj-gnu/./ansi/assert.o: assert.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/assert.o assert.c

../obj-gnu/./ansi/atexit.o: atexit.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/atexit.o atexit.c

../obj-gnu/./ansi/atof.o: atof.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/atof.o atof.c

../obj-gnu/./ansi/atoi.o: atoi.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/atoi.o atoi.c

../obj-gnu/./ansi/atol.o: atol.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/atol.o atol.c

../obj-gnu/./ansi/bsearch.o: bsearch.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/bsearch.o bsearch.c

../obj-gnu/./ansi/calloc.o: calloc.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/calloc.o calloc.c

../obj-gnu/./ansi/chartab.o: chartab.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/chartab.o chartab.c

../obj-gnu/./ansi/clock.o: clock.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/clock.o clock.c

../obj-gnu/./ansi/ctime.o: ctime.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/ctime.o ctime.c

../obj-gnu/./ansi/difftime.o: difftime.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/difftime.o difftime.c

../obj-gnu/./ansi/div.o: div.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/div.o div.c

../obj-gnu/./ansi/errlist.o: errlist.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/errlist.o errlist.c

../obj-gnu/./ansi/exit.o: exit.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/exit.o exit.c

../obj-gnu/./ansi/ext_comp.o: ext_comp.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/ext_comp.o ext_comp.c

../obj-gnu/./ansi/getenv.o: getenv.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/getenv.o getenv.c

../obj-gnu/./ansi/gmtime.o: gmtime.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/gmtime.o gmtime.c

../obj-gnu/./ansi/isalnum.o: isalnum.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/isalnum.o isalnum.c

../obj-gnu/./ansi/isalpha.o: isalpha.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/isalpha.o isalpha.c

../obj-gnu/./ansi/isascii.o: isascii.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/isascii.o isascii.c

../obj-gnu/./ansi/iscntrl.o: iscntrl.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/iscntrl.o iscntrl.c

../obj-gnu/./ansi/isdigit.o: isdigit.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/isdigit.o isdigit.c

../obj-gnu/./ansi/isgraph.o: isgraph.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/isgraph.o isgraph.c

../obj-gnu/./ansi/islower.o: islower.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/islower.o islower.c

../obj-gnu/./ansi/isprint.o: isprint.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/isprint.o isprint.c

../obj-gnu/./ansi/ispunct.o: ispunct.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/ispunct.o ispunct.c

../obj-gnu/./ansi/isspace.o: isspace.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/isspace.o isspace.c

../obj-gnu/./ansi/isupper.o: isupper.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/isupper.o isupper.c

../obj-gnu/./ansi/isxdigit.o: isxdigit.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/isxdigit.o isxdigit.c

../obj-gnu/./ansi/labs.o: labs.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/labs.o labs.c

../obj-gnu/./ansi/ldiv.o: ldiv.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/ldiv.o ldiv.c

../obj-gnu/./ansi/localeconv.o: localeconv.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/localeconv.o localeconv.c

../obj-gnu/./ansi/localtime.o: localtime.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/localtime.o localtime.c

../obj-gnu/./ansi/malloc.o: malloc.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/malloc.o malloc.c

../obj-gnu/./ansi/mblen.o: mblen.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/mblen.o mblen.c

../obj-gnu/./ansi/mbstowcs.o: mbstowcs.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/mbstowcs.o mbstowcs.c

../obj-gnu/./ansi/mbtowc.o: mbtowc.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/mbtowc.o mbtowc.c

../obj-gnu/./ansi/misc.o: misc.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/misc.o misc.c

../obj-gnu/./ansi/mktime.o: mktime.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/mktime.o mktime.c

../obj-gnu/./ansi/qsort.o: qsort.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/qsort.o qsort.c

../obj-gnu/./ansi/raise.o: raise.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/raise.o raise.c

../obj-gnu/./ansi/rand.o: rand.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/rand.o rand.c

../obj-gnu/./ansi/setlocale.o: setlocale.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/setlocale.o setlocale.c

../obj-gnu/./ansi/sigmisc.o: sigmisc.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/sigmisc.o sigmisc.c

../obj-gnu/./ansi/signal.o: signal.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/signal.o signal.c

../obj-gnu/./ansi/strcoll.o: strcoll.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strcoll.o strcoll.c

../obj-gnu/./ansi/strcspn.o: strcspn.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strcspn.o strcspn.c

../obj-gnu/./ansi/strerror.o: strerror.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strerror.o strerror.c

../obj-gnu/./ansi/strftime.o: strftime.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strftime.o strftime.c

../obj-gnu/./ansi/strpbrk.o: strpbrk.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strpbrk.o strpbrk.c

../obj-gnu/./ansi/strspn.o: strspn.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strspn.o strspn.c

../obj-gnu/./ansi/strstr.o: strstr.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strstr.o strstr.c

../obj-gnu/./ansi/strtok.o: strtok.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strtok.o strtok.c

../obj-gnu/./ansi/strtol.o: strtol.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strtol.o strtol.c

../obj-gnu/./ansi/strxfrm.o: strxfrm.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/strxfrm.o strxfrm.c

../obj-gnu/./ansi/system.o: system.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/system.o system.c

../obj-gnu/./ansi/tolower.o: tolower.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/tolower.o tolower.c

../obj-gnu/./ansi/toupper.o: toupper.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/toupper.o toupper.c

../obj-gnu/./ansi/tzset.o: tzset.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/tzset.o tzset.c

../obj-gnu/./ansi/wcstombs.o: wcstombs.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/wcstombs.o wcstombs.c

../obj-gnu/./ansi/wctomb.o: wctomb.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -D__USG -c -o ../obj-gnu/./ansi/wctomb.o wctomb.c




clean::
	rm -f ../obj-ack/./ansi/*
	rm -f ../obj-gnu/./ansi/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
