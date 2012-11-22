# Makefile for ProcFS server
#

.include <bsd.own.mk>

PROG=	procfs
SRCS=	buf.c main.c pid.c root.c tree.c util.c cpuinfo.c mounts.c

CPPFLAGS+= -I${NETBSDSRCDIR} -I${NETBSDSRCDIR}/servers

DPADD+=	${LIBVTREEFS} ${LIBSYS}
LDADD+=	-lvtreefs -lsys

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
