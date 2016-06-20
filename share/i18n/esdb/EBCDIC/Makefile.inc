# $NetBSD: Makefile.inc,v 1.4 2008/10/25 22:27:36 apb Exp $

.PATH: ${.CURDIR}/EBCDIC

CODE:=		EBCDIC
.include "${.CURDIR}/Makefile.part"

.for i in ${EBCDIC_PART}
EBCDIC-${i:S/:/@/}.src: EBCDIC.src
	${_MKTARGET_CREATE}
	${TOOL_SED} 's/EBCDIC-x/EBCDIC-$i/' $> > $@
CLEANFILES:= ${CLEANFILES} EBCDIC-${i:S/:/@/}.src
.endfor
