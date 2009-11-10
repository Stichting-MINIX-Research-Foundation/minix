#ifndef IPC_H
#define IPC_H

/* This header file defines constants for MINIX inter-process communication.
 * These definitions are used in the file proc.c.
 */
#include <minix/com.h>

/* Masks and flags for system calls. */
#define NON_BLOCKING    0x0080  /* do not block if target not ready */

/* System call numbers that are passed when trapping to the kernel. */
#define SEND		   1	/* blocking send */
#define RECEIVE		   2	/* blocking receive */
#define SENDREC	 	   3  	/* SEND + RECEIVE */
#define NOTIFY		   4	/* asynchronous notify */
#define SENDNB             5    /* nonblocking send */
#define SENDA		   16	/* asynchronous send */

#define WILLRECEIVE(target, source_ep) \
  ((RTS_ISSET(target, RTS_RECEIVING) && !RTS_ISSET(target, RTS_SENDING)) &&	\
    (target->p_getfrom_e == ANY || target->p_getfrom_e == source_ep))


#endif /* IPC_H */
