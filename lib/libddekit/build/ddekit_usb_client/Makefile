.if ${COMPILER_TYPE} == "gnu"
LIB=		ddekit_usb_client
SRC_DIR = /usr/src/lib/libddekit/src 
VPATH = $(SRC_DIR) 
SRCS = usb_client.c
CFLAGS += -DDDEBUG=1 -Wall -I../../include
.endif
.include <bsd.lib.mk>

