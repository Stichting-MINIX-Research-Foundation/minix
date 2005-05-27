/* Masks and flags for system calls. */
#define SYSCALL_FUNC	0x0F	/* mask for system call function */
#define SYSCALL_FLAGS   0xF0    /* mask for system call flags */
#define NON_BLOCKING    0x10	/* prevent blocking, return error */
#define FRESH_ANSWER    0x20	/* ignore pending notifications as answer */
				/* (default behaviour for SENDREC calls) */

/* System calls (numbers passed when trapping to the kernel) */
#define ECHO		 0	/* function code for echoing messages */
#define SEND		 1	/* function code for sending messages */
#define RECEIVE		 2	/* function code for receiving messages */
#define SENDREC	 	 3  	/* function code for SEND + RECEIVE */
#define NOTIFY		 4	/* function code for notifications */

