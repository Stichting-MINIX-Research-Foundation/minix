#	$NetBSD: Makefile.inc,v 1.184 2012/04/12 22:08:32 christos Exp $
#	from: @(#)Makefile.inc	8.6 (Berkeley) 5/4/95

# gen sources
.if defined(__MINIX)
.PATH: ${.CURDIR}/gen/minix
.endif
.PATH: ${ARCHDIR}/gen ${.CURDIR}/gen

.if defined(__MINIX)
# Unsupported by Minix.
# 	closefrom.c confstr.c extattr.c getdevmajor.c getmntinfo.c \
#	pthread_atfork.c setdomainname.c sethostname.c setproctitle.c \
#	sysctl.c sysctlbyname.c sysctlgetmibinfo.c sysctlnametomib.c \
#	devname.c wait3.c

# To be ported
# nlist.c nlist_aout.c nlist_coff.c nlist_ecoff.c nlist_elf32.c nlist_elf64.c
#
# Not useful but portable
#	disklabel.c
#
# Already in getprogname
#	setprogname.c

SRCS+= 	_errno.c alarm.c alphasort.c arc4random.c assert.c basename.c clock.c \
	closedir.c \
	ctermid.c ctype_.c daemon.c \
	dehumanize_number.c dirname.c err.c errx.c \
	errlist.c errno.c execl.c execle.c execlp.c execv.c execvp.c \
	fmtcheck.c fmtmsg.c fnmatch.c fstab.c ftok.c \
	fts.c ftw.c getbsize.c getcap.c getcwd.c \
	getdomainname.c getgrent.c \
	getgrouplist.c getgroupmembership.c gethostname.c \
	getloadavg.c getlogin.c \
	getnetgrent.c getpagesize.c \
	getpass.c getprogname.c getpwent.c getttyent.c \
	getusershell.c glob.c humanize_number.c initdir.c initgroups.c \
	isascii.c isatty.c isctype.c lockf.c nftw.c \
	nice.c \
	opendir.c pause.c popen.c \
	psignal.c \
	ptree.c pwcache.c pw_scan.c raise.c randomid.c rb.c readdir.c \
	rewinddir.c scandir.c seekdir.c \
	setjmperr.c setmode.c \
	shquote.c shquotev.c sighold.c sigignore.c siginterrupt.c \
	siglist.c signal.c signame.c sigrelse.c \
	sigset.c sigsetops.c sleep.c \
	stringlist.c sysconf.c \
	syslog.c telldir.c time.c \
	times.c toascii.c tolower_.c ttyname.c ttyslot.c \
	toupper_.c ualarm.c ulimit.c uname.c unvis.c usleep.c utime.c utmp.c \
	utmpx.c valloc.c vis.c wait.c waitpid.c warn.c warnx.c \
	vwarn.c vwarnx.c verr.c verrx.c wordexp.c
.endif

# indirect reference stubs, to be removed soon.
SRCS+=	_err.c _errx.c \
	_sysconf.c _verr.c _verrx.c _vwarn.c _vwarnx.c _warn.c _warnx.c

# uses alloca
COPTS.execvp.c = -Wno-stack-protector
COPTS.execl.c = -Wno-stack-protector
COPTS.execle.c = -Wno-stack-protector
COPTS.execlp.c = -Wno-stack-protector
COPTS.syslog.c = -Wno-format-nonliteral

# machine-dependent gen sources
# m-d Makefile.inc must include sources for:
#	_setjmp() bswap16() bswap32() bswap64() fabs() infinity
#	isinf() setjmp() sigsetjmp()

.include "${ARCHDIR}/gen/Makefile.inc"

MAN+=	alarm.3 arc4random.3 basename.3 bswap.3 clock.3 closefrom.3 \
	confstr.3 \
	cpuset.3 ctermid.3 ctype.3 daemon.3 devname.3 directory.3 dirname.3 \
	endutxent.3 err.3 exec.3 extattr_copy_file.3 extattr_namespace_to_string.3 \
	fmtcheck.3 fmtmsg.3 fnmatch.3 fpclassify.3 fpgetmask.3 \
	ftok.3 fts.3 ftw.3 \
	getbsize.3 cgetcap.3 getcwd.3 getdevmajor.3 \
	getdomainname.3 getdiskbyname.3 getfsent.3 \
	getgrent.3 getgrouplist.3 gethostname.3 getlastlogx.3 getloadavg.3 \
	getmntinfo.3 getnetgrent.3 getpagesize.3 getpass.3 \
	getprogname.3 getpwent.3 getttyent.3 getusershell.3 glob.3 \
	humanize_number.3 \
	initgroups.3 isalnum.3 isalpha.3 isascii.3 isblank.3 iscntrl.3 \
	isdigit.3 isfinite.3 isgraph.3 isgreater.3 isinf.3 islower.3 isnan.3 \
	isnormal.3 isprint.3 ispunct.3 isspace.3 isupper.3 isxdigit.3 \
	lockf.3 _lwp_makecontext.3 makecontext.3 \
	nice.3 nlist.3 \
	pause.3 popen.3 \
	posix_spawn.3 posix_spawn_file_actions_addopen.3 \
	posix_spawn_file_actions_init.3 posix_spawnattr_getflags.3 \
	posix_spawnattr_getpgroup.3 posix_spawnattr_getschedparam.3 \
	posix_spawnattr_getschedpolicy.3 posix_spawnattr_init.3 \
	posix_spawnattr_getsigdefault.3 posix_spawnattr_getsigmask.3 \
	psignal.3 pwcache.3 pthread_atfork.3 \
	raise.3 randomid.3 realpath.3 scandir.3 setjmp.3 setmode.3 \
	setproctitle.3 shquote.3 sighold.3 sigignore.3 siginterrupt.3 \
	signal.3 signbit.3 sigrelse.3 sigset.3 sigsetops.3 sleep.3 \
	stringlist.3 sysconf.3 sysctl.3 syslog.3 time.3 times.3 \
	timezone.3 toascii.3 tolower.3 toupper.3 ttyname.3 \
	ualarm.3 ulimit.3 uname.3 unvis.3 usleep.3 utime.3 valloc.3 vis.3 \
	wordexp.3

MLINKS+=bswap.3 bswap16.3 bswap.3 bswap32.3 bswap.3 bswap64.3
MLINKS+=cpuset.3 cpuset_create.3 cpuset.3 cpuset_destroy.3 \
	cpuset.3 cpuset_zero.3 cpuset.3 cpuset_set.3 cpuset.3 cpuset_clr.3 \
	cpuset.3 cpuset_isset.3 cpuset.3 cpuset_size.3
MLINKS+=directory.3 closedir.3 directory.3 dirfd.3 directory.3 opendir.3 \
	directory.3 readdir.3 directory.3 readdir_r.3 directory.3 \
	rewinddir.3 directory.3 seekdir.3 directory.3 telldir.3 \
	directory.3 fdopendir.3	
MLINKS+=endutxent.3 getutxent.3 endutxent.3 getutxid.3 \
	endutxent.3 getutxline.3 endutxent.3 pututxline.3 \
	endutxent.3 setutxent.3
MLINKS+=err.3 verr.3 err.3 errx.3 err.3 verrx.3 err.3 warn.3 err.3 vwarn.3 \
        err.3 warnx.3 err.3 vwarnx.3
MLINKS+=exec.3 execl.3 exec.3 execle.3 exec.3 execlp.3 exec.3 execv.3 \
	exec.3 execvp.3 exec.3 exect.3
MLINKS+=extattr_namespace_to_string.3 extattr_string_to_namespace.3 \
	extattr_copy_file.3 extattr_copy_fd.3 \
	extattr_copy_file.3 extattr_copy_link.3 \
	extattr_copy_file.3 cpxattr.3 \
	extattr_copy_file.3 fcpxattr.3 \
	extattr_copy_file.3 lcpxattr.3
MLINKS+=fpgetmask.3 fpgetround.3 fpgetmask.3 fpgetsticky.3 \
	fpgetmask.3 fpsetmask.3  fpgetmask.3 fpsetround.3 \
	fpgetmask.3 fpsetsticky.3 fpgetmask.3 fpgetprec.3 \
	fpgetmask.3 fpsetprec.3
MLINKS+=fts.3 fts_open.3 fts.3 fts_read.3 fts.3 fts_children.3 \
	fts.3 fts_set.3 fts.3 fts_close.3
MLINKS+=ftw.3 nftw.3
MLINKS+=cgetcap.3 cgetclose.3 cgetcap.3 cgetent.3 \
	cgetcap.3 cgetfirst.3 cgetcap.3 cgetmatch.3 cgetcap.3 cgetnext.3 \
	cgetcap.3 cgetnum.3 cgetcap.3 cgetset.3 cgetcap.3 cgetstr.3 \
	cgetcap.3 cgetustr.3
MLINKS+=getcwd.3 getwd.3
MLINKS+=getpass.3 getpass_r.3
MLINKS+=getpass.3 getpassfd.3
MLINKS+=getdiskbyname.3 setdisktab.3
MLINKS+=getdomainname.3 setdomainname.3
MLINKS+=getfsent.3 endfsent.3 getfsent.3 getfsfile.3 getfsent.3 getfsspec.3 \
	getfsent.3 setfsent.3
MLINKS+=getgrent.3 endgrent.3 getgrent.3 setgroupent.3 getgrent.3 getgrgid.3 \
	getgrent.3 getgrnam.3 getgrent.3 setgrent.3
MLINKS+=getgrent.3 getgrgid_r.3 getgrent.3 getgrnam_r.3 getgrent.3 getgrent_r.3
MLINKS+=getgrouplist.3 getgroupmembership.3
MLINKS+=gethostname.3 sethostname.3
MLINKS+=getlastlogx.3 getutmp.3 getlastlogx.3 getutmpx.3
MLINKS+=getlastlogx.3 updlastlogx.3 getlastlogx.3 updwtmpx.3
MLINKS+=getlastlogx.3 utmpxname.3
MLINKS+=getnetgrent.3 endnetgrent.3 getnetgrent.3 innetgr.3 \
	getnetgrent.3 setnetgrent.3
MLINKS+=getprogname.3 setprogname.3
MLINKS+=getpwent.3 endpwent.3 getpwent.3 setpassent.3 getpwent.3 getpwnam.3 \
	getpwent.3 getpwuid.3 getpwent.3 setpwent.3
MLINKS+=getpwent.3 getpwnam_r.3 getpwent.3 getpwuid_r.3 getpwent.3 getpwent_r.3
# getpwent.3 setpwfile.3 - deprecated
MLINKS+=getttyent.3 endttyent.3 getttyent.3 getttynam.3 \
	getttyent.3 setttyent.3 getttyent.3 setttyentpath.3
MLINKS+=getusershell.3 endusershell.3 getusershell.3 setusershell.3
MLINKS+=glob.3 globfree.3
MLINKS+=glob.3 glob_pattern_p.3
MLINKS+=humanize_number.3 dehumanize_number.3
MLINKS+=makecontext.3 swapcontext.3
MLINKS+=popen.3 pclose.3
MLINKS+=posix_spawn.3 posix_spawnp.3 \
	posix_spawn_file_actions_addopen.3 posix_spawn_file_actions_addclose.3 \
	posix_spawn_file_actions_addopen.3 posix_spawn_file_actions_adddup2.3 \
	posix_spawn_file_actions_init.3 posix_spawn_file_actions_destroy.3 \
	posix_spawnattr_getflags.3 posix_spawnattr_setflags.3 \
	posix_spawnattr_getpgroup.3 posix_spawnattr_setpgroup.3 \
	posix_spawnattr_getschedparam.3 posix_spawnattr_setschedparam.3 \
	posix_spawnattr_getschedpolicy.3 posix_spawnattr_setschedpolicy.3 \
	posix_spawnattr_getsigdefault.3 posix_spawnattr_setsigdefault.3 \
	posix_spawnattr_getsigmask.3 posix_spawnattr_setsigmask.3 \
	posix_spawnattr_init.3 posix_spawnattr_destroy.3
MLINKS+=psignal.3 sys_siglist.3 psignal.3 sys_signame.3 psignal.3 psiginfo.3
MLINKS+=pwcache.3 user_from_uid.3 pwcache.3 group_from_gid.3
MLINKS+=pwcache.3 uid_from_user.3 pwcache.3 gid_from_group.3
MLINKS+=pwcache.3 pwcache_userdb.3 pwcache.3 pwcache_groupdb.3
MLINKS+=randomid.3 randomid_new.3 randomid.3 randomid_delete.3
MLINKS+=scandir.3 alphasort.3
MLINKS+=setjmp.3 _longjmp.3 setjmp.3 _setjmp.3 setjmp.3 longjmp.3 \
	setjmp.3 longjmperror.3 setjmp.3 sigsetjmp.3 setjmp.3 siglongjmp.3
MLINKS+=setmode.3 getmode.3
MLINKS+=shquote.3 shquotev.3
MLINKS+=sigsetops.3 sigemptyset.3 sigsetops.3 sigfillset.3 \
	sigsetops.3 sigaddset.3 sigsetops.3 sigdelset.3 \
	sigsetops.3 sigismember.3
MLINKS+=stringlist.3 sl_init.3 stringlist.3 sl_free.3 stringlist.3 sl_add.3 \
	stringlist.3 sl_find.3
MLINKS+=sysctl.3 sysctlbyname.3 sysctl.3 sysctlgetmibinfo.3 \
	sysctl.3 sysctlnametomib.3
MLINKS+=syslog.3 closelog.3 syslog.3 openlog.3 syslog.3 setlogmask.3 \
	syslog.3 vsyslog.3
MLINKS+=syslog.3 closelog_r.3 syslog.3 openlog_r.3 syslog.3 setlogmask_r.3 \
	syslog.3 syslog_r.3 syslog.3 vsyslog_r.3
MLINKS+=syslog.3 syslogp.3 syslog.3 syslogp_r.3 \
	syslog.3 vsyslogp.3 syslog.3 vsyslogp_r.3
MLINKS+=ttyname.3 isatty.3 ttyname.3 ttyslot.3 ttyname.3 ttyname_r.3
MLINKS+=unvis.3 strunvis.3 unvis.3 strunvisx.3
MLINKS+=unvis.3 strnunvis.3 unvis.3 strnunvisx.3
MLINKS+=vis.3 svis.3 vis.3 strvis.3 vis.3 strvisx.3 vis.3 strsvis.3 \
	vis.3 strsvisx.3 vis.3 nvis.3
MLINKS+=vis.3 snvis.3 vis.3 strnvis.3 vis.3 strnvisx.3 vis.3 strsnvis.3 \
	vis.3 strsnvisx.3

CPPFLAGS.ctype_.c+=	-I${LIBCDIR}/locale
CPPFLAGS.isctype.c+=	-I${LIBCDIR}/locale
CPPFLAGS.tolower_.c+=	-I${LIBCDIR}/locale
CPPFLAGS.toupper_.c+=	-I${LIBCDIR}/locale

errlist.c: errlist.awk ${NETBSDSRCDIR}/sys/sys/errno.h
.if defined(__MINIX)
	${TOOL_CAT} ${NETBSDSRCDIR}/sys/sys/errno.h | ${TOOL_SED} 's/(_SIGN//' | ${TOOL_AWK} -f ${NETBSDSRCDIR}/lib/libc/gen/errlist.awk > ${.TARGET}
.else
	${TOOL_AWK} -f ${.ALLSRC} > ${.TARGET}
.endif

CLEANFILES+= errlist.c
