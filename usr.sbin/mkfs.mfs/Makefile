PROG=	mkfs.mfs
SRCS=	mkfs.c
BINDIR=	/sbin
MAN=

CPPFLAGS+= -D_MINIX

# LSC Adding this to activate the getline renaming macro
CPPFLAGS+= -D__NBSD_LIBC

.if ${HOST_OSTYPE:C/\-.*//:U} == "Minix" || !defined(HOSTPROGNAME)
LDADD+= -lminlib -lcompat_minix
.endif

.include <bsd.prog.mk>
