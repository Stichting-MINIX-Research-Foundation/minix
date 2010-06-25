# Makefile for crontab.

.include <bsd.own.mk>

PROG=	crontab
SRCS=	crontab.c tab.c misc.c
CPPFLAGS+= -I${.CURDIR} -I${MINIXSRCDIR}/commands/cron
BINMODE= 4755
MAN=

.PATH: ${MINIXSRCDIR}/commands/cron

.include <bsd.prog.mk>
