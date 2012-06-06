#	$NetBSD: Makefile,v 1.4 2009/11/10 10:24:50 skrll Exp $

.include <bsd.own.mk>

TOP=		${NETBSDSRCDIR}/external/gpl3/binutils

.include "${.CURDIR}/../common/Makefile.inc"
.include "${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk"

PROG=		as
SRCS=		${G_OBJS:Nitbl-[pl]*:Nm68k-parse.o:.o=.c} \
		${G_OBJS:Mitbl-parse.o:.o=.y} \
		${G_OBJS:Mitbl-lex.o:.o=.l} \
		${G_OBJS:Mm68k-parse.o:.o=.y}

COPTS.obj-elf.c = -Wno-stack-protector
COPTS.atof-generic.c = -Wno-stack-protector
COPTS.dwarf2dbg.c = -Wno-stack-protector
COPTS.symbols.c = -Wno-stack-protector
COPTS.stabs.c = -Wno-stack-protector
COPTS.macro.c = -Wno-stack-protector

CPPFLAGS+=	-I${.CURDIR}/arch/${MACHINE_ARCH}
CPPFLAGS+=	-I${DIST}/gas -I${DIST}/gas/config
CPPFLAGS+=	-I${DIST}

LDADD=		-lintl
DPADD=		${LIBINTL}

OPCODESOBJ!=	cd ${TOP}/${BFDSUBDIR}/libopcodes && ${PRINTOBJDIR}
LDADD+=		-L${OPCODESOBJ} -lopcodes
.if ${MKPICLIB} != "no"
DPADD+=		${OPCODESOBJ}/libopcodes_pic.a
.else
DPADD+=		${OPCODESOBJ}/libopcodes.a
.endif

BFDOBJ!=	cd ${TOP}/${BFDSUBDIR}/libbfd && ${PRINTOBJDIR}
LDADD+=		-L${BFDOBJ} -lbfd
.if ${MKPICLIB} != "no"
DPADD+=		${BFDOBJ}/libbfd_pic.a
.else
DPADD+=		${BFDOBJ}/libbfd.a
.endif

IBERTYOBJ!=	cd ${TOP}/${BFDSUBDIR}/libiberty && ${PRINTOBJDIR}
LDADD+=		-L${IBERTYOBJ} -liberty
DPADD+=		${IBERTYOBJ}/libiberty.a

TEXINFO=	as.texinfo
COMMONOBJDIR!=	cd ${TOP}/usr.bin/common && ${PRINTOBJDIR}
INFOFLAGS=	-I${DIST}/gas/doc -I${COMMONOBJDIR} -I${DIST}/libiberty

as.info:	bfdver.texi

.PATH: ${COMMONOBJDIR} ${DIST}/gas ${DIST}/gas/config ${DIST}/gas/doc

.include <bsd.prog.mk>
.include <bsd.info.mk>
