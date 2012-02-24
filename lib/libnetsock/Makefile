LIB = netsock

CPPFLAGS += -I${.CURDIR}/include/ -D_SYSTEM -D_MINIX
CPPFLAGS+= -I${.CURDIR}/../../lib/liblwip/include
CFLAGS += -Wall -Wextra -std=c99

SRCS += socket.c

.include <bsd.lib.mk>
