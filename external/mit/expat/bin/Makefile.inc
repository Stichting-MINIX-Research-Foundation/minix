#	$NetBSD: Makefile.inc,v 1.1 2012/02/29 20:07:56 tron Exp $

BINDIR?=	/usr/bin

CPPFLAGS+=	-I${EXPATSRCDIR}/lib

LDADD+=	-L${EXPATOBJDIR.expat} -lexpat
DPADD+=	${EXPATLIB.expat}

.include "../Makefile.inc"
