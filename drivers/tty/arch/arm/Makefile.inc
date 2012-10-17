# Makefile for arch-dependent TTY code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:  ${HERE}

SRCS +=	console.c keyboard.c rs232.c
