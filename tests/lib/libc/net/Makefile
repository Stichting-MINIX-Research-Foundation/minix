# $NetBSD: Makefile,v 1.7 2012/09/15 16:22:58 plunky Exp $

.include <bsd.own.mk>

MKMAN=	no

#LSC: Unsupported on MINIX
#TESTS_SUBDIRS+=	getaddrinfo

TESTSDIR=	${TESTSBASE}/lib/libc/net

TESTS_C+=	t_getprotoent
#LSC: Unsupported on MINIX
#TESTS_C+=	t_ether_aton

#SRCS.t_ether_aton=	aton_ether_subr.c t_ether_aton.c

aton_ether_subr.c: gen_ether_subr ${NETBSDSRCDIR}/sys/net/if_ethersubr.c
	${HOST_SH} ${.ALLSRC} ${.TARGET}

TESTS_SH+=	t_nsdispatch
TESTS_SH+=	t_protoent
TESTS_SH+=	t_servent

BINDIR=		${TESTSDIR}

#LSC: Unsupported on MINIX
#PROGS+=		h_nsd_recurse
PROGS+=		h_protoent
PROGS+=		h_servent

LDADD.h_nsd_recurse+=	-lpthread

CLEANFILES+=	aton_ether_subr.c

.include <bsd.test.mk>
