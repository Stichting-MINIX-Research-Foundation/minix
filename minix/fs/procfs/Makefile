# Makefile for ProcFS server
.include <bsd.own.mk>

PROG=	procfs
SRCS=	buf.c cpuinfo.c main.c pid.c root.c service.c tree.c util.c

CPPFLAGS+= -I${NETBSDSRCDIR}/minix
CPPFLAGS+= -I${NETBSDSRCDIR}/minix/fs
CPPFLAGS+= -I${NETBSDSRCDIR}/minix/servers

DPADD+=	${LIBVTREEFS} ${LIBFSDRIVER}
LDADD+=	-lvtreefs -lfsdriver

.include <minix.service.mk>
