
LIB=		ddekit

SRC_DIR = ${NETBSDSRCDIR}/lib/libddekit/src

VPATH = $(SRC_DIR) 

SRCS = pci.c printf.c mem.c pgtab.c dde.c initcall.c thread.c condvar.c lock.c semaphore.c timer.c panic.c irq.c resource.c msg_queue.c

CFLAGS +=  -Wall -D_NETBSD_SOURCE

.include <bsd.lib.mk>

