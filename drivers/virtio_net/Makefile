#
# Makefile for virtio_net
#
PROG=	virtio_net
SRCS=	virtio_net.c

FILES=$(PROG).conf
FILESNAME=$(PROG)
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBNETDRIVER} ${LIBSYS} ${LIBVIRTIO}
LDADD+=	-lnetdriver -lsys -lvirtio

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
