#	$NetBSD: Makefile,v 1.3 2012/06/03 23:19:11 joerg Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/8/93

CLEANFILES= terminfo.cdb
realall: terminfo.cdb
FILES=terminfo.cdb terminfo
.if defined(__MINIX)
FILESDIR=/usr/share/terminfo
.else
FILESDIR=${BINDIR}/misc
.endif

terminfo.cdb: terminfo ${TOOL_TIC}
	${_MKTARGET_CREATE}
	${TOOL_TIC} -ax -o ${.TARGET} "${.CURDIR}/terminfo" 
#LSC FIXME: Hack, on some platform TOOL_TIC automatically add 
#           .db to the filename...
	test -e ${.TARGET}.db && mv ${.TARGET}.db ${.TARGET}

.include <bsd.prog.mk>
