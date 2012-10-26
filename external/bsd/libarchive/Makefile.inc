# $NetBSD: Makefile.inc,v 1.2 2010/02/20 02:55:53 joerg Exp $

.include <bsd.own.mk>

USE_FORT?=	yes # complex string handling

LIBARCHIVEDIR=	${NETBSDSRCDIR}/external/bsd/libarchive/dist

.if defined(__MINIX)
CPPFLAGS+=	-I${DESTDIR}/usr/include
.endif
CPPFLAGS+=	-I${NETBSDSRCDIR}/external/bsd/libarchive/include
CPPFLAGS+=	-DPLATFORM_CONFIG_H=\"config_netbsd.h\"

LIBARCHIVE_FE_DIR!=	cd ${.PARSEDIR}/lib/libarchive_fe && ${PRINTOBJDIR}
LIBARCHIVE_FE=		${LIBARCHIVE_FE_DIR}/libarchive_fe.a

WARNS?=	4
