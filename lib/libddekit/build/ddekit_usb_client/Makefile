.if ${COMPILER_TYPE} == "gnu"
LIB=		ddekit_usb_client

SRC_DIR = ${MINIXSRCDIR}/lib/libddekit/src 

VPATH = $(SRC_DIR) 

SRCS = usb_client.c

CFLAGS += -Wall 

.endif
.include <bsd.lib.mk>

