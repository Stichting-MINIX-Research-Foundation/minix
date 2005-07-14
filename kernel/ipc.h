#ifndef IPC_H
#define IPC_H

/* Masks and flags for system calls. */
#define SYSCALL_FUNC	0x0F	/* mask for system call function */
#define SYSCALL_FLAGS   0xF0    /* mask for system call flags */
#define NON_BLOCKING    0x10	/* prevent blocking, return error */
#define FRESH_ANSWER    0x20	/* ignore pending notifications as answer */
				/* (default behaviour for SENDREC calls) */

/* System calls (numbers passed when trapping to the kernel). */
#define ECHO		 0	/* function code for echoing messages */
#define SEND		 1	/* function code for sending messages */
#define RECEIVE		 2	/* function code for receiving messages */
#define SENDREC	 	 3  	/* function code for SEND + RECEIVE */
#define NOTIFY		 4	/* function code for notifications */
#define ALERT		 5	/* function code for alerting */

/* Call masks indicating which system calls a process can make. */
#define EMPTY_CALL_MASK 	(0)
#define _USER_CALL_MASK	 	((1 << SENDREC) | (1 << ALERT))
#define SYSTEM_CALL_MASK 	(~0)
#define USER_CALL_MASK 	(~0)


#endif /* IPC_H */
