#	$NetBSD: Makefile,v 1.8 2015/01/07 22:24:03 pooka Exp $
#

.PATH: ${.CURDIR}/../rump_allserver

PROG=		rump_server
SRCS=		rump_allserver.c
NOMAN=		installed by ../rump_allserver

.if defined(__MINIX)
LDADD+=		-Wl,--whole-archive -lrumpkern_sysproxy -lrump \
		-lrumpuser -Wl,--no-whole-archive -lmthread
.else
LDADD+=		-Wl,--whole-archive -lrumpkern_sysproxy -lrump \
		-lrumpuser -Wl,--no-whole-archive -lpthread
.endif

.include <bsd.prog.mk>
