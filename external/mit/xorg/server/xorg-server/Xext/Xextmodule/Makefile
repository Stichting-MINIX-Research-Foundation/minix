#	$NetBSD: Makefile,v 1.7 2013/09/12 15:36:14 joerg Exp $

LIBISPRIVATE=	yes
LIB=		Xextmodule

COPTS+=	${PICFLAGS}

.include "../../Makefile.serverlib"
.include "../Makefile.Xextmodule"

SRCS=	${SRCS.Xextmodule} shape.c ${SRCS.Xext}

.include "../Makefile.Xext"
