#	$NetBSD: Makefile,v 1.11 2014/10/03 12:41:41 apb Exp $

USE_SHLIBDIR=	yes
REQUIRETOOLS=	yes
NOLINT=		# defined

LIB=		gcc_s
.if !defined(__MINIX)
NOSTATICLIB=	# defined
.endif # !defined(__MINIX)
NOPROFILE=	# defined
NOPICINSTALL=	# defined

.include "../../../Makefile.gcc_path"
.include <bsd.own.mk>

.if ${MKGCC} != "no"

.cc: # disable .cc->NULL transform

SHLIB_MAJOR=	1
SHLIB_MINOR=	0

LDFLAGS+=	-nodefaultlibs
LDFLAGS+=	-Wl,--version-script=${.OBJDIR}/libgcc.map

# XXX handle this better?
# XXX handle this better?  GCC 4.8 moved them.
.if ${MACHINE} == "emips"
G_LIB2ADD_HACK+=	${GNUHOSTDIST}/gcc/config/floatunsidf.c \
			${GNUHOSTDIST}/gcc/config/floatunsisf.c
.endif

SRCS+=		${LIB2FUNCS} ${LIB2DIVMOD} ${LIB2_EH} ${LIB2_EHASM} \
		${G_LIB2ADD:T:S/.asm/.S/} ${G_LIB2ADD_HACK:T:S/.asm/.S/} \
		${G_LIB2ADD_ST:T} ${LIB1ASMFUNCS}

COPTS.unwind-dw2.c = -Wno-stack-protector

DPADD+=		libgcc.map
CLEANFILES+=	libgcc.map

.include <bsd.lib.mk>

.if ${MKPICLIB} != "no"
LIBGCC_S_OBJS=	${SOBJS}
.else
LIBGCC_S_OBJS=	${OBJS}
.endif

LIBGCC_STD_VER_IN=	${GNUHOSTDIST}/libgcc/libgcc-std.ver.in
CLEANFILES+=		libgcc-std.ver

libgcc-std.ver:	${LIBGCC_STD_VER_IN}
	${_MKTARGET_CREATE}
	${TOOL_SED}  -e 's/__PFX__/__/g' -e 's/__FIXPTPFX__/__/g' < ${LIBGCC_STD_VER_IN}  > ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}

libgcc.map:	${LIBGCC_S_OBJS} ${G_SHLIB_MKMAP} ${G_SHLIB_MAPFILES}
	{ \
		${NM} ${G_SHLIB_NM_FLAGS} ${LIBGCC_S_OBJS}; \
		echo %%; \
		for f in ${G_SHLIB_MAPFILES}; do \
			if [ -f ${.OBJDIR}/$${f} ]; then \
				cat ${.OBJDIR}/$${f}; \
			else \
				cat $${f}; \
			fi; \
		done | \
			${TOOL_SED} -e "/^[ 	]*#/d" \
			    -e 's/^%\(if\|else\|elif\|endif\|define\)/#\1/' | \
	  		${CC} ${G_LIBGCC2_CFLAGS} ${G_INCLUDES} -E -xassembler-with-cpp -; \
	} | ${TOOL_AWK} -f ${G_SHLIB_MKMAP} > ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET} 

.else
.include <bsd.prog.mk> # do nothing
.endif
