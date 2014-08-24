PROG =	devman
SRCS =  main.c device.c buf.c bind.c

DPADD+=	${LIBVTREEFS} ${LIBFSDRIVER} ${LIBSYS}
LDADD+=  -lvtreefs -lfsdriver -lsys

.include <minix.service.mk>
