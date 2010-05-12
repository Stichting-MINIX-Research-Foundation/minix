# Makefile for crontab.

.include <minix.own.mk>

PROG=	crontab
SRCS=	crontab.c tab.c misc.c
CPPFLAGS+= -I${.CURDIR} -I${MINIXSRCDIR}/commands/cron
BINMODE= 4755
MAN=

.PATH: ${MINIXSRCDIR}/commands/cron

.include <minix.prog.mk>
