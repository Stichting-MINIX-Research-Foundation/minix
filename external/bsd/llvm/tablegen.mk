#	$NetBSD: tablegen.mk,v 1.4 2011/10/11 13:53:57 joerg Exp $

.include <bsd.own.mk>

.for t in ${TABLEGEN_SRC}
.for f in ${TABLEGEN_OUTPUT} ${TABLEGEN_OUTPUT.${t}}
${f:C,\|.*$,,}: ${t} ${TOOL_LLVM_TBLGEN}
	[ -z "${f:C,\|.*$,,}" ] || mkdir -p ${f:C,\|.*$,,:H}
	${TOOL_LLVM_TBLGEN} -I${LLVM_SRCDIR}/include ${TABLEGEN_INCLUDES} \
	    ${TABLEGEN_INCLUDES.${t}} ${f:C,^.*\|,,:C,\^, ,} \
	    ${.ALLSRC:M*/${t}} -d ${.TARGET}.d -o ${.TARGET}
DPSRCS+=	${f:C,\|.*$,,}
CLEANFILES+=	${f:C,\|.*$,,} ${f:C,\|.*$,,:C,$,.d,}

.sinclude "${f:C,\|.*$,,:C,$,.d,}"
.endfor
.endfor

.for t in ${CLANG_TABLEGEN_SRC}
.for f in ${CLANG_TABLEGEN_OUTPUT} ${CLANG_TABLEGEN_OUTPUT.${t}}
${f:C,\|.*$,,}: ${t} ${TOOL_CLANG_TBLGEN}
	[ -z "${f:C,\|.*$,,}" ] || mkdir -p ${f:C,\|.*$,,:H}
	${TOOL_CLANG_TBLGEN} -I${LLVM_SRCDIR}/include \
	    ${CLANG_TABLEGEN_INCLUDES} ${CLANG_TABLEGEN_INCLUDES.${t}} \
	    ${f:C,^.*\|,,:C,\^, ,} \
	    ${.ALLSRC:M*/${t}} -d ${.TARGET}.d -o ${.TARGET}
DPSRCS+=	${f:C,\|.*$,,}
CLEANFILES+=	${f:C,\|.*$,,} ${f:C,\|.*$,,:C,$,.d,}

.sinclude "${f:C,\|.*$,,:C,$,.d,}"
.endfor
.endfor
