# Makefile for crontab.

.include <bsd.own.mk>

PROG=	crontab
SRCS=	crontab.c tab.c misc.c
CPPFLAGS+= -I${.CURDIR} -I${NETBSDSRCDIR}/minix/commands/cron
BINMODE= 4755

.PATH: ${NETBSDSRCDIR}/minix/commands/cron

.include <bsd.prog.mk>
