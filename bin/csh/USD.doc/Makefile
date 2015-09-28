#	$NetBSD: Makefile,v 1.7 2007/10/18 18:26:31 tls Exp $
#	@(#)Makefile	8.1 (Berkeley) 8/14/93

DIR=	usd/04.csh
SRCS=	tabs csh.1 csh.2 csh.3 csh.4 csh.ap csh.g
MACROS= -ms

paper.ps: ${SRCS}
	${TOOL_SOELIM} -I${.CURDIR} ${.ALLSRC} | \
	    ${TOOL_ROFF_PS} ${MACROS} > ${.TARGET}                  

.include <bsd.doc.mk>
