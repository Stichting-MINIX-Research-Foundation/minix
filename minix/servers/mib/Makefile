# Makefile for the Management Information Base (MIB) server

.include <bsd.own.mk>

.if !empty(DBG:M-Og) || !empty(CFLAGS:M-Og) || \
	!empty(DBG:M-g) || !empty(CFLAGS:M-g)
#LSC: -Wno-maybe-uninitialized while compiling with -DNDEBUG -Og
CWARNFLAGS.gcc+=	-Wno-maybe-uninitialized
.endif

PROG=	mib
SRCS=	main.c tree.c remote.c kern.c vm.c hw.c proc.c minix.c

CPPFLAGS+= -I${NETBSDSRCDIR}/minix

DPADD+=	${LIBSYS}
LDADD+=	-lsys

WARNS?=	5

.include <minix.service.mk>
