# Makefile for the Management Information Base (MIB) server

PROG=	mib
SRCS=	main.c tree.c kern.c vm.c hw.c proc.c minix.c

CPPFLAGS+= -I${NETBSDSRCDIR}/minix

DPADD+=	${LIBSYS}
LDADD+=	-lsys

WARNS?=	5

.include <minix.service.mk>
