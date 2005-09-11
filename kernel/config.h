#ifndef CONFIG_H
#define CONFIG_H

/* This file defines the kernel configuration. It allows to set sizes of some
 * kernel buffers and to enable or disable debugging code, timing features, 
 * and individual kernel calls.
 *
 * Changes:
 *   Jul 11, 2005	Created.  (Jorrit N. Herder)
 */

/* In embedded and sensor applications, not all the kernel calls may be
 * needed. In this section you can specify which kernel calls are needed
 * and which are not. The code for unneeded kernel calls is not included in
 * the system binary, making it smaller. If you are not sure, it is best
 * to keep all kernel calls enabled.
 */
#define USE_FORK       	   1	/* fork a new process */
#define USE_NEWMAP     	   1	/* set a new memory map */
#define USE_EXEC       	   1	/* update process after execute */
#define USE_EXIT	   1	/* clean up after process exit */
#define USE_TRACE      	   1	/* process information and tracing */
#define USE_GETKSIG    	   1	/* retrieve pending kernel signals */
#define USE_ENDKSIG    	   1	/* finish pending kernel signals */
#define USE_KILL       	   1 	/* send a signal to a process */
#define USE_SIGSEND    	   1	/* send POSIX-style signal */
#define USE_SIGRETURN  	   1	/* sys_sigreturn(proc_nr, ctxt_ptr, flags) */
#define USE_ABORT      	   1	/* shut down MINIX */
#define USE_GETINFO    	   1 	/* retrieve a copy of kernel data */
#define USE_TIMES 	   1	/* get process and system time info */
#define USE_SETALARM	   1	/* schedule a synchronous alarm */
#define USE_DEVIO      	   1	/* read or write a single I/O port */
#define USE_VDEVIO     	   1	/* process vector with I/O requests */
#define USE_SDEVIO     	   1	/* perform I/O request on a buffer */
#define USE_IRQCTL     	   1	/* set an interrupt policy */
#define USE_SEGCTL     	   1	/* set up a remote segment */
#define USE_PRIVCTL    	   1	/* system privileges control */
#define USE_NICE 	   1	/* change scheduling priority */
#define USE_UMAP       	   1	/* map virtual to physical address */
#define USE_VIRCOPY   	   1	/* copy using virtual addressing */ 
#define USE_VIRVCOPY  	   1	/* vector with virtual copy requests */
#define USE_PHYSCOPY  	   1 	/* copy using physical addressing */
#define USE_PHYSVCOPY  	   1	/* vector with physical copy requests */
#define USE_MEMSET  	   1	/* write char to a given memory area */

/* Length of program names stored in the process table. This is only used
 * for the debugging dumps that can be generated with the IS server. The PM
 * server keeps its own copy of the program name.  
 */
#define P_NAME_LEN	   8

/* Kernel diagnostics are written to a circular buffer. After each message, 
 * a system server is notified and a copy of the buffer can be retrieved to 
 * display the message. The buffers size can safely be reduced.  
 */
#define KMESS_BUF_SIZE   256   	

/* Buffer to gather randomness. This is used to generate a random stream by 
 * the MEMORY driver when reading from /dev/random. 
 */
#define RANDOM_ELEMENTS   32

/* This section contains defines for valuable system resources that are used
 * by device drivers. The number of elements of the vectors is determined by 
 * the maximum needed by any given driver. The number of interrupt hooks may
 * be incremented on systems with many device drivers. 
 */
#define NR_IRQ_HOOKS	  16		/* number of interrupt hooks */
#define VDEVIO_BUF_SIZE   64		/* max elements per VDEVIO request */
#define VCOPY_VEC_SIZE    16		/* max elements per VCOPY request */

/* How many bytes for the kernel stack. Space allocated in mpx.s. */
#define K_STACK_BYTES   1024	

/* This section allows to enable kernel debugging and timing functionality.
 * For normal operation all options should be disabled.
 */
#define DEBUG_SCHED_CHECK  0	/* sanity check of scheduling queues */
#define DEBUG_LOCK_CHECK   0	/* kernel lock() sanity check */
#define DEBUG_TIME_LOCKS   0	/* measure time spent in locks */

#endif /* CONFIG_H */

