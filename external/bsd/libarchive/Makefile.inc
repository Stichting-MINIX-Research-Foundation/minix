# $NetBSD: Makefile.inc,v 1.3 2017/04/20 20:34:24 joerg Exp $

.include <bsd.own.mk>

USE_FORT?=	yes # complex string handling

LIBARCHIVEDIR=	${NETBSDSRCDIR}/external/bsd/libarchive/dist

CPPFLAGS+=	-I${NETBSDSRCDIR}/external/bsd/libarchive/include
CPPFLAGS+=	-DPLATFORM_CONFIG_H=\"config_netbsd.h\"

LIBARCHIVE_FE_DIR!=	cd ${.PARSEDIR}/lib/libarchive_fe && ${PRINTOBJDIR}
LIBARCHIVE_FE=		${LIBARCHIVE_FE_DIR}/libarchive_fe.a

# GCC applies the check even for same functions like strftime,
# completely defeating the purpose.
CWARNFLAGS.gcc+=	-Wno-format-nonliteral

WARNS?=	4
