#ifndef _IPC_CONST_H
#define _IPC_CONST_H

 /* System call numbers that are passed when trapping to the kernel. */
#define SEND		   1	/* blocking send */
#define RECEIVE		   2	/* blocking receive */
#define SENDREC	 	   3  	/* SEND + RECEIVE */
#define NOTIFY		   4	/* asynchronous notify */
#define SENDNB             5    /* nonblocking send */
#define SENDA		   16	/* asynchronous send */

/* Macros for IPC status code manipulation. */
#define IPC_STATUS_CALL_SHIFT	0
#define IPC_STATUS_CALL_MASK	0x3F
#define IPC_STATUS_CALL(status)	\
	(((status) >> IPC_STATUS_CALL_SHIFT) & IPC_STATUS_CALL_MASK)
#define IPC_STATUS_CALL_TO(call) \
	(((call) & IPC_STATUS_CALL_MASK) << IPC_STATUS_CALL_SHIFT)

#endif /* IPC_CONST_H */
