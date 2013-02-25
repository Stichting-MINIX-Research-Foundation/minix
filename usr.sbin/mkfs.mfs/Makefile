PROG=	mkfs.mfs
SRCS=	mkfs.c
BINDIR=	/sbin
MAN=

.if ${HOST_OSTYPE:C/\-.*//:U} == "Minix" || !defined(HOSTPROGNAME)
LDADD+= -lminlib -lcompat_minix
.endif

.include <bsd.prog.mk>
