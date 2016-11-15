#	$NetBSD: Makefile.inc,v 1.2 2014/03/04 14:58:15 joerg Exp $

GENASSYM_CONF=	${S}/dev/arcbios/genassym.cf

.if !target(assym.h)
assym.h: ${GENASSYM_CONF} ${GENASSYM_EXTRA}
	${_MKTARGET_CREATE}
	cat ${GENASSYM_CONF} ${GENASSYM_EXTRA} | \
	    ${TOOL_GENASSYM} -- ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} \
	    ${GENASSYM_CPPFLAGS} > assym.h.tmp && \
	mv -f assym.h.tmp assym.h
${SRCS:C/\.[Ss]/.o/} ${SRCS:C/\.[Ss]/.d/}: assym.h
.endif
