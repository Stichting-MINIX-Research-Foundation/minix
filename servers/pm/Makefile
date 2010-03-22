# Makefile for Process Manager (PM)
PROG=	pm
SRCS=	main.c forkexit.c break.c exec.c time.c timers.c alarm.c \
	signal.c utility.c table.c trace.c getset.c misc.c \
	profile.c dma.c mcontext.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 32k

.include <minix.prog.mk>
