#	$NetBSD: Makefile,v 1.14 2009/10/29 14:36:14 christos Exp $

WARNS?= 1	# XXX -Wshadow -Wcast-qual issues

.include <bsd.own.mk>

PROG=		menuc
SRCS=		main.c parse.y scan.l avl.c mdb.c util.c
CPPFLAGS+=	-I. -I${.CURDIR}
YHEADER=

.if ${MKSHARE} != "no"
FILES=		menu_sys.def
FILESDIR=	/usr/share/misc
.endif

.ifndef HOSTPROG
LDADD+=		-ll
DPADD+=		${LIBL}
.endif

.include <bsd.prog.mk>
