#	$NetBSD: Makefile,v 1.3 2003/07/10 10:34:44 lukem Exp $
#
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

DIR=	psd/17.m4
SRCS=	m4.ms
MACROS=	-msU

paper.ps: ${SRCS}
	${TOOL_ROFF_PS} ${SRCS} > ${.TARGET}

.include <bsd.doc.mk>
