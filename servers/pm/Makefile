.include <bsd.own.mk>

# Makefile for Process Manager (PM)
PROG=	pm
SRCS=	main.c forkexit.c break.c exec.c time.c alarm.c \
	signal.c utility.c table.c getset.c misc.c \
	profile.c dma.c schedule.c

.if ${USE_MCONTEXT} != "no"
SRCS+= mcontext.c
CPPFLAGS+= -DUSE_MCONTEXT
.endif

.if ${USE_TRACE} != "no"
SRCS+= trace.c
CPPFLAGS+= -DUSE_TRACE
.endif

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 32k

CPPFLAGS.main.c+=	-I${MINIXSRCDIR}
CPPFLAGS.misc.c+=	-I${MINIXSRCDIR}
CPPFLAGS.schedule.c+=	-I${MINIXSRCDIR}
CPPFLAGS.utility.c+=	-I${MINIXSRCDIR}

.include <minix.bootprog.mk>
