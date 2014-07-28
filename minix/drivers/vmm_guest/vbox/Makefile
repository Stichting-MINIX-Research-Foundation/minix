# Makefile for the VirtualBox backdoor driver (VBOX)
PROG=	vbox
SRCS=	vbox.c hgcm.c err.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

.include <minix.service.mk>
