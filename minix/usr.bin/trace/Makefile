.include <bsd.own.mk>

PROG=	trace
SRCS=	call.o error.o escape.o format.o ioctl.o kernel.o mem.o output.o \
	proc.o signal.o trace.o
.PATH:	${.CURDIR}/service
SRCS+=	pm.o vfs.o rs.o mib.o vm.o ipc.o
.PATH:	${.CURDIR}/ioctl
SRCS+=	block.o char.o net.o svrctl.o

CPPFLAGS+=	-D_MINIX_SYSTEM=1 -I${.CURDIR} -I${NETBSDSRCDIR}/minix

COPTS.format.c+=	-Wno-format-nonliteral

error.c: error.awk ${NETBSDSRCDIR}/sys/sys/errno.h
	${TOOL_AWK} -f ${.ALLSRC} > ${.TARGET}

signal.c: signal.awk ${NETBSDSRCDIR}/sys/sys/signal.h
	${TOOL_AWK} -f ${.ALLSRC} > ${.TARGET}

CLEANFILES+=	error.c signal.c

WARNS?=	5

.include <bsd.prog.mk>
