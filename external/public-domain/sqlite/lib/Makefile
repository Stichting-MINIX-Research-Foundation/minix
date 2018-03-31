# $NetBSD: Makefile,v 1.6 2014/03/07 18:37:48 christos Exp $

LIB=		sqlite3
INCS=		sqlite3.h sqlite3ext.h
INCSDIR=	/usr/include

SRCS=		sqlite3.c

CFLAGS+=	-DNDEBUG

FILES+=			sqlite3.pc
FILESOWN_sqlite3.pc=	${BINOWN}
FILESGRP_sqlite3.pc=	${BINGRP}
FILESMODE_sqlite3.pc=	${NONBINMODE}
FILESDIR_sqlite3.pc=	/usr/lib/pkgconfig

CLEANFILES+=sqlite3.pc

.include <bsd.lib.mk>

.if defined(__MINIX)
.if !empty(DBG:M-Og) || !empty(CFLAGS:M-Og) || \
	!empty(DBG:M-g) || !empty(CFLAGS:M-g)
#LSC: -Wno-maybe-uninitialized while compiling with -DNDEBUG -Og
CWARNFLAGS.gcc+=	-Wno-maybe-uninitialized
.endif
.endif # defined(__MINIX)

all: sqlite3.pc
dependall: all

sqlite3.pc: ${SRCDIR}/sqlite3.h sqlite3.pc.in
	@(V=$$( (echo '#include <sqlite3.h>'; echo SQLITE_VERSION) | \
	    ${HOST_CC} -E -I${SRCDIR} - | tail -1 | tr -d '"') && \
	    ${TOOL_SED} -e s/@VERSION@/$$V/ < ${.CURDIR}/sqlite3.pc.in \
	    > ${.TARGET})
