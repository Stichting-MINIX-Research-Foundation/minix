.if ${COMPILER_TYPE} == "gnu"
LIB=		ddekit_usb_server
SRC_DIR = ${MINIXSRCDIR}/lib/libddekit/src 
VPATH = $(SRC_DIR) 
SRCS = usb_server.c
CFLAGS += -Wall
.endif
.include <bsd.lib.mk>


