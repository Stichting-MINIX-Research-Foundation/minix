#	$NetBSD: Makefile,v 1.4 2009/04/14 22:15:24 lukem Exp $
#

.include <bsd.own.mk>

.if defined(__MINIX)
USE_KERBEROS= no
.endif

PROG=	newgrp
SRCS=	newgrp.c grutil.c
BINOWN=	root
BINMODE=4555

CPPFLAGS+=-DGRUTIL_ACCEPT_GROUP_NUMBERS
CPPFLAGS+=-DGRUTIL_ALLOW_GROUP_ERRORS	# for POSIX.1 compliance
#CPPFLAGS+=-DLOGIN_CAP

DPADD+=	${LIBCRYPT} ${LIBUTIL}
LDADD+= -lcrypt -lutil

.if (${USE_KERBEROS} != "no")
CPPFLAGS+=-DKERBEROS
.endif

.include <bsd.prog.mk>
