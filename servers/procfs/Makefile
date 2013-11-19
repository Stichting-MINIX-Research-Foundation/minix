# Makefile for ProcFS server
#

.include <bsd.own.mk>

PROG=	procfs
SRCS=	buf.c main.c pid.c root.c tree.c util.c cpuinfo.c

CPPFLAGS+= -I${NETBSDSRCDIR} -I${NETBSDSRCDIR}/servers

DPADD+=	${LIBVTREEFS} ${LIBMINLIB}
LDADD+=	-lvtreefs -lminlib

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
