# Makefile for the Orinoco wireless card (Prism chipset)
PROG=	orinoco
SRCS=	orinoco.c hermes.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

CPPFLAGS.orinoco.c+=	-I${NETBSDSRCDIR}/minix

.include <minix.service.mk>
