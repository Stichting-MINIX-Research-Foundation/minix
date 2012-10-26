#	$NetBSD: Makefile.inc,v 1.15 2012/08/29 03:12:08 msaitoh Exp $

.if ${MKGCC} != "no"
.if exists(${.CURDIR}/../arch/${MACHINE_ARCH}.mk)
.include "${.CURDIR}/../arch/${MACHINE_ARCH}.mk"
.endif
.endif

UNSUPPORTED_COMPILER.clang=	# defined

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist
GNUHOSTDIST=	${DIST}
GCCARCH=	${NETBSDSRCDIR}/external/gpl3/gcc/usr.bin/gcc/arch/${MACHINE_ARCH}
GCCARCHLIBGCC=	${NETBSDSRCDIR}/external/gpl3/gcc/lib/libgcc/arch/${MACHINE_ARCH}
GCCARCHXX=	${NETBSDSRCDIR}/external/gpl3/gcc/lib/libstdc++-v3/arch/${MACHINE_ARCH}

GCPPFLAGS=	${G_LIBGCC2_CFLAGS} ${G_USE_COLLECT2} ${G_INCLUDES}
CPPFLAGS+=	-I${.CURDIR} -I${GCCARCHLIBGCC}
CPPFLAGS+=	-I${GCCARCH} ${GCPPFLAGS:M-D*} ${GCPPFLAGS:M-I*:N-I.*}
CPPFLAGS+=	${GCPPFLAGS:M-m*} ${GCPPFLAGS:M-f*}
CPPFLAGS+=	${G_EXTRA_HEADERS:M/*:H:u:S/^/-I/g}
CPPFLAGS+=	-I${DIST} -I${DIST}/gcc/cp -I${DIST}/gcc/config -I${GCCARCHXX} -I.

.if ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "sh3el" || \
    ${MACHINE_ARCH} == "sh3eb" || \
    ${MACHINE_ARCH} == "m68000"
_TRADITIONAL_CPP=
COMPILE.S=	${CC} ${AFLAGS} ${CPPFLAGS} -c
.endif

#
# Old style libgcc build procedure.
#

# This is copied from gcc/mklibgcc.in:$lib2funcs
LIB2FUNCS_ALL= \
	_muldi3 _negdi2 _lshrdi3 _ashldi3 _ashrdi3 _cmpdi2 _ucmpdi2    \
	_clear_cache _enable_execute_stack _trampoline __main _absvsi2 \
	_absvdi2 _addvsi3 _addvdi3 _subvsi3 _subvdi3 _mulvsi3 _mulvdi3 \
	_negvsi2 _negvdi2 _ctors _ffssi2 _ffsdi2 _clz _clzsi2 _clzdi2  \
	_ctzsi2 _ctzdi2 _popcount_tab _popcountsi2 _popcountdi2        \
	_paritysi2 _paritydi2 _powisf2 _powidf2 _powixf2 _powitf2      \
	_mulsc3 _muldc3 _mulxc3 _multc3 _divsc3 _divdc3 _divxc3        \
	_divtc3 _bswapsi2 _bswapdi2

# non swfloat versions 
.for _p in _fix _fixuns
. for _m in sf df xf tf
LIB2FUNCS_ALL+=	${_p}${_m}di
. endfor
.endfor

.for _p in _float _floatun
. for _m in sf df xf tf
LIB2FUNCS_ALL+=	${_p}di${_m}
. endfor
.endfor

.for _m in sf df xf
LIB2FUNCS_ALL+=	_fixuns${_m}si
.endfor

LIB2FUNCS_SHORT:=${LIB2FUNCS_ALL}
LIB2_DIVMOD_FUNCS:=${G_LIB2_DIVMOD_FUNCS}
.for f in ${G_LIB1ASMFUNCS}; do
LIB2FUNCS_SHORT:=${LIB2FUNCS_SHORT:N${f}}
LIB2_DIVMOD_FUNCS:=${LIB2_DIVMOD_FUNCS:N${f}}
.endfor

LIB2FUNCS=	${LIB2FUNCS_SHORT:=.c}
LIB2FUNCS_ST=	${G_LIB2FUNCS_ST:=.c}
LIB2DIVMOD=	${LIB2_DIVMOD_FUNCS:=.c}
LIB2_EH=	${G_LIB2ADDEH:M*.c:T}
LIB2_EHASM=	${G_LIB2ADDEH:M*.S:T}
.if !empty(G_LIB1ASMFUNCS)
LIB1ASMFUNCS=	${G_LIB1ASMFUNCS:=.S}
.endif

DPSRCS+=	${.CURDIR}/../arch/${MACHINE_ARCH}.mk
CLEANFILES+=	${LIB2FUNCS} ${LIB2FUNCS_ST} ${LIB2DIVMOD} cs-tconfig.h
.if !empty(G_LIB1ASMFUNCS)
CLEANFILES+=	${LIB1ASMFUNCS}
.endif

.for file in ${G_LIB2ADD:M${GNUHOSTDIST}*:N*.c}
BUILDSYMLINKS+=	${file} ${file:T:S/.asm$/.S/}
.endfor

# XXX
.if (${MACHINE_ARCH} == "m68000" || ${MACHINE_ARCH} == "m68k")
CPICFLAGS:=
BUILDSYMLINKS+=	${DIST}/gcc/config/m68k/fpgnulib.c fpgnulib.c
BUILDSYMLINKS+=	${DIST}/gcc/config/m68k/fpgnulib.c xfgnulib.c
CPPFLAGS.xfgnulib.c+=-DEXTFLOAT=1
.endif

.if ${MACHINE_ARCH} == "powerpc64"
COPTS+=	-mlong-double-128
.endif

${LIB2FUNCS} ${LIB2FUNCS_ST}: ${.CURDIR}/Makefile
	${_MKTARGET_CREATE}
	printf '#define L${.PREFIX}\n#include <libgcc2.c>\n' >${.TARGET}

${LIB2DIVMOD}: ${.CURDIR}/Makefile
	${_MKTARGET_CREATE}
	printf '#define L${.PREFIX}\n#include <libgcc2.c>\n' >${.TARGET}

.if !empty(G_LIB1ASMFUNCS)
${LIB1ASMFUNCS}: ${.CURDIR}/Makefile
	${_MKTARGET_CREATE}
	printf '#define L${.PREFIX}\n#include <${G_LIB1ASMSRC}>\n' >${.TARGET}
.endif

${LIB2_EH:.c=.o}:
	${_MKTARGET_COMPILE}
	${COMPILE.c} -fexceptions ${COPTS.${.IMPSRC:T}} -o ${.TARGET} ${.IMPSRC}

${LIB2_EH:.c=.pico}:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${CPICFLAGS} -fexceptions ${COPTS.${.IMPSRC:T}} -o ${.TARGET} ${.IMPSRC}

${G_LIB2_DIVMOD_FUNCS:=.o}:
	${_MKTARGET_COMPILE}
	${COMPILE.c} -fexceptions -fnon-call-exceptions ${COPTS.${.IMPSRC:T}} -o ${.TARGET} ${.IMPSRC}

${G_LIB2_DIVMOD_FUNCS:=.pico}:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${CPICFLAGS} -fexceptions -fnon-call-exceptions ${COPTS.${.IMPSRC:T}} -o ${.TARGET} ${.IMPSRC}

dp-bit.c: ${.CURDIR}/Makefile
	${_MKTARGET_CREATE}
	printf '#define FLOAT\n#include <fp-bit.c>\n' >${.TARGET}

#
# Common rules.
#

.PATH: ${DIST}/gcc ${DIST}/gcc/cp ${DIST}/gcc/cp/inc ${DIST}/gcc/config \
	${G_CONFIGDIR} ${G_LIB2ADD:H:O:u}

.include "../Makefile.tconfigh"
DPSRCS+=	tconfig.h

.if ${MKGCC} != "no"
# XXX figure out why Makefile.hacks doesn't work
BUILDSYMLINKS+=	${G_UNWIND_H} unwind.h
DPSRCS+=	unwind.h
.endif

# these aren't necessary but are #include'd
FAKEHEADERS=options.h insn-flags.h insn-constants.h sysroot-suffix.h
${FAKEHEADERS}:
	${_MKTARGET_CREATE}
	touch ${.TARGET}
DPSRCS+=	${FAKEHEADERS}
CLEANFILES+=	${FAKEHEADERS}

# XXX arm hack
.if ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "armeb"
${SRCS}: insn-modes.h
# XXX XXX
${__DPSRCS.d}: insn-modes.h
insn-modes.h:
	${_MKTARGET_CREATE}
	echo "enum machine_mode { X };" > ${.TARGET}
DPSRCS+=	insn-modes.h
CLEANFILES+=	insn-modes.h
.endif

# XXX
.if defined(HAVE_GCC) && ${HAVE_GCC} == 45 && ${MACHINE_ARCH} == "m68k"
COPTS._fixunsxfdi.c+=	-O0
.endif
