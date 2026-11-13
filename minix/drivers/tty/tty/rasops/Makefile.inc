# Makefile for framebuffer console driver

.include <bsd.own.mk>

HERE=${.CURDIR}/rasops
.PATH:  ${HERE}

SRCS +=	rasops.c
SRCS +=	rasops_masks.c
SRCS +=	rasops1.c rasops2.c rasops4.c rasops8.c rasops15.c rasops24.c rasops32.c

