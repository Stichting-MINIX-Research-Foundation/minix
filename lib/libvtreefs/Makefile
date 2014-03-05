NOGCCERROR=yes
NOCLANGERROR=yes
CPPFLAGS+= -D_MINIX_SYSTEM

# Makefile for libvtreefs

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
	utility.c \
	vtreefs.c

.include <bsd.lib.mk>
