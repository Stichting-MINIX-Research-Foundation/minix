#	$NetBSD: Makefile,v 1.5 2008/08/09 06:48:17 rtr Exp $

LIBISPRIVATE=	yes
LIB=		Xext

.include "../Makefile.serverlib"

.include "Makefile.Xextbuiltin"
.include "Makefile.Xextmodule"

SRCS=	${SRCS.Xextbuiltin} ${SRCS.Xextmodule}

SUBDIR=	Xextbuiltin Xextmodule

.include <bsd.subdir.mk>
.include "Makefile.Xext"
