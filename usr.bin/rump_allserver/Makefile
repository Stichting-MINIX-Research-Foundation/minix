#	$NetBSD: Makefile,v 1.7 2013/11/13 17:47:27 pooka Exp $
#

PROG=	rump_allserver
MLINKS+=rump_allserver.1 rump_server.1

RUMPTOP=${.CURDIR}/../../sys/rump

.include "${RUMPTOP}/dev/Makefile.rumpdevcomp"
.include "${RUMPTOP}/fs/Makefile.rumpfscomp"
.include "${RUMPTOP}/kern/Makefile.rumpkerncomp"
.include "${RUMPTOP}/net/Makefile.rumpnetcomp"

LDADD+=	${RUMPDEVLDADD} ${RUMPFSLDADD} ${RUMPKERNLDADD} ${RUMPNETLDADD}
.if defined(__MINIX)
LDADD+= -lrumpdev -lrumpvfs -lrumpnet -lrump -lrumpuser
.else
LDADD+= -lrumpdev -lrumpvfs -lrumpnet -lrump -lrumpuser -lpthread
.endif

.include <bsd.prog.mk>
