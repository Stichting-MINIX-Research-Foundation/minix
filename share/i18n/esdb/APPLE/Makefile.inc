# $NetBSD: Makefile.inc,v 1.2 2008/10/25 22:27:35 apb Exp $

.PATH: ${.CURDIR}/APPLE

CODE:=		MAC
PARTFILE_MAC:=	${.CURDIR}/APPLE/MAC.part
ALIASFILE_MAC:=	${.CURDIR}/APPLE/MAC.alias
SEP_MAC:=
SUBDIR_MAC:=	APPLE
.include "${.CURDIR}/Makefile.part"

.for i in ${MAC_PART}
.if !exists(${.CURDIR}/MAC/MAC${i:S/:/@/}.src)
MAC${i:S/:/@/}.src: MAC.src
	${_MKTARGET_CREATE}
	${TOOL_SED} 's/changeme/$i/' $> > $@
CLEANFILES+= MAC${i:S/:/@/}.src
.endif
.endfor
