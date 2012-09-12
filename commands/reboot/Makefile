# Makefile for shutdown / halt / reboot.

PROGS=	shutdown halt
SRCS.shutdown=	shutdown.c sh_wall.c log.c
SRCS.halt=	halt.c log.c
BINDIR=/bin
BINMODE= 4754
SYMLINKS+=	${BINDIR}/halt ${BINDIR}/reboot
SYMLINKS+=	${BINDIR}/halt /usr/bin/halt
SYMLINKS+=	${BINDIR}/shutdown /usr/bin/shutdown
SYMLINKS+=	${BINDIR}/reboot /usr/bin/reboot
MAN.shutdown=
MAN.halt=

CPPFLAGS+= -D__NBSD_LIBC

.include <bsd.prog.mk>
