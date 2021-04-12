#	$NetBSD: Makefile,v 1.1 2020/04/04 01:34:30 christos Exp $

NOLINKLIB=	# defined
NOLINT=		# defined
NOMAN=		# defined
NOPROFILE=	# defined
NOCLANGERROR=	# defined

.include <bsd.init.mk>

LIB=		gnuctf

BFD_MACHINE_ARCH?=	${MACHINE_ARCH:C/armv[4-7]/arm/}

.if exists(${.CURDIR}/arch/${BFD_MACHINE_ARCH}/defs.mk)
.include "${.CURDIR}/arch/${BFD_MACHINE_ARCH}/defs.mk"

.if defined(__MINIX)
# MINIX: make sure sources are fetched, even when tools not built earlier
GNUHOSTDIST=	${DIST}
.include "${.CURDIR}/../../../../../minix/Makefile.fetchgnu"
.endif # defined(__MINIX)

SHLIB_MAJOR=	0
SHLIB_MINOR=	0

GCPPFLAGS=	${G_archdefs} ${G_DEFS} ${G_INCLUDES} ${G_TDEFAULTS}
CPPFLAGS+=	-I${.CURDIR}/arch/${BFD_MACHINE_ARCH} \
		-I${.CURDIR}/../libbfd/arch/${BFD_MACHINE_ARCH} \
		-I${DIST}/include -I. -I${DIST}/libctf \
		-I${DIST}/bfd ${GCPPFLAGS:M-D*} \
		${GCPPFLAGS:M-I*:N-I.*}

SRCS=		${G_libctf_la_SOURCES}

.PATH: ${DIST}/libctf

.include <bsd.lib.mk>

.if defined(__MINIX) #&& ${USETOOLS} != "yes"
# Trigger the fetch phase, even when not building tools
${SRCS} realdepend realall realinstall: ${fetch_done}
.endif # defined(__MINIX)

.else
.include <bsd.prog.mk> # do nothing
.endif
