.include <bsd.own.mk>

# Makefile for Process Manager (PM)
PROG=	pm
SRCS=	main.c forkexit.c exec.c time.c alarm.c \
	signal.c utility.c table.c trace.c getset.c misc.c \
	profile.c mcontext.c schedule.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

CPPFLAGS.main.c+=	-I${NETBSDSRCDIR}/minix
CPPFLAGS.misc.c+=	-I${NETBSDSRCDIR}/minix
CPPFLAGS.schedule.c+=	-I${NETBSDSRCDIR}/minix
CPPFLAGS.utility.c+=	-I${NETBSDSRCDIR}/minix

.include <minix.service.mk>
