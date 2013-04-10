#	$NetBSD: Makefile.inc,v 1.10 2011/05/26 12:56:29 joerg Exp $

.if !defined(__MAKEFILE_INC_INCLUDED__)
__MAKEFILE_INC_INCLUDED__=1

.include <bsd.own.mk>

DIST=		${NETBSDSRCDIR}/gnu/dist
IDIST=		${DIST}/texinfo

CPPFLAGS+=	-I${.CURDIR}/../common -I${IDIST}/lib \
		-DLOCALEDIR=\"/usr/share/locale\" -DHAVE_CONFIG_H

CWARNFLAGS.clang+=	-Wno-format-security

.if !defined(MAKEDOC)
DPADD+=		${LIBCOMMONDIR}/libcommon.a
LDADD+=		-L${LIBCOMMONDIR} -lcommon

.if !defined(__MINIX)
DPADD+=		${LIBINTL}
LDADD+=		-lintl
.endif # !defined(__MINIX)

LIBCOMMONDIR!=	cd ${.CURDIR}/../common && ${PRINTOBJDIR}
.endif

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif

.endif # __MAKEFILE_INC_INCLUDED__
