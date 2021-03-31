#	$NetBSD: Makefile,v 1.9 2016/01/26 19:37:48 christos Exp $

CPPFLAGS+=	-I${.CURDIR}/arch/${BINUTILS_MACHINE_ARCH}

.include "${.CURDIR}/../common/Makefile.inc"
.include "${.CURDIR}/arch/${BINUTILS_MACHINE_ARCH}/defs.mk"

PROG=		gprof
SRCS=		${G_gprof_OBJECTS:.o=.c}

PROGDPLIBS+=	bfd	${TOP}/${BFDSUBDIR}/libbfd
PROGDPLIBS+=	iberty	${TOP}/${BFDSUBDIR}/libiberty

TEXINFO=	${G_TEXINFOS}
COMMONOBJDIR!=	cd ${TOP}/usr.bin/common && ${PRINTOBJDIR}
INFOFLAGS=	-I${COMMONOBJDIR} -I${DIST}/libiberty

gprof.info:	bfdver.texi

.PATH: ${COMMONOBJDIR} ${DIST}/gprof

.include <bsd.prog.mk>
.include <bsd.info.mk>

.if !defined(__MINIX)
LDADD+=	-lintl -lz
DPADD+=	${LIBINTL} ${LIBZ}
.else
LDADD+= -lz
DPADD+= ${LIBZ}
.endif # !defined(__MINIX)
