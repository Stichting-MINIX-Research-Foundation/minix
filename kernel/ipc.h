#ifndef IPC_H
#define IPC_H

#include <minix/com.h>

/* Masks and flags for system calls. */
#define SYSCALL_FUNC	0x0F	/* mask for system call function */
#define SYSCALL_FLAGS   0xF0    /* mask for system call flags */
#define NON_BLOCKING    0x10	/* prevent blocking, return error */
#define FRESH_ANSWER    0x20	/* ignore pending notifications as answer */
				/* (default behaviour for SENDREC calls) */

/* System call numbers that are passed when trapping to the kernel. The 
 * numbers are carefully defined so that it can easily be seen (based on 
 * the bits that are on) which checks should be done in sys_call().
 */
#define SEND		 1	/* 0 0 0 1 : blocking send */
#define RECEIVE		 2	/* 0 0 1 0 : blocking receive */
#define SENDREC	 	 3  	/* 0 0 1 1 : SEND + RECEIVE */
#define NOTIFY		 4	/* 0 1 0 0 : nonblocking notify */
#define ECHO		 8	/* 1 0 0 0 : echo a message */

/* The following bit masks determine what checks that should be done. */
#define CHECK_PTR     0x0B      /* 1 0 1 1 : validate message buffer */
#define CHECK_DST     0x05	/* 0 1 0 1 : validate message destination */
#define CHECK_SRC     0x02	/* 0 0 1 0 : validate message source */

/* Call masks indicating which system calls (traps) a process can make. 
 * System processes can do anything; user processes are highly restricted. 
 */
#define EMPTY_MASK 		(0)
#define FILLED_MASK 		(~0)
#define TASK_CALL_MASK	 	(1 << RECEIVE)
#define USER_CALL_MASK	 	((1 << SENDREC) | (1 << ECHO))

/* Send masks determine to whom processes can send messages or notifications. 
 * The values here are used for the processes in the boot image. We rely on 
 * the initialization code in main() to match the s_nr_to_id() mapping for the
 * processes in the boot image, so that the send mask that is defined here 
 * can be directly copied onto map[0] of the actual send mask. Privilege
 * structure 0 is shared by user processes. 
 *
 * Note that process numbers in the boot image should not be higher than
 * "BITCHUNK_BITS - NR_TASKS", because a bitchunk_t field is used to store 
 * the send masks in the table that describes that processes in the image.  
 */
#define s_nr_to_id(n)		(NR_TASKS + (n) + 1)
#define s(n)			(1 << s_nr_to_id(n))
#define USER_SEND_MASK		(s(PM_PROC_NR) | s(FS_PROC_NR))
#define DRIVER_SEND_MASK	(s(PM_PROC_NR) | s(FS_PROC_NR) | s(SYSTEM) | \
	 			 s(CLOCK) | s(PRINTF_PROC) | s(TTY))
#define SERVER_SEND_MASK	(~0)
#define SYSTEM_SEND_MASK	(~1)

/* Sanity check to make sure the send masks can be set. */
extern int dummy[(BITCHUNK_BITS-NR_TASKS > INIT_PROC_NR) ? 1 : -1];

#endif /* IPC_H */
