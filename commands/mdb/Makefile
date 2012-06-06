#
# Makefile for mdb
#
#
# 
# i)   For GNU_EXEC Support, uncomment:
#
FOR_GNU=	gnu_sym.c
DEF_GNU=	-DGNU_SUPPORT
# 
# ii)  For tracing of syscalls, uncomment:
#
#FOR_SYSCALLS=	syscalls.c decode.c ioctl.c
#DEF_SYSCALLS=	-DSYSCALLS_SUPPORT
#
# iii) For no debugging of mdb, uncomment:
#
#DEF_DEBUG=	-DNDEBUG

EXTRA_SRCS=	${FOR_GNU} ${FOR_SYSCALLS}
EXTRA_DEFS=	${DEF_GNU} ${DEF_SYSCALLS} ${DEF_DEBUG}
CPPFLAGS+=	-I${NETBSDSRCDIR} -I${NETBSDSRCDIR}/servers \
		 ${EXTRA_DEFS}

PROG=	mdb
SRCS=	mdb.c mdbexp.c kernel.o sym.c trace.c core.c misc.c io.c
SRCS+=	mdbdis86.c
SRCS+=	${EXTRA_SRCS}

.include <bsd.prog.mk>
