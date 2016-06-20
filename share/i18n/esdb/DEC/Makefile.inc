# $NetBSD: Makefile.inc,v 1.2 2008/10/25 22:27:36 apb Exp $

.PATH: ${.CURDIR}/DEC

CODE:=		DEC
SEP_DEC:=
.include "${.CURDIR}/Makefile.part"

.for i in ${DEC_PART}
.if !exists(${.CURDIR}/DEC/DEC${i:S/:/@/}.src)
DEC${i:S/:/@/}.src: DEC.src
	${_MKTARGET_CREATE}
	${TOOL_SED} 's/DECx/DEC$i/' $> > $@
CLEANFILES+=	DEC${i:S/:/@/}.src
.endif
.endfor
