# $NetBSD: Makefile.inc,v 1.4 2008/10/25 22:27:36 apb Exp $

.PATH: ${.CURDIR}/ISO-8859

CODE:=		ISO-8859
.include "${.CURDIR}/Makefile.part"

.for i in ${ISO-8859_PART}
ISO-8859-$i.src: ISO-8859.src
	${_MKTARGET_CREATE}
	${TOOL_SED} 's/ISO-8859-x/ISO-8859-$i/' $> > $@
CLEANFILES+= ISO-8859-$i.src
.endfor
