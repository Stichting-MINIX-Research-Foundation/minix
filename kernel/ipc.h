#ifndef IPC_H
#define IPC_H

/* This header file defines constants for MINIX inter-process communication.
 * These definitions are used in the file proc.c.
 */
#include <minix/com.h>

/* Masks and flags for system calls. */
#define NON_BLOCKING    0x0080  /* do not block if target not ready */
#define FROM_KERNEL     0x0100  /* message from kernel on behalf of a process */

#define WILLRECEIVE(target, source_ep) \
  ((RTS_ISSET(target, RTS_RECEIVING) && !RTS_ISSET(target, RTS_SENDING)) &&	\
    (target->p_getfrom_e == ANY || target->p_getfrom_e == source_ep))

/* IPC status code macros. */
#define IPC_STATUS_REG		bx
#define IPC_STATUS_GET(p)	((p)->p_reg.IPC_STATUS_REG)
#define IPC_STATUS_SET(p, m)	((p)->p_reg.IPC_STATUS_REG = m)
#define IPC_STATUS_CLEAR(p)	IPC_STATUS_SET(p, 0)
#define IPC_STATUS_ADD(p, m)	((p)->p_reg.IPC_STATUS_REG |= m)

#endif /* IPC_H */
