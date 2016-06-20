# $NetBSD: Makefile.inc,v 1.2 2008/10/25 22:27:36 apb Exp $

.PATH: ${.CURDIR}/KOI

CODE:=		KOI
SEP_KOI:=
.include "${.CURDIR}/Makefile.part"

.for i in ${KOI_PART}
.if !exists(${.CURDIR}/KOI/KOI${i:S/:/@/}.src)
KOI${i:S/:/@/}.src: KOI.src
	${_MKTARGET_CREATE}
	${TOOL_SED} 's/KOIx/KOI$i/' $> > $@
CLEANFILES+= KOI${i:S/:/@/}.src
.endif
.endfor
