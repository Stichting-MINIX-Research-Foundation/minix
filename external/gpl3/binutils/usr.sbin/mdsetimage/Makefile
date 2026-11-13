#	$NetBSD: Makefile,v 1.13 2016/09/22 01:53:39 christos Exp $

.include <bsd.init.mk>

MDSETIMAGE=${NETBSDSRCDIR}/usr.sbin/mdsetimage
.PATH: ${MDSETIMAGE}

.if (defined(HOSTPROG) && \
     (${TOOLCHAIN_MISSING} == "no" || defined(EXTERNAL_TOOLCHAIN))) || \
    ${MKBINUTILS} != "no"
BFDSUBDIR=	lib

PROG=		mdsetimage
SRCS=		mdsetimage.c bin_bfd.c
MAN=		mdsetimage.8

CPPFLAGS+=	-I${DIST}/bfd -I${DIST}/binutils -I${DIST}/include -I${MDSETIMAGE}

.if !defined(HOSTPROG)
PROGDPLIBS+=	bfd	${TOP}/${BFDSUBDIR}/libbfd
PROGDPLIBS+=	iberty	${TOP}/${BFDSUBDIR}/libiberty
.else
DPADD+=		${BFDDIR}/libbfd.a
LDADD+=		-L${BFDDIR} -lbfd
DPADD+=		${IBERTYDIR}/libiberty.a
LDADD+=		-L${IBERTYDIR} -liberty
CPPFLAGS+=	-I${BFDDIR}
.endif

.include <bsd.prog.mk>

.ifndef HOSTPROG
.include	"${TOP}/${BFDSUBDIR}/libbfd/arch/${BINUTILS_MACHINE_ARCH}/defs.mk"

CPPFLAGS+=	-I${TOP}/${BFDSUBDIR}/libbfd/arch/${BINUTILS_MACHINE_ARCH} ${G_INCLUDES}
LDADD+=		-lintl
DPADD+=		${LIBINTL}
DPADD+=		${LIBZ}
.endif # HOSTPROG

LDADD+=		-lz
# No DPADD because we don't know the format and we didn't build it.

.endif # MKBINUTILS != no
