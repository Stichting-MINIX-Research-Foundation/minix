#	$NetBSD: Makefile,v 1.2 2009/09/08 07:08:02 skrll Exp $

.include <bsd.own.mk>

.if (defined(HOSTPROG) && \
     (${TOOLCHAIN_MISSING} == "no" || defined(EXTERNAL_TOOLCHAIN))) || \
    ${MKBINUTILS} != "no"
TOP=		${NETBSDSRCDIR}/external/gpl3/binutils
DIST=		${TOP}/dist
BFDSUBDIR=	lib

PROG=		mdsetimage
MAN=		mdsetimage.8

CPPFLAGS+=	-I${BFDDIR} -I${DIST}/bfd -I${DIST}/binutils -I${DIST}/include

.if !defined(HOSTPROG) && ${MKPICLIB} != "no"
DPADD+=		${BFDDIR}/libbfd_pic.a
.else
DPADD+=		${BFDDIR}/libbfd.a
.endif
LDADD+=		-L${BFDDIR} -lbfd

DPADD+=		${IBERTYDIR}/libiberty.a
LDADD+=		-L${IBERTYDIR} -liberty

.ifndef HOSTPROG
.include	"${TOP}/${BFDSUBDIR}/libbfd/arch/${MACHINE_ARCH}/defs.mk"

BFDDIR!=	cd ${TOP}/${BFDSUBDIR}/libbfd && ${PRINTOBJDIR}
IBERTYDIR!=	cd ${TOP}/${BFDSUBDIR}/libiberty && ${PRINTOBJDIR}

CPPFLAGS+=	-I${TOP}/${BFDSUBDIR}/libbfd/arch/${MACHINE_ARCH} ${G_INCLUDES}
LDADD+=		-lintl
DPADD+=		${LIBINTL}
.endif # HOSTPROG
.endif # MKBINUTILS != no

.include <bsd.prog.mk>
