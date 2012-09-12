# $NetBSD: bsd.clang-analyze.mk,v 1.3 2012/04/04 10:37:18 joerg Exp $

.ifndef CLANG_ANALYZE_SRCS

CLANG_ANALYZE_FLAGS+=	--analyze

CLANG_ANALYZE_CHECKERS+=	core deadcode security unix

.for checker in ${CLANG_ANALYZE_CHECKERS}
CLANG_ANALYZE_FLAGS+=	-Xanalyzer -analyzer-checker=${checker}
.endfor

.SUFFIXES: .c .cc .cpp .cxx .C .clang-analyzer

CLANG_ANALYZE_CFLAGS=		${CFLAGS:N-Wa,--fatal-warnings}
CLANG_ANALYZE_CXXFLAGS=	${CXXFLAGS:N-Wa,--fatal-warnings}

.c.clang-analyzer:
	${TOOL_CC.clang} ${CLANG_ANALYZE_FLAGS} \
	    ${CLANG_ANALYZE_CFLAGS} ${CPPFLAGS} \
	    ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} \
	    ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}
.cc.clang-analyzer .cpp.clang-analyzer .cxx.clang-analyzer .C.clang-analyzer:
	${TOOL_CXX.clang} ${CLANG_ANALYZE_FLAGS} \
	    ${CLANG_ANALYZE_CXXFLAGS} ${CPPFLAGS} \
	    ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} \
	    ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}

CLANG_ANALYZE_SRCS= \
	${SRCS:M*.[cC]} ${SRCS:M*.cc} \
	${SRCS:M*.cpp} ${SRCS:M*.cxx} \
	${DPSRCS:M*.[cC]} ${DPSRCS:M*.cc} \
	${DPSRCS:M*.cpp} ${DPSRCS:M*.cxx}
.if !empty(CLANG_ANALYZE_SRCS)
CLANG_ANALYZE_OUTPUT=	${CLANG_ANALYZE_SRCS:R:S,$,.clang-analyzer,}
.endif

analyze: ${CLANG_ANALYZE_OUTPUT}

.endif
