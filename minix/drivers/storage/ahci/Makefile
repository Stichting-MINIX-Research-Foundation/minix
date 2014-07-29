# Makefile for the Advanced Host Controller Interface driver (AHCI)
PROG=	ahci
SRCS=	ahci.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS} ${LIBTIMERS} ${LIBMTHREAD}
LDADD+=	-lblockdriver -lsys -ltimers -lmthread

.include <minix.service.mk>
