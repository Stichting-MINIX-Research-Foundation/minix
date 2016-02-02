# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:19 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH: ${HEIMDIST}/kcm

PROG= kcm
MAN=  kcm.8

NOGCCERROR=	# Heimdal is using its own deprecated functions

SRCS= \
	acl.c		\
	acquire.c	\
	cache.c		\
	client.c	\
	config.c	\
	connect.c	\
	events.c	\
	glue.c		\
	log.c		\
	main.c		\
	protocol.c	\
	renew.c		\
	sessions.c

CPPFLAGS+= -I${HEIMDIST}/lib/ipc

LDADD+=	-lkafs -lheimntlm -lutil
DPADD+=	${LIBKAFS} ${LIBHEIMNTLM} ${LIBUTIL}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
