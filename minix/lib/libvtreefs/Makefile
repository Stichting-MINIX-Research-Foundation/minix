# Makefile for libvtreefs

NOGCCERROR=yes

CPPFLAGS+= -D_MINIX_SYSTEM

LIB=	vtreefs

CPPFLAGS+= -I${NETBSDSRCDIR}/include

SRCS=	\
	inode.c \
	link.c \
	mount.c \
	path.c \
	read.c \
	sdbm.c \
	stadir.c \
	table.c \
	vtreefs.c

.include <bsd.lib.mk>
