# Makefile for virtio_blk device
PROG=	virtio_blk
SRCS=	virtio_blk.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS} ${LIBMTHREAD} ${LIBVIRTIO}
LDADD+=	-lblockdriver -lsys -lmthread -lvirtio

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
