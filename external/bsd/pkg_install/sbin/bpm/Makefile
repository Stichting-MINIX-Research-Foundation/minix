# $NetBSD: Makefile,v 1.1 2008/09/30 19:19:56 joerg Exp $

SCRIPTS= bpm.sh
MAN= bpm.1

.include <bsd.init.mk>

.PATH:	${DIST}/bpm

CLEANFILES+=	bpm.sh

bpm.sh: ${DIST}/bpm/bpm.sh.in
	${TOOL_CAT} ${DIST}/bpm/bpm.sh.in > ${.TARGET}

.include <bsd.prog.mk>
