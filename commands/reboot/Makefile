# Makefile for shutdown / halt / reboot.

PROGS=	shutdown halt
SRCS.shutdown=	shutdown.c sh_wall.c log.c
SRCS.halt=	halt.c log.c
BINMODE= 4754
LINKS+=	${BINDIR}/halt ${BINDIR}/reboot
MAN.shutdown=
MAN.halt=

.include <bsd.prog.mk>
