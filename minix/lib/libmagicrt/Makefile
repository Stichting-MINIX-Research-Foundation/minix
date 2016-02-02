# Makefile for libmagicrt
#
# The magic runtime library is the runtime state transfer component for live
# update.  It is not a regular library.  First of all, it is only built when
# MKMAGIC is enabled, which also implies that we are building bitcode.
# Second, the produced file is a single bitcode object containing basically the
# concatenation of the individual bitcode objects produced from the source
# files.  The final bitcode object is used to link against system services
# during the rest of the compilation process.  It is installed only because
# this makes it easier to refer to it during the rest of the compilation
# process.  However, both the library's generation and its installation cannot
# be done with regular bsd.lib.mk rules, which is why this Makefile is rather
# nonstandard.

.include <bsd.own.mk>

LIBNAME=	libmagicrt.bcc

SRCS=		magic.c magic_analysis.c magic_asr.c magic_ds.c
SRCS+=		magic_eval.c magic_eval_lib.c magic_mem.c magic_range.c
SRCS+=		magic_selement.c magic_sentry.c magic_splay_tree.c
SRCS+=		magic_st.c magic_util.c

OBJS=		${SRCS:.c=.bc}

CPPFLAGS+=	-D__MINIX -D_MINIX_SYSTEM -D_SYSTEM
CPPFLAGS+=	-I${.CURDIR}/include
CPPFLAGS+=	-I${NETBSDSRCDIR}/minix/llvm/include # for magic_common.h
CPPFLAGS.magic_ds.c+=	-I${NETBSDSRCDIR}/minix/servers # for ds/store.h

# XXX: there is essential code in assert() statements, so force asserts on..
CPPFLAGS+=	-UNDEBUG

# All functions and data must be assigned to nonstandard sections.  However,
# the magic_st module has different rules from the rest.
SECTIONIFY=	-sectionify-no-override \
	-sectionify-data-section-map=^_____magic_instr_.*/magic_instr_data,.*/magic_data \
	-sectionify-function-section-map=.*/magic_functions
SECTIONIFY.magic_st.c=	\
	-sectionify-data-section-map=.*/magic_data_st \
	-sectionify-function-section-map=.*/magic_functions_st

# HACK: keep the "temporary" .bc.o files generated as part of running the
# sectionify pass, so that we can trick clang into taking bitcode objects
# during the linking phase, because its driver refuses to take objects with
# the .bc suffix even when using the gold plugin.  This works only because
# we are using sectionify on all objects here, and because bsd.lib.mk uses
# temporary names convenient for us.  See also the comment in bsd.lib.mk.
SECTIONIFYMV=cp

LDFLAGS+=-nostdlib -rdynamic -shared -Wl,--plugin \
	-Wl,${NETBSDSRCDIR}/minix/llvm/bin/LLVMgold.so \
	-Wl,-plugin-opt=-disable-fp-elim -Wl,-plugin-opt=emit-llvm

realall: ${LIBNAME}

${LIBNAME}: ${OBJS}
	${_MKTARGET_LINK}
	${LINK.c} -o ${.TARGET} ${OBJS:.bc=.bc.o}

# The following block is a modified copy of similar blocks in bsd.lib.mk.
_LIB=${DESTDIR}/${LIBDIR}/${LIBNAME}
libinstall:: ${_LIB}
.PRECIOUS: ${_LIB}
.if ${MKUPDATE} == "no"
.if !defined(BUILD) && !make(all) && !make(${LIBNAME})
${_LIB}! .MADE
.endif
${_LIB}! ${LIBNAME} __archiveinstall
.else
.if !defined(BUILD) && !make(all) && !make(${LIBNAME})
${_LIB}: .MADE
.endif
${_LIB}: ${LIBNAME} __archiveinstall
.endif

CLEANFILES+=	${LIBNAME} ${OBJS:.bc=.bc.o}

.include <bsd.lib.mk>
