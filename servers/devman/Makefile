PROG =	devman

SRCS =  main.c device.c buf.c bind.c


DPADD+=	${LIBSYS} ${LIBMINLIB}
LDADD =  -lvtreefs -lsys -lminlib
BINDIR?= /sbin

MAN=
.include <minix.service.mk>
