#	$NetBSD: Makefile,v 1.10 2012/08/06 02:34:28 matt Exp $

REQUIRETOOLS=	yes
NOLINT=		# defined
NOPIC=		# defined
NOPROFILE=	# defined

LIB=		gcc

.include <bsd.own.mk>

.if ${MKGCC} != "no"

.cc: # disable .cc->NULL transform

# XXX handle this better?
.if ${MACHINE} == "emips"
G_LIB2ADD_HACK+=	${GNUHOSTDIST}/gcc/config/floatunsidf.c \
			${GNUHOSTDIST}/gcc/config/floatunsisf.c
.endif

SRCS+=		${LIB2FUNCS} ${LIB2FUNCS_ST} ${LIB2DIVMOD} \
		${G_LIB2ADD_HACK:T:S/.asm/.S/} ${G_LIB2ADD:T:S/.asm/.S/} \
		${G_LIB2ADD_ST} ${LIB1ASMFUNCS}
.if ${MKPIC} == "no"
.if ${MACHINE_ARCH} != "earm" && ${MACHINE_ARCH} != "earmeb"
SRCS+=		${LIB2_EH} ${LIB2_EHASM}
.endif
.else
CPPFLAGS+=	-DPIC
.endif

# These usually just have "foo.S", we need to link from the
# gcc/config/$PLATFORM/foo.asm to it.  So far, only rs6000 uses it,
# so we hack it for now.
#.for file in ${G_LIB2ADD_ST}
.   if ${MACHINE_ARCH} == "powerpc"  # XXX XXX why does the above work?*
.for file in crtsavfpr.S crtresfpr.S crtsavgpr.S crtresgpr.S crtresxfpr.S crtresxgpr.S
BUILDSYMLINKS+=	${GNUHOSTDIST}/gcc/config/rs6000/${file:S/.S$/.asm/} ${file}
.endfor
.   endif

# Force building libgcc.a.  It will be PIC from -DPIC above and also
# the -fpic/-FPIC present in ${G_LIBGCC2_CFLAGS}.
MKPIC:= no

COPTS.unwind-dw2.c = -Wno-stack-protector

.include <bsd.lib.mk>


CLEANFILES+=	${SOBJS:=.tmp1} ${SOBJS:=.tmp2}

.c.o:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CSHLIBFLAGS} ${.IMPSRC} -o ${.TARGET}.tmp1
	${NM} -pg ${.TARGET}.tmp1 | \
	    ${TOOL_AWK} 'NF == 3 { print "\t.hidden", $$3 }' | \
	    ${CC} ${COPTS} -Wl,-x -r -nostdinc -nostdlib ${CPUFLAGS} -o ${.TARGET}.tmp2 ${.TARGET}.tmp1 -xassembler -
.if defined(COPTS) && !empty(COPTS:M*-g*)
	mv ${.TARGET}.tmp2 ${.TARGET}
.else
	${LD} -x -r ${.TARGET}.tmp2 -o ${.TARGET}
	rm -f ${.TARGET}.tmp2
.endif
	rm -f ${.TARGET}.tmp1

.S.o .s.o:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${CAPICFLAGS} ${CFLAGS:M-[ID]*} ${.IMPSRC} -o ${.TARGET}.tmp1
	${NM} -pg ${.TARGET}.tmp1 | \
	    ${TOOL_AWK} 'NF == 3 { print "\t.hidden", $$3 }' | \
	    ${CC} -Wl,-x -r -nostdinc -nostdlib -o ${.TARGET}.tmp2 ${.TARGET}.tmp1 -xassembler -
.if defined(COPTS) && !empty(COPTS:M*-g*)
	mv ${.TARGET}.tmp2 ${.TARGET}
.else
	${LD} -x -r ${.TARGET}.tmp2 -o ${.TARGET}
	rm -f ${.TARGET}.tmp2
.endif
	rm -f ${.TARGET}.tmp1

.else
.include <bsd.prog.mk> # do nothing
.endif
