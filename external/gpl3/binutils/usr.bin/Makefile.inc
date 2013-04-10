#	$NetBSD: Makefile.inc,v 1.3 2011/05/30 14:41:26 joerg Exp $

BINDIR?=	/usr/bin

.if !defined(__MAKEFILE_INC_INCLUDED__)
__MAKEFILE_INC_INCLUDED__=1

.include <bsd.own.mk>

GNUHOSTDIST=	${DIST}
GNUCPPFLAGS=	${G_DEFS} ${G_INCLUDES}
CPPFLAGS+=	${GNUCPPFLAGS:M-D*:N-DLOCALEDIR*} ${GNUCPPFLAGS:M-I*:N-I.*} \
		-I${TOP}/${BFDSUBDIR}/libbfd/arch/${MACHINE_ARCH} \
		-I${DIST}/include -I${DIST}/bfd -I${DIST}/binutils \
		-DLOCALEDIR=\"${LOCALEDIR}\"
LDADD+=-lz
DPADD+=${LIBZ}

.if defined(__MINIX)
LDADD+=         -lm
DPADD+=         ${LIBM}
.endif

NOGCCERROR=	# defined

.endif # __MAKEFILE_INC_INCLUDED__
