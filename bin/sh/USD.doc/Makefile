#	$NetBSD: Makefile,v 1.1 2010/08/22 01:58:16 perry Exp $
#	@(#)Makefile	8.1 (Berkeley) 8/14/93

DIR=	usd/03.shell
SRCS=	Rv7man t.mac t1 t2 t3 t4
MACROS= -ms

paper.ps: ${SRCS}
	${TOOL_REFER} -e -p ${SRCS} | \
	    ${TOOL_ROFF_PS} ${MACROS} > ${.TARGET}

.include <bsd.doc.mk>
