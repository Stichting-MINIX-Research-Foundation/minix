# Makefile for libsffs
.include <bsd.own.mk>

NOGCCERROR=yes

CPPFLAGS+= -D_MINIX_SYSTEM

LIB=	sffs

SRCS=	dentry.c handle.c inode.c link.c lookup.c main.c misc.c mount.c \
	name.c path.c read.c stat.c table.c verify.c write.c

.include <bsd.lib.mk>
