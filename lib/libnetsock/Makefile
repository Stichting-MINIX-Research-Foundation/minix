LIB = netsock

CPPFLAGS += -I${.CURDIR}/include/ -D_SYSTEM
CPPFLAGS+= -I${.CURDIR}/../../lib/liblwip/include -I${.CURDIR}/../../lib/liblwip/include/ipv4 -I${.CURDIR}/../../lib/liblwip/include/ipv6
CFLAGS += -Wall -Wextra -std=c99

SRCS += socket.c

.include <bsd.lib.mk>
