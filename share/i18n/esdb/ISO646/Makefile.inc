# $NetBSD: Makefile.inc,v 1.4 2008/10/25 22:27:36 apb Exp $

.PATH: ${.CURDIR}/ISO646

CODE:=		ISO646
.include "${.CURDIR}/Makefile.part"

.for i in ${ISO646_PART}
ISO646-${i:S/:/@/}.src: ISO646.src
	${_MKTARGET_CREATE}
	${TOOL_SED} 's/ISO646-x/ISO646-$i/' $> > $@
CLEANFILES:= ${CLEANFILES} ISO646-${i:S/:/@/}.src
.endfor
