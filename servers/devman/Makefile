PROG =	devman

SRCS =  main.c device.c buf.c bind.c


DPADD+=	${LIBSYS}
LDADD =  -lvtreefs -lsys
BINDIR?= /sbin
INSTALLFLAGS+=	-S 128k

MAN=
.include <minix.service.mk>
