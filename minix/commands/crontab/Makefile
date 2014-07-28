# Makefile for crontab.

.include <bsd.own.mk>

# LSC For now...
NOGCCERROR:= yes

PROG=	crontab
SRCS=	crontab.c tab.c misc.c
CPPFLAGS+= -I${.CURDIR} -I${NETBSDSRCDIR}/minix/commands/cron
BINMODE= 4755
MAN=

.PATH: ${NETBSDSRCDIR}/minix/commands/cron

.include <bsd.prog.mk>
