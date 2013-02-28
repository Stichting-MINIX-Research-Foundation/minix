.include <bsd.own.mk>

# Makefile for Process Manager (PM)
PROG=	pm
SRCS=	main.c forkexit.c break.c exec.c time.c alarm.c \
	signal.c utility.c table.c getset.c misc.c \
	profile.c schedule.c

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

CPPFLAGS.main.c+=	-I${NETBSDSRCDIR}
CPPFLAGS.misc.c+=	-I${NETBSDSRCDIR}
CPPFLAGS.schedule.c+=	-I${NETBSDSRCDIR}
CPPFLAGS.utility.c+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
