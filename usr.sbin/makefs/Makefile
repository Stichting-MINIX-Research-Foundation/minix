#	$NetBSD: Makefile,v 1.37 2019/10/13 07:28:19 mrg Exp $
#

WARNS?=	5

.include <bsd.own.mk>

PROG=	makefs
SRCS=	cd9660.c chfs.c ffs.c v7fs.c msdos.c udf.c\
	getid.c \
	makefs.c misc.c \
	pack_dev.c \
	spec.c \
	walk.c
MAN=	makefs.8

MKNODSRC=	${NETBSDSRCDIR}/sbin/mknod
MTREESRC=	${NETBSDSRCDIR}/usr.sbin/mtree

CPPFLAGS+=	-I${.CURDIR} -I${MKNODSRC} -I${MTREESRC} -DMAKEFS
#CPPFLAGS+=	-DMSDOSFS_DEBUG
.PATH:		${MKNODSRC} ${MTREESRC}

.include "${.CURDIR}/cd9660/Makefile.inc"
.include "${.CURDIR}/chfs/Makefile.inc"
.include "${.CURDIR}/ffs/Makefile.inc"
.include "${.CURDIR}/v7fs/Makefile.inc"
.include "${.CURDIR}/msdos/Makefile.inc"
.include "${.CURDIR}/udf/Makefile.inc"

.if !defined(HOSTPROG)
DPADD+= ${LIBUTIL}
LDADD+= -lutil

COPTS.cd9660.c+=		${GCC_NO_STRINGOP_TRUNCATION} ${GCC_NO_FORMAT_TRUNCATION}
COPTS.cd9660_conversion.c+=	${GCC_NO_FORMAT_TRUNCATION}
COPTS.udf.c+=			${GCC_NO_STRINGOP_TRUNCATION}
COPTS.v7fs_estimate.c+=		${GCC_NO_STRINGOP_TRUNCATION}
COPTS.v7fs_populate.c+=		${GCC_NO_STRINGOP_TRUNCATION} ${GCC_NO_FORMAT_TRUNCATION}
.endif

.include <bsd.prog.mk>
# DO NOT DELETE
