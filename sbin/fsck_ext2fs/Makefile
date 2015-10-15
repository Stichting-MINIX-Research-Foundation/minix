#	$NetBSD: Makefile,v 1.17 2014/03/04 21:07:22 joerg Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

.include <bsd.own.mk>

PROG=	fsck_ext2fs
MAN=    fsck_ext2fs.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c fsutil.c setup.c utilities.c ext2fs_bswap.c
FSCK=	${NETBSDSRCDIR}/sbin/fsck
CPPFLAGS+= -I${FSCK}
.PATH:	${NETBSDSRCDIR}/sys/ufs/ext2fs ${FSCK}

CWARNFLAGS.clang+=	-Wno-error=tautological-pointer-compare

.if defined(__MINIX)
# To compile with clang 3.4
NOCLANGERROR= yes
.endif # defined(__MINIX)

.include <bsd.prog.mk>

LDADD+=-lutil
DPADD+=${LIBUTIL}
