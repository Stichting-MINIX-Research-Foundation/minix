#ifndef IPC_H
#define IPC_H

/* This header file defines constants for MINIX inter-process communication.
 * These definitions are used in the file proc.c.
 */
#include <minix/com.h>
#include <minix/ipcconst.h>

/* Masks and flags for system calls. */
#define NON_BLOCKING    0x0080  /* do not block if target not ready */
#define FROM_KERNEL     0x0100  /* message from kernel on behalf of a process */

#define WILLRECEIVE(target, source_ep) \
  ((RTS_ISSET(target, RTS_RECEIVING) && !RTS_ISSET(target, RTS_SENDING)) &&	\
    (target->p_getfrom_e == ANY || target->p_getfrom_e == source_ep))

/* IPC status code macros. */
#define IPC_STATUS_GET(p)	((p)->p_reg.IPC_STATUS_REG)
#define IPC_STATUS_CLEAR(p)	((p)->p_reg.IPC_STATUS_REG = 0)

/*
 * XXX: the following check is used to set the status code only on RECEIVE.
 * SENDREC is not currently atomic for user processes. A process can return
 * from SENDREC in a different context than the original when a Posix signal
 * handler gets executed. For this reason, it is not safe to manipulate
 * the context (i.e. registers) when a process is blocked on a SENDREC.
 * Unfortunately, avoiding setting the status code for SENDREC doesn't solve
 * the problem entirely because in rare situations it is still necessary to
 * override retreg dynamically (and possibly in a different context).
 * A possible reliable solution is to improve our Posix signal handling
 * implementation and guarantee SENDREC atomicity w.r.t. the process context.
 */
#define IPC_STATUS_ADD(p, m)	do { \
        if(!((p)->p_misc_flags & MF_REPLY_PEND)) { \
            (p)->p_reg.IPC_STATUS_REG |= (m); \
        } \
    } while(0)
#define IPC_STATUS_ADD_CALL(p, call) \
    IPC_STATUS_ADD(p, IPC_STATUS_CALL_TO(call))
#define IPC_STATUS_ADD_FLAGS(p, flags) \
    IPC_STATUS_ADD(p, IPC_STATUS_FLAGS(flags))

#endif /* IPC_H */
