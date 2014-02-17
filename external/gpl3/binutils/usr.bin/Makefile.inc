#	$NetBSD: Makefile.inc,v 1.6 2013/08/06 05:35:57 matt Exp $

BINDIR?=	/usr/bin

.if !defined(__MAKEFILE_INC_INCLUDED__)
__MAKEFILE_INC_INCLUDED__=1

.include <bsd.own.mk>

BINUTILS_MACHINE_ARCH=	${MACHINE_ARCH:C/armv[4-7]/arm/}

GNUHOSTDIST=	${DIST}
GNUCPPFLAGS=	${G_DEFS} ${G_INCLUDES}
CPPFLAGS+=	${GNUCPPFLAGS:M-D*:N-DLOCALEDIR*} ${GNUCPPFLAGS:M-I*:N-I.*} \
		-I${TOP}/${BFDSUBDIR}/libbfd/arch/${BINUTILS_MACHINE_ARCH} \
		-I${DIST}/include -I${DIST}/bfd -I${DIST}/binutils \
		-DLOCALEDIR=\"${LOCALEDIR}\"
NOGCCERROR=	# defined

.endif # __MAKEFILE_INC_INCLUDED__
