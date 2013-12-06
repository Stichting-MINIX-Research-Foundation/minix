#	$NetBSD: Makefile.inc,v 1.1 2009/12/19 05:52:01 thorpej Exp $

.include <bsd.own.mk>

LIBELF_DIR=	${NETBSDSRCDIR}/external/bsd/libelf/dist

CPPFLAGS+=	-I${LIBELF_DIR}

WARNS?=		4

.PATH:		${LIBELF_DIR}
