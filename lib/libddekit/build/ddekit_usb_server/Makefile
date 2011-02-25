.if ${COMPILER_TYPE} == "gnu"
LIB=		ddekit_usb_server
SRC_DIR = /usr/src/lib/libddekit/src 
VPATH = $(SRC_DIR) 
SRCS = usb_server.c
CFLAGS += -DDDEBUG=1 -I../../include -Wall
.endif
.include <bsd.lib.mk>


