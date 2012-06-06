# Makefile for crontab.

.include <bsd.own.mk>

PROG=	crontab
SRCS=	crontab.c tab.c misc.c
CPPFLAGS+= -I${.CURDIR} -I${NETBSDSRCDIR}/commands/cron
BINMODE= 4755
MAN=

.PATH: ${NETBSDSRCDIR}/commands/cron

.include <bsd.prog.mk>
