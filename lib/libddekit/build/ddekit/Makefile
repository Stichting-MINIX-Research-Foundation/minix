.if ${COMPILER_TYPE} == "gnu"

LIB=		ddekit

SRC_DIR = ${MINIXSRCDIR}/lib/libddekit/src 

VPATH = $(SRC_DIR) 

SRCS = pci.c printf.c mem.c pgtab.c dde.c initcall.c thread.c condvar.c lock.c semaphore.c timer.c panic.c irq.c resource.c msg_queue.c

CFLAGS +=  -Wall 

.if ${NBSD_LIBC} != "no"
CFLAGS += -D_NETBSD_SOURCE
.endif

.endif
.include <bsd.lib.mk>

