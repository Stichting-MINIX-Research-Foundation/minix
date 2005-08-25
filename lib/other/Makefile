# Makefile for lib/other.

# The bxxx(), and *index() functions are not used, because they have assembly
# equivalents.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -I../../servers
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libc.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(_allocmem.o) \
	$(LIBRARY)(_freemem.o) \
	$(LIBRARY)(_brk.o) \
	$(LIBRARY)(_reboot.o) \
	$(LIBRARY)(_seekdir.o) \
	$(LIBRARY)(_svrctl.o) \
	$(LIBRARY)(_getsysinfo.o) \
	$(LIBRARY)(_getprocnr.o) \
	$(LIBRARY)(_devctl.o) \
	$(LIBRARY)(_findproc.o) \
	$(LIBRARY)(asynchio.o) \
	$(LIBRARY)(basename.o) \
	$(LIBRARY)(configfile.o) \
	$(LIBRARY)(crypt.o) \
	$(LIBRARY)(ctermid.o) \
	$(LIBRARY)(cuserid.o) \
	$(LIBRARY)(environ.o) \
	$(LIBRARY)(errno.o) \
	$(LIBRARY)(fdopen.o) \
	$(LIBRARY)(ffs.o) \
	$(LIBRARY)(fslib.o) \
	$(LIBRARY)(fsversion.o) \
	$(LIBRARY)(getgrent.o) \
	$(LIBRARY)(getlogin.o) \
	$(LIBRARY)(getopt.o) \
	$(LIBRARY)(getpagesize.o) \
	$(LIBRARY)(getpass.o) \
	$(LIBRARY)(getpwent.o) \
	$(LIBRARY)(getttyent.o) \
	$(LIBRARY)(getw.o) \
	$(LIBRARY)(hypot.o) \
	$(LIBRARY)(itoa.o) \
	$(LIBRARY)(loadname.o) \
	$(LIBRARY)(lock.o) \
	$(LIBRARY)(lrand.o) \
	$(LIBRARY)(lsearch.o) \
	$(LIBRARY)(memccpy.o) \
	$(LIBRARY)(mstats.o) \
	$(LIBRARY)(mtab.o) \
	$(LIBRARY)(nlist.o) \
	$(LIBRARY)(peekpoke.o) \
	$(LIBRARY)(popen.o) \
	$(LIBRARY)(putenv.o) \
	$(LIBRARY)(putw.o) \
	$(LIBRARY)(random.o) \
	$(LIBRARY)(setgroups.o) \
	$(LIBRARY)(settimeofday.o) \
	$(LIBRARY)(stderr.o) \
	$(LIBRARY)(strdup.o) \
	$(LIBRARY)(strtok_r.o) \
	$(LIBRARY)(swab.o) \
	$(LIBRARY)(syscall.o) \
	$(LIBRARY)(sysconf.o) \
	$(LIBRARY)(telldir.o) \
	$(LIBRARY)(termcap.o) \
	$(LIBRARY)(ttyname.o) \
	$(LIBRARY)(ttyslot.o) \
	$(LIBRARY)(v8regexp.o) \
	$(LIBRARY)(v8regsub.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(_allocmem.o):	_allocmem.c
	$(CC1) _allocmem.c

$(LIBRARY)(_freemem.o):	_freemem.c
	$(CC1) _freemem.c

$(LIBRARY)(_brk.o):	_brk.c
	$(CC1) _brk.c

$(LIBRARY)(_reboot.o):	_reboot.c
	$(CC1) _reboot.c

$(LIBRARY)(_seekdir.o):	_seekdir.c
	$(CC1) _seekdir.c

$(LIBRARY)(_svrctl.o):	_svrctl.c
	$(CC1) _svrctl.c

$(LIBRARY)(_devctl.o):	_devctl.c
	$(CC1) _devctl.c

$(LIBRARY)(_getprocnr.o):	_getprocnr.c
	$(CC1) _getprocnr.c

$(LIBRARY)(_findproc.o):	_findproc.c
	$(CC1) _findproc.c

$(LIBRARY)(_getsysinfo.o):	_getsysinfo.c
	$(CC1) _getsysinfo.c

$(LIBRARY)(asynchio.o):	asynchio.c
	$(CC1) asynchio.c

$(LIBRARY)(basename.o):	basename.c
	$(CC1) basename.c

$(LIBRARY)(bcmp.o):	bcmp.c
	$(CC1) bcmp.c

$(LIBRARY)(bcopy.o):	bcopy.c
	$(CC1) bcopy.c

$(LIBRARY)(bzero.o):	bzero.c
	$(CC1) bzero.c

$(LIBRARY)(configfile.o):	configfile.c
	$(CC1) configfile.c

$(LIBRARY)(crypt.o):	crypt.c
	$(CC1) crypt.c

$(LIBRARY)(ctermid.o):	ctermid.c
	$(CC1) ctermid.c

$(LIBRARY)(cuserid.o):	cuserid.c
	$(CC1) cuserid.c

$(LIBRARY)(environ.o):	environ.c
	$(CC1) environ.c

$(LIBRARY)(errno.o):	errno.c
	$(CC1) errno.c

$(LIBRARY)(fdopen.o):	fdopen.c
	$(CC1) fdopen.c

$(LIBRARY)(ffs.o):	ffs.c
	$(CC1) ffs.c

$(LIBRARY)(fslib.o):	fslib.c
	$(CC1) fslib.c

$(LIBRARY)(fsversion.o):	fsversion.c
	$(CC1) fsversion.c

$(LIBRARY)(getgrent.o):	getgrent.c
	$(CC1) getgrent.c

$(LIBRARY)(getlogin.o):	getlogin.c
	$(CC1) getlogin.c

$(LIBRARY)(getopt.o):	getopt.c
	$(CC1) getopt.c

$(LIBRARY)(getpagesize.o):	getpagesize.c
	$(CC1) getpagesize.c

$(LIBRARY)(getpass.o):	getpass.c
	$(CC1) getpass.c

$(LIBRARY)(getpwent.o):	getpwent.c
	$(CC1) getpwent.c

$(LIBRARY)(getttyent.o):	getttyent.c
	$(CC1) getttyent.c

$(LIBRARY)(getw.o):	getw.c
	$(CC1) getw.c

$(LIBRARY)(hypot.o):	hypot.c
	$(CC1) hypot.c

$(LIBRARY)(index.o):	index.c
	$(CC1) index.c

$(LIBRARY)(itoa.o):	itoa.c
	$(CC1) itoa.c

$(LIBRARY)(loadname.o):	loadname.c
	$(CC1) loadname.c

$(LIBRARY)(lock.o):	lock.c
	$(CC1) lock.c

$(LIBRARY)(lrand.o):	lrand.c
	$(CC1) lrand.c

$(LIBRARY)(lsearch.o):	lsearch.c
	$(CC1) lsearch.c

$(LIBRARY)(memccpy.o):	memccpy.c
	$(CC1) memccpy.c

$(LIBRARY)(mstats.o):	mstats.c
	$(CC1) mstats.c

$(LIBRARY)(mtab.o):	mtab.c
	$(CC1) mtab.c

$(LIBRARY)(nlist.o):	nlist.c
	$(CC1) nlist.c

$(LIBRARY)(peekpoke.o):	peekpoke.c
	$(CC1) peekpoke.c

$(LIBRARY)(popen.o):	popen.c
	$(CC1) popen.c

$(LIBRARY)(putenv.o):	putenv.c
	$(CC1) putenv.c

$(LIBRARY)(putw.o):	putw.c
	$(CC1) putw.c

$(LIBRARY)(rindex.o):	rindex.c
	$(CC1) rindex.c

$(LIBRARY)(random.o):	random.c
	$(CC1) random.c

$(LIBRARY)(setgroups.o):	setgroups.c
	$(CC1) setgroups.c

$(LIBRARY)(settimeofday.o):	settimeofday.c
	$(CC1) settimeofday.c

$(LIBRARY)(stderr.o):	stderr.c
	$(CC1) stderr.c

$(LIBRARY)(strdup.o):	strdup.c
	$(CC1) strdup.c

$(LIBRARY)(strtok_r.o):	strtok_r.c
	$(CC1) strtok_r.c

$(LIBRARY)(swab.o):	swab.c
	$(CC1) swab.c

$(LIBRARY)(syscall.o):	syscall.c
	$(CC1) syscall.c

$(LIBRARY)(sysconf.o):	sysconf.c
	$(CC1) sysconf.c

$(LIBRARY)(syslib.o):	syslib.c
	$(CC1) syslib.c

$(LIBRARY)(telldir.o):	telldir.c
	$(CC1) telldir.c

$(LIBRARY)(termcap.o):	termcap.c
	$(CC1) termcap.c

$(LIBRARY)(ttyname.o):	ttyname.c
	$(CC1) ttyname.c

$(LIBRARY)(ttyslot.o):	ttyslot.c
	$(CC1) ttyslot.c

$(LIBRARY)(v8regexp.o):	v8regexp.c
	$(CC1) v8regexp.c

$(LIBRARY)(v8regsub.o):	v8regsub.c
	$(CC1) v8regsub.c

$(LIBRARY)(writev.o):	writev.c
	$(CC1) writev.c
