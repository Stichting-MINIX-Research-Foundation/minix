# $NetBSD: Makefile.inc,v 1.2 2008/10/25 22:27:35 apb Exp $

.PATH: ${.CURDIR}/AST

CODE:=			ARMSCII
PARTFILE_ARMSCII:=	${.CURDIR}/AST/ARMSCII.part
ALIASFILE_ARMSCII:=	${.CURDIR}/AST/ARMSCII.alias
SUBDIR_ARMSCII:=	AST
.include "${.CURDIR}/Makefile.part"

.for i in ${ARMSCII_PART}
.if !exists(${.CURDIR}/AST/ARMSCII-${i:S/:/@/}.src)
ARMSCII-${i:S/:/@/}.src: ARMSCII.src
	${_MKTARGET_CREATE}
	${TOOL_SED} 's/ARMSCII-x/ARMSCII-$i/' $> > $@
CLEANFILES+= ARMSCII-${i:S/:/@/}.src
.endif
.endfor
