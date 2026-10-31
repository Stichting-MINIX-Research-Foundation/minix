# Makefile for arch-dependent TTY code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:  ${HERE}

SRCS +=	keyboard.c rs232.c
SRCS +=	console.c ser_console.c fb_console.c genfb.c

