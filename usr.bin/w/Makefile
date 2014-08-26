#	$NetBSD: Makefile,v 1.21 2011/10/21 02:26:09 christos Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

PROG=	w
SRCS=	fmt.c pr_time.c w.c
MAN=	w.1 uptime.1
.if defined(__MINIX)
.PATH:	${NETBSDSRCDIR}/minix/usr.bin/w
SRCS+=	minix_proc.c
CPPFLAGS+= -I${NETBSDSRCDIR}/minix/usr.bin/w
.else
DPADD=	${LIBKVM} ${LIBUTIL}
LDADD=	-lkvm -lutil
.endif
LINKS=	${BINDIR}/w ${BINDIR}/uptime
CPPFLAGS+= -DSUPPORT_UTMP -DSUPPORT_UTMPX

.PATH: ${NETBSDSRCDIR}/bin/ps

COPTS.pr_time.c += -Wno-format-y2k

.include <bsd.prog.mk>
