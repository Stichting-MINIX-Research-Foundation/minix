# $NetBSD: Makefile.inc,v 1.4 2008/10/25 22:27:35 apb Exp $

.PATH: ${.CURDIR}/CP

CODE:=		CP
SEP_CP:=
.include "${.CURDIR}/Makefile.part"

.for i in ${CP_PART}
.if !exists(${.CURDIR}/CP/CP${i:S/:/@/}.src)
CP${i:S/:/@/}.src: CP.src
	${_MKTARGET_CREATE}
	${TOOL_SED} 's/CPx/CP$i/' $> > $@
CLEANFILES+= CP${i:S/:/@/}.src
.endif
.endfor
