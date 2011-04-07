LIB = lwip

CPPFLAGS += -I${.CURDIR}/include/ -D_SYSTEM

#.include "${.CURDIR}/api/Makefile.inc"
.include "${.CURDIR}/core/Makefile.inc"
.include "${.CURDIR}/netif/Makefile.inc"

SRCS += sys_arch.c

.include <bsd.lib.mk>
