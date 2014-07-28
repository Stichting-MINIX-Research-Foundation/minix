NOGCCERROR=yes
NOCLANGERROR=yes
CPPFLAGS+= -D_MINIX_SYSTEM

# Makefile for libsffs
.include <bsd.own.mk>

LIB=	sffs

SRCS=	dentry.c handle.c inode.c link.c lookup.c main.c misc.c mount.c \
	name.c path.c read.c stat.c table.c util.c verify.c write.c

.include <bsd.lib.mk>
