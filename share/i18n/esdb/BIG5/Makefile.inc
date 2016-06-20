# $NetBSD: Makefile.inc,v 1.3 2008/10/25 22:35:36 apb Exp $

.PATH: ${.CURDIR}/BIG5

CODE:=			Big5
PARTFILE_Big5:=		${.CURDIR}/BIG5/Big5.part
ALIASFILE_Big5:=	${.CURDIR}/BIG5/Big5.alias
SUBDIR_Big5:=		BIG5
.include "${.CURDIR}/Makefile.part"

.for i in ${Big5_PART:S/:/@/}
Big5_$i_variable!= ${TOOL_SED} \
	'/^\#/d;/^ *$$/d;/^${i:S/@/:/}[ 	]/!d;s/^[^ 	]*[ 	]*//' \
	${.CURDIR}/BIG5/Big5.variable
.endfor

.for i in ${Big5_PART}
.if !exists(${.CURDIR}/BIG5/Big5-${i:S/:/@/}.src)
# XXX: FIXME
Big5-${i:S/:/@/}.src: Big5.src Big5.variable
	${_MKTARGET_CREATE}
	${TOOL_SED} -e 's/encoding/Big5-$i/' \
		-e 's/variable/${Big5_$i_variable}/' \
		${.CURDIR}/BIG5/Big5.src > $@
CLEANFILES+= Big5-${i:S/:/@/}.src
.endif
.endfor
