# Makefile for the UNIX Domain Sockets driver (UDS)
PROG=	uds
SRCS=	uds.c io.c stat.c
MAN=	unix.8

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBSOCKEVENT} ${LIBSOCKDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsockevent -lsockdriver -lsys -ltimers

.if !empty(DBG:M-Os) || !empty(CFLAGS:M-Os) || \
	!empty(DBG:M-Og) || !empty(CFLAGS:M-Og) || \
	!empty(DBG:M-g) || !empty(CFLAGS:M-g)
#LSC: -Wno-maybe-uninitialized while compiling with -DNDEBUG -Os
#LSC: -Wno-maybe-uninitialized while compiling with -DNDEBUG -Og
CWARNFLAGS.gcc+=	-Wno-maybe-uninitialized
.endif

WARNS?=	5

.include <minix.service.mk>
