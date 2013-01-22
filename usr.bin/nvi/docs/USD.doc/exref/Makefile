#	$NetBSD: Makefile,v 1.1 2008/09/02 09:25:39 christos Exp $
#
# @(#)Makefile	8.8 (Berkeley) 10/10/96

.include "../../../Makefile.inc"
.PATH: ${DIST}/docs/exref

DIR=		usd/12.ex
SRCS=		ex.rm ex.summary
MACROS=		-ms
CLEANFILES=	summary.ps

all: paper.ps summary.ps

paper.ps: ex.rm
	${TOOL_TBL} ${.ALLSRC} | ${TOOL_ROFF_PS} ${MACROS} > ${.TARGET}

summary.ps: ex.summary
	${TOOL_TBL} ${.ALLSRC} | ${TOOL_ROFF_PS} ${MACROS} > ${.TARGET}

.include <bsd.doc.mk>
