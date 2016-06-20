#	$NetBSD: Makefile,v 1.7 2009/10/29 14:37:16 christos Exp $

NOMAN=# defined (must come before bsd.own.mk)

.ifndef HOSTPROG
.include <bsd.own.mk>
.endif

.PATH: ${NETBSDSRCDIR}/lib/libc/citrus

PROG=		mkcsmapper
SRCS=		yacc.y lex.l \
		citrus_db_factory.c citrus_bcs.c citrus_db_hash.c \
		citrus_lookup_factory.c citrus_pivot_factory.c
CPPFLAGS+=	-I. -I${.CURDIR} -I${NETBSDSRCDIR}/lib/libc \
		-I${NETBSDSRCDIR}/lib/libc/citrus \
		-I${NETBSDSRCDIR}/lib/libc/citrus/modules \
		-DHOSTPROG -DLIBC_SCCS
YHEADER=	1

.ifndef HOSTPROG
.include <bsd.prog.mk>
.endif
