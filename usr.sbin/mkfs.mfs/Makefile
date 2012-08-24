PROG=	mkfs.mfs
SRCS=	mkfs.c
BINDIR=	/sbin
MAN=

CPPFLAGS+= -D_MINIX

LDADD?= -lminlib -lcompat_minix

.include <bsd.prog.mk>
