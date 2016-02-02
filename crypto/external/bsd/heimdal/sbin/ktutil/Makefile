# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:19 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

USE_FORT?= yes	# network client

.PATH: ${HEIMDIST}/admin

PROG= ktutil

MAN= ktutil.8

HEIMSRCS= ktutil-commands.in

SRCS=			\
	add.c		\
	change.c	\
	copy.c		\
	destroy.c	\
	get.c		\
	ktutil.c	\
	ktutil_locl.h	\
	list.c		\
	purge.c		\
	remove.c	\
	rename.c

LDADD+= -lkadm5clnt -lsl -ledit -lterminfo -lutil
DPADD+= ${LIBKADM5CLNT} ${LIBSL} ${LIBEDIT} ${LIBTERMINFO} ${LIBUTIL}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
