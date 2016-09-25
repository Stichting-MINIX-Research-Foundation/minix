# Makefile for arch-dependent TTY code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:  ${HERE}

SRCS +=	console.c keyboard.c

.if ${BSP_NAME} == rpi
SRCS +=	pl011.c
.elif ${BSP_NAME} == omap
SRCS +=	omap.c
.else
.error Unknown bsp ${BSP_NAME}
.endif
