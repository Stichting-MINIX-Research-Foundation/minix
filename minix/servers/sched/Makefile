# Makefile for Scheduler (SCHED)
PROG=	sched
SRCS=	main.c schedule.c utility.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

CPPFLAGS.main.c+=	-I${NETBSDSRCDIR}/minix
CPPFLAGS.schedule.c+=	-I${NETBSDSRCDIR}/minix
CPPFLAGS.utility.c+=	-I${NETBSDSRCDIR}/minix

.include <minix.service.mk>
