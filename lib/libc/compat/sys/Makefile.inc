#	$NetBSD: Makefile.inc,v 1.9 2013/03/29 02:09:58 christos Exp $

#__MINIX: Not everything is imported yet.
.PATH: ${COMPATDIR}/sys
SRCS+= compat_getdents.c compat_getdirentries.c \
    compat_sched.c \
    compat_stat.c \
    compat_socket.c \
    \
    compat_getrusage.c compat_gettimeofday.c compat_utimes.c \
    compat_wait4.c compat_settimeofday.c \
    compat_clock.c compat_itimer.c compat_nanosleep.c \
    compat_adjtime.c compat_mknod.c \
    \
    compat___shmctl13.c \
    
MAN+=	getdirentries.3

.if ${MKLINT} != "no"
LSRCS+= Lint_Ovfork.c
.endif
