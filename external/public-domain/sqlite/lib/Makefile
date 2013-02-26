# $NetBSD: Makefile,v 1.3 2013/02/15 07:59:36 martin Exp $

LIB=		sqlite3
INCS=		sqlite3.h sqlite3ext.h
INCSDIR=	/usr/include

SRCS=		sqlite3.c

CFLAGS+=	-DNDEBUG

#LSC: need bsd.own for the test
.include <bsd.own.mk>
.if defined(__MINIX) && ${ACTIVE_CC} == "clang"
# LSC FIXME: Clang seems unable to compile this when !-O0
DBG=		-O0
.endif #  defined(__MINIX) && ${ACTIVE_CC} == "clang"

FILES+=			sqlite3.pc
FILESOWN_sqlite3.pc=	${BINOWN}
FILESGRP_sqlite3.pc=	${BINGRP}
FILESMODE_sqlite3.pc=	${NONBINMODE}
FILESDIR_sqlite3.pc=	/usr/lib/pkgconfig

.include <bsd.lib.mk>
