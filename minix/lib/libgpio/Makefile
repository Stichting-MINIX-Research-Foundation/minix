# Makefile for libgpio

CPPFLAGS+= -D_SYSTEM -D_MINIX_SYSTEM

LIB=	gpio

.if ${BSP_NAME} == omap
SRCS=	gpio_omap.c
.elif ${BSP_NAME} == rpi
SRCS=	gpio_rpi.c
.else
.error Unknown bsp ${BSP_NAME}
.endif

WARNS?=	5

CFLAGS+= -fno-builtin

.include <bsd.lib.mk>
