.include <bsd.own.mk>

# Makefile for Reincarnation Server (RS)
PROG=	rs
SRCS=	exec.c main.c request.c manager.c table.c utility.c memory.c error.c

.if ${USE_PCI} != "no"
CPPFLAGS+= -DUSE_PCI
.endif

DPADD+=	${LIBSYS} ${LIBEXEC}
LDADD+=	-lsys -lexec

CPPFLAGS+=	-I${NETBSDSRCDIR}/minix

.include <minix.service.mk>
