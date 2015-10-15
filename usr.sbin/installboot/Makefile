#	$NetBSD: Makefile,v 1.50 2015/06/06 15:45:47 joerg Exp $
#

.include <bsd.own.mk>

PROG=	installboot_nbsd
MAN=	installboot_nbsd.8
SRCS=	installboot.c sum.c machines.c fstypes.c install_master.c


.if !defined(__MINIX)
ARCH_XLAT= amd64-i386.c news68k-news.c newsmips-news.c
ARCH_XLAT+= sun2-sun68k.c sun3-sun68k.c
.else
ARCH_XLAT= amd64-i386.c
.endif # !defined(__MINIX)

.if !defined(SMALLPROG) && !defined(ARCH_FILES)
.if !defined(__MINIX)
ARCH_FILES=  alpha.c amiga.c emips.c ews4800mips.c hp300.c hppa.c i386.c
ARCH_FILES+= landisk.c macppc.c news.c next68k.c pmax.c
ARCH_FILES+= sparc.c sparc64.c sun68k.c vax.c x68k.c
.else
ARCH_FILES=  i386.c
.endif # !defined(__MINIX)
.else
ARCH_FILES?= ${ARCH_XLAT:M${MACHINE}-*:S/${MACHINE}-//}
.if empty(ARCH_FILES)
ARCH_FILES= ${MACHINE}.c
.endif
.endif

SRCS+=${ARCH_FILES}

.if !defined(__MINIX)
.if empty(ARCH_FILES:C/(macppc|news|sparc|sun68k|x68k)/stg2/:Mstg2.c)
CPPFLAGS	+= -DNO_STAGE2
.else
SRCS+= bbinfo.c

# fstypes are only needed for 'stage2' and then only from bbinfo.
SRCS+= ffs.c
.if SMALLPROG
CPPFLAGS+=	-DNO_FFS_SWAP
.else
SRCS+= ffs_bswap.c
.endif
#SRCS+= ext2fs.c ext2fs_bswap.c
.endif

UFSSRC=		${NETBSDSRCDIR}/sys/ufs
CPPFLAGS+=	-I${.CURDIR} -I.
.PATH:		${.CURDIR}/arch ${UFSSRC}/ffs ${UFSSRC}/ext2fs
.else
CPPFLAGS	+= -DNO_STAGE2
SRCS+= minixfs3.c
.PATH:		${.CURDIR}/arch
CPPFLAGS+=	-I${.CURDIR} -I.
.endif # !defined(__MINIX)

.if !defined(HOSTPROGNAME)
.for f in i386 macppc
COPTS.${f}.c+=  -Wno-pointer-sign
.endfor

LDADD+= -lutil
DPADD+= ${LIBUTIL}
.endif

.include <bsd.prog.mk>
