#	$NetBSD: Makefile,v 1.13 2014/06/01 19:51:01 mrg Exp $

REQUIRETOOLS=	yes
NOLINT=		# defined

UNSUPPORTED_COMPILER.clang=	# defined

.include <bsd.init.mk>

LIB=		objc
GCC_MACHINE_ARCH?=	${MACHINE_ARCH:S/earmv5/earm/}

.if ${MKGCC} != "no"

SHLIB_MAJOR=	4
SHLIB_MINOR=	0

# XXX just while all platforms defs.mk are updated..
.if exists(${.CURDIR}/arch/${GCC_MACHINE_ARCH}/defs.mk)
# Machine-dependent definitions (include file names).
.include "${.CURDIR}/arch/${GCC_MACHINE_ARCH}/defs.mk"
EXTRA_FAKEHEADERS=
.else
# Machine-independent definitions (include file names).
.include "${.CURDIR}/defs.mk"
EXTRA_FAKEHEADERS=
.endif

LIBGCCOBJ!=    cd ${.CURDIR}/../libgcc/libgcc && ${PRINTOBJDIR}

DIST=		${GCCDIST}
GNUHOSTDIST=	${DIST}
GCCARCH=	${GCC_SUBDIR}/usr.bin/gcc/arch/${GCC_MACHINE_ARCH}
LIBOBJCARCH=	${GCC_SUBDIR}/lib/libobjc/arch/${GCC_MACHINE_ARCH}
GCCLIBXX=	${GCC_SUBDIR}/lib/libstdc++-v3/arch/${GCC_MACHINE_ARCH}

SRCS=		${G_OBJC_SOURCE_FILES} ${G_C_SOURCE_FILES}

GCPPFLAGS=	${G_ALL_CFLAGS} ${G_INCLUDES}
CPPFLAGS+=	-I. -I${.CURDIR}/arch/${GCC_MACHINE_ARCH}
CPPFLAGS+=	-I${GCCARCH} ${GCPPFLAGS:M-D*} ${GCPPFLAGS:M-I*:N-I.*}
CPPFLAGS+=	-I${GCCLIBXX}
CPPFLAGS+=	-I${DIST}/libobjc/objc-private

BUILDSYMLINKS=	${.CURDIR}/../libstdc++-v3/arch/${GCC_MACHINE_ARCH} bits
DPSRCS+=	bits

.for f in Protocol.m Object.m NXConstStr.m linking.m
OBJCOPTS.${f}=	-fgnu-runtime
.endfor

.for f in thr.c thr-objc.c
COPTS.${f}=	-Wno-missing-noreturn
.endfor

COPTS+=		-fno-strict-aliasing -fexceptions
OBJCOPTS+=	-fno-strict-aliasing -fexceptions

OBJCOPTS.Object.m = -Wno-stack-protector
COPTS.sendmsg.c = -Wno-stack-protector
COPTS.objc-act.c = -Wno-stack-protector

INCS=		${G_OBJC_H}
INCSDIR=	/usr/include/objc

DPSRCS+=	runtime-info.h
CLEANFILES+=	rtscratch rtscratch.s runtime-info.h
runtime-info.h:
	${_MKTARGET_CREATE}
	touch rtscratch
	`${OBJC} --print-prog-name=cc1obj` -print-objc-runtime-info rtscratch >$@

${OBJS} ${POBJS} ${SOBJS}: runtime-info.h

${SRCS}: tconfig.h options.h

.if ${HAVE_LIBGCC_EH} == "no"
G_CONFIGLINKS:= ${G_CONFIGLINKS:S,libgcc/config/arm/unwind-arm.h,libgcc/unwind-generic.h,}
.endif

.if ${MKGCC} != "no"
.if defined(G_CONFIGLINKS) && !empty(G_CONFIGLINKS)
BUILDSYMLINKS+=	${G_CONFIGLINKS}
.for _src _dst in ${G_CONFIGLINKS}
DPSRCS+=	${_dst}
.endfor
.endif
.endif

.include "../Makefile.tconfigh"
.include "../Makefile.hacks"

# XXX just while all platforms defs.mk are updated..
.if exists(${.CURDIR}/arch/${GCC_MACHINE_ARCH}/defs.mk)
optionlist: ${G_ALL_OPT_FILES} ${LIBOBJCARCH}/defs.mk Makefile ${DIST}/gcc/opt-gather.awk
	${TOOL_AWK} -f ${DIST}/gcc/opt-gather.awk ${G_ALL_OPT_FILES} \
		> ${.TARGET}

.if ${MACHINE_ARCH} == "vax" || ${MACHINE_ARCH} == "mipseb"
OPTION_FLAGS=-v header_name="config.h system.h coretypes.h options.h tm.h" \
	-f ${DIST}/gcc/opt-read.awk
OPTION_DEPS=${DIST}/gcc/opt-read.awk
.endif

options.h: optionlist ${DIST}/gcc/opt-functions.awk ${DIST}/gcc/opth-gen.awk \
	${OPTION_DEPS}
	${TOOL_AWK} -f ${DIST}/gcc/opt-functions.awk \
		-f ${DIST}/gcc/opth-gen.awk ${OPTION_FLAGS} \
		< optionlist > ${.TARGET}

CLEANFILES+=	options.h optionlist
.endif

.include <bsd.lib.mk>

.PATH: ${DIST}/libobjc ${DIST}/libobjc/objc ${DIST}/libobjc/objc-private

.else
.include <bsd.prog.mk>	# do nothing
.endif

COPTS.archive.c+=	-Wno-error
COPTS.sendmsg.c+=	-Wno-error
