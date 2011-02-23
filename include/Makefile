# Doing a make includes builds /usr/include

INCS=	alloca.h a.out.h ar.h assert.h configfile.h ctype.h \
	curses.h dirent.h env.h err.h errno.h fcntl.h fenv.h \
	float.h fnmatch.h fts.h getopt.h glob.h grp.h ifaddrs.h \
	inttypes.h libgen.h libutil.h limits.h locale.h \
	mathconst.h math.h midiparser.h netdb.h pwd.h regex.h \
	regexp.h setjmp.h sgtty.h signal.h stdarg.h stddef.h \
	stdint.h stdbool.h stdio.h stdlib.h string.h strings.h sysexits.h \
	syslog.h tar.h termcap.h time.h timers.h tools.h \
	ttyent.h ucontext.h unistd.h utime.h utmp.h wchar.h wctype.h \
	hgfs.h tzfile.h util.h fetch.h mpool.h ndbm.h db.h poll.h resolv.h \
	memory.h paths.h libexec.h complex.h
INCS+=	arpa/inet.h arpa/nameser.h
INCS+=	minix/a.out.h minix/cdrom.h minix/cpufeature.h \
	minix/crtso.h minix/dirent.h \
	minix/dir.h minix/dl_eth.h minix/dmap.h minix/driver.h \
	minix/acpi.h \
	minix/drivers.h minix/drvlib.h \
	minix/fslib.h \
	minix/mq.h minix/mthread.h \
	minix/netdriver.h minix/partition.h \
	minix/procfs.h \
	minix/queryparam.h \
	minix/sched.h minix/sound.h \
	minix/tty.h minix/types.h \
	minix/vfsif.h minix/vtreefs.h \
	minix/compiler-ack.h minix/sha2.h minix/sha1.h minix/md5.h \
	minix/audio_fw.h minix/hash.h minix/input.h \
	minix/devman.h minix/usb.h minix/usb_ch9.h 
	
INCS+=	net/hton.h net/if.h net/ioctl.h net/netlib.h
INCS+=	netinet/if_ether.h netinet/in.h netinet/tcp.h

.include <bsd.own.mk>

INCSDIR=        /usr/include

.include "${MINIXSRCDIR}/common/include/Makefile.inc"
.include "${MINIXSRCDIR}/common/include/Makefile.termios.inc"

SUBDIR=		sys arch

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
