#	$NetBSD: Makefile,v 1.6 2011/07/01 10:04:36 mrg Exp $

REQUIRETOOLS=	yes
NOLINT=		# defined

UNSUPPORTED_COMPILER.clang=	# defined

.include <bsd.own.mk>

LIB=		objc

.if ${MKGCC} != "no"

SHLIB_MAJOR=	3
SHLIB_MINOR=	0

# XXX just while all platforms defs.mk are updated..
.if exists(${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk)
# Machine-dependent definitions (include file names).
.include "${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk"
EXTRA_FAKEHEADERS=
.else
# Machine-independent definitions (include file names).
.include "${.CURDIR}/defs.mk"
EXTRA_FAKEHEADERS=
.endif

LIBGCCOBJ!=    cd ${.CURDIR}/../libgcc/libgcc && ${PRINTOBJDIR}

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist
GNUHOSTDIST=	${NETBSDSRCDIR}/external/gpl3/gcc/dist
GCCARCH=	${NETBSDSRCDIR}/external/gpl3/gcc/usr.bin/gcc/arch/${MACHINE_ARCH}
LIBOBJCARCH=	${NETBSDSRCDIR}/external/gpl3/gcc/lib/libobjc/arch/${MACHINE_ARCH}
GCCLIBXX=	${NETBSDSRCDIR}/external/gpl3/gcc/lib/libstdc++-v3/arch/${MACHINE_ARCH}

SRCS=		${G_OBJS:N[A-Z]*:Nlinking.lo:.lo=.c} ${G_OBJS:M[A-Z]*:.lo=.m} linking.m

GCPPFLAGS=	${G_ALL_CFLAGS} ${G_INCLUDES}
CPPFLAGS+=	-I. -I${.CURDIR}/arch/${MACHINE_ARCH}
CPPFLAGS+=	-I${GCCARCH} ${GCPPFLAGS:M-D*} ${GCPPFLAGS:M-I*:N-I.*}
CPPFLAGS+=	-I${GCCLIBXX}

BUILDSYMLINKS=	${.CURDIR}/../libstdc++-v3/arch/${MACHINE_ARCH} bits
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

INCS=		${G_OBJC_H}
INCSDIR=	/usr/include/objc

DPSRCS+=	runtime-info.h
CLEANFILES+=	rtscratch rtscratch.s runtime-info.h
runtime-info.h:
	${_MKTARGET_CREATE}
	touch rtscratch
	`${OBJC} --print-prog-name=cc1obj` -print-objc-runtime-info rtscratch >$@

.PATH: ${DIST}/libobjc ${DIST}/libobjc/objc

${OBJS} ${POBJS} ${SOBJS}: runtime-info.h

${SRCS}: tconfig.h unwind.h options.h

BUILDSYMLINKS+=	${G_UNWIND_H} unwind.h
DPSRCS+=	unwind.h

.include "../Makefile.tconfigh"
.include "../Makefile.hacks"

# XXX just while all platforms defs.mk are updated..
.if exists(${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk)
optionlist: ${G_ALL_OPT_FILES} ${LIBOBJCARCH}/defs.mk Makefile ${DIST}/gcc/opt-gather.awk
	${TOOL_AWK} -f ${DIST}/gcc/opt-gather.awk ${G_ALL_OPT_FILES} \
		> ${.TARGET}

options.h: optionlist ${DIST}/gcc/opt-functions.awk ${DIST}/gcc/opth-gen.awk
	${TOOL_AWK} -f ${DIST}/gcc/opt-functions.awk \
		-f ${DIST}/gcc/opth-gen.awk \
		< optionlist > ${.TARGET}

CLEANFILES+=	options.h optionlist
.endif

.include <bsd.lib.mk>

.else
.include <bsd.prog.mk>	# do nothing
.endif

COPTS.archive.c+=	-Wno-error
COPTS.sendmsg.c+=	-Wno-error
