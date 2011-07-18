#	$NetBSD: Makefile,v 1.2 2010/02/05 16:34:04 roy Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/8/93


CLEANFILES= terminfo.db
realall: terminfo.db
FILES=terminfo.db terminfo
.if defined(__MINIX)
FILESDIR=/usr/share/terminfo
TOOL_TIC=/usr/bin/tic
.else
FILESDIR=${BINDIR}/misc
.endif

terminfo.db: terminfo
	${_MKTARGET_CREATE}
	${TOOL_TIC} -ax -o "${.OBJDIR}/terminfo" "${.CURDIR}/terminfo" 

.include <bsd.prog.mk>
