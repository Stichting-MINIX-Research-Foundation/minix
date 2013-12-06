NOGCCERROR=yes
NOCLANGERROR=yes

LIB = lwip

CPPFLAGS += -I${.CURDIR}/include/ -I${.CURDIR}/include/ipv4 -I${.CURDIR}/include/ipv6 -D_SYSTEM

#.include "${.CURDIR}/api/Makefile.inc"
.include "${.CURDIR}/core/Makefile.inc"
.include "${.CURDIR}/netif/Makefile.inc"

SRCS += sys_arch.c

.include <bsd.lib.mk>
