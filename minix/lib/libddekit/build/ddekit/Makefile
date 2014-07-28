LIB=		ddekit

SRC_DIR = ${NETBSDSRCDIR}/minix/lib/libddekit/src

VPATH = ${SRC_DIR} 

SRCS = pci.c printf.c mem.c pgtab.c dde.c initcall.c thread.c condvar.c \
       lock.c semaphore.c timer.c panic.c irq.c resource.c msg_queue.c

CPPFLAGS +=  -D_NETBSD_SOURCE -D_MINIX_SYSTEM

.include <bsd.lib.mk>

