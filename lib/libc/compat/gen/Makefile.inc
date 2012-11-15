#	$NetBSD: Makefile.inc,v 1.15 2012/10/10 22:52:26 christos Exp $

.PATH: ${COMPATDIR}/gen
.if !defined(__MINIX)
SRCS+=compat_errlist.c compat_fts.c compat___fts13.c compat___fts30.c \
    compat___fts31.c compat_getmntinfo.c compat_glob.c compat___glob13.c \
    compat_opendir.c compat_readdir.c compat__readdir_unlocked30.c \
    compat_scandir.c compat_siglist.c compat_signame.c compat_sigsetops.c \
    compat_times.c compat_timezone.c compat_unvis.c compat_utmpx.c \
    compat__sys_errlist.c compat__sys_nerr.c compat__sys_siglist.c \
    compat_time.c compat_utime.c compat_devname.c compat_alphasort.c \
    compat_getpwent.c compat___fts32.c compat_utmp.c compat___fts50.c \
    compat___unvis13.c compat_syslog.c
.else
SRCS+= \
    compat___unvis13.c compat_syslog.c
.endif # !defined(__MINIX)
