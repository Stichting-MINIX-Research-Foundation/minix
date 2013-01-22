#	$NetBSD: Makefile,v 1.1 2008/09/02 09:25:39 christos Exp $
#
#	@(#)Makefile	8.7 (Berkeley) 8/18/96

.include "../../../Makefile.inc"
.PATH: ${DIST}/docs/vitut
DIR=		usd/12.vi
SRCS=		vi.in vi.chars
MACROS=		-ms
CLEANFILES+=	vitut.ps summary.ps viapwh.ps

all: vitut.ps summary.ps viapwh.ps

vitut.ps: ${SRCS}
	${TOOL_TBL} ${.ALLSRC} | ${TOOL_ROFF_PS} ${MACROS} > $@
	chmod 444 $@

summary.ps: vi.summary
	${TOOL_TBL} ${.ALLSRC} | ${TOOL_ROFF_PS} ${MACROS} > $@
	chmod 444 $@

viapwh.ps: vi.apwh.ms
	${TOOL_TBL} ${.ALLSRC} | ${TOOL_ROFF_PS} ${MACROS} > $@
	chmod 444 $@

.include <bsd.doc.mk>
