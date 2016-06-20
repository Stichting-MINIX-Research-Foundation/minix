#	$NetBSD: Makefile,v 1.16 2009/04/14 22:15:24 lukem Exp $

WARNS?=	2	# XXX -Wextra -Wsign-compare issues

.ifndef HOSTPROG
.include <bsd.own.mk>
.endif

.PATH: ${NETBSDSRCDIR}/lib/libc/locale ${NETBSDSRCDIR}/lib/libc/citrus ${NETBSDSRCDIR}/lib/libc/stdio

PROG=		mklocale
SRCS=		yacc.y lex.l mklocaledb.c fix_grouping.c \
		citrus_bcs_strtoul.c citrus_db_factory.c citrus_db_hash.c
CPPFLAGS+=	-I. -I${.CURDIR} \
		-I${NETBSDSRCDIR}/lib/libc/include \
		-I${NETBSDSRCDIR}/lib/libc/locale \
		-I${NETBSDSRCDIR}/lib/libc/citrus \
		-DNBCHAR_MAX=127 -DHOSTPROG
CPPFLAGS.citrus_bcs_strtoul.c+= \
		-I${NETBSDSRCDIR}/common/lib/libc/stdlib
YHEADER=	1

.ifndef HOSTPROG
.include <bsd.prog.mk>
.endif
