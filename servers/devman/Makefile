PROG =	devman

SRCS =  main.c device.c buf.c bind.c


DPADD+=	${LIBSYS}
LDADD =  -lvtreefs -lsys
BINDIR?= /sbin

MAN=
.include <minix.service.mk>
