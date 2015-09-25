FSCK=   ${NETBSDSRCDIR}/sbin/fsck
PROG=	fsck_mfs
SRCS=	fsck.c
CPPFLAGS+= -I${NETBSDSRCDIR}/minix/fs -I${FSCK}
BINDIR=	/sbin
MAN=	fsck.mfs.1

.include <bsd.prog.mk>
