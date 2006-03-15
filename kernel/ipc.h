#ifndef IPC_H
#define IPC_H

/* This header file defines constants for MINIX inter-process communication.
 * These definitions are used in the file proc.c.
 */
#include <minix/com.h>

/* Masks and flags for system calls. */
#define SYSCALL_FUNC	0x000F	/* mask for system call function */
#define SYSCALL_FLAGS   0x00F0  /* mask for system call flags */
#define NON_BLOCKING    0x0010  /* do not block if target not ready */

/* System call numbers that are passed when trapping to the kernel. The 
 * numbers are carefully defined so that it can easily be seen (based on 
 * the bits that are on) which checks should be done in sys_call().
 */
#define SEND		   1	/* 0001 : blocking send */
#define RECEIVE		   2	/* 0010 : blocking receive */
#define SENDREC	 	   3  	/* 0011 : SEND + RECEIVE */
#define NOTIFY		   4	/* 0100 : nonblocking notify */
#define ECHO		   8	/* 1000 : echo a message */

#define IPC_REQUEST	   5	/* 0101 : blocking request */
#define IPC_REPLY	   6    /* 0110 : nonblocking reply */
#define IPC_NOTIFY	   7    /* 0111 : nonblocking notification */
#define IPC_RECEIVE	   9	/* 1001 : blocking receive */

/* The following bit masks determine what checks that should be done. */
#define CHECK_PTR       0xBB	/* 1011 1011 : validate message buffer */
#define CHECK_DST       0x55	/* 0101 0101 : validate message destination */
#define CHECK_DEADLOCK  0x93	/* 1001 0011 : check for deadlock */

#endif /* IPC_H */
