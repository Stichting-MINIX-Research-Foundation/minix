# $NetBSD: Makefile,v 1.1 2012/11/11 17:37:34 alnsn Exp $

.include <bsd.own.mk>
.include <../../../external/bsd/sljit/Makefile.inc>

TESTSDIR=	${TESTSBASE}/lib/libbpfjit

TESTS_C+=	t_bpfjit

# XXX this variable doesn't belong to here
LIBBPFJITDIR!=	cd ${NETBSDSRCDIR}/lib/libbpfjit && ${PRINTOBJDIR}

LDADD+=		-L${LIBBPFJITDIR} -lbpfjit
DPADD+=		${LIBBPFJITDIR}/libbpfjit.a

LDADD+=		-L${LIBSLJITDIR} -lsljit
DPADD+=		${LIBSLJITDIR}/libsljit.a

LDADD+=		${LIBPCAP}

.include <bsd.test.mk>
