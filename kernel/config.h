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
#define USE_CLEAR	   1	/* clean up after process exit */
#define USE_EXIT	   1	/* a system process wants to exit */
#define USE_GETKSIG    	   1	/* retrieve pending kernel signals */
#define USE_ENDKSIG    	   1	/* finish pending kernel signals */
#define USE_KILL       	   1 	/* send a signal to a process */
#define USE_SIGSEND    	   1	/* send POSIX-style signal */
#define USE_SIGRETURN  	   1	/* sys_sigreturn(proc_nr, ctxt_ptr, flags) */
#define USE_ABORT      	   1	/* shut down MINIX */
#define USE_GETINFO    	   1 	/* retrieve a copy of kernel data */
#define USE_TIMES 	   1	/* get process and system time info */
#define USE_SETALARM	   1	/* schedule a synchronous alarm */
#define USE_VTIMER         1	/* set or retrieve a process-virtual timer */
#define USE_DEVIO      	   1	/* read or write a single I/O port */
#define USE_VDEVIO     	   1	/* process vector with I/O requests */
#define USE_SDEVIO     	   1	/* perform I/O request on a buffer */
#define USE_IRQCTL     	   1	/* set an interrupt policy */
#define USE_PRIVCTL    	   1	/* system privileges control */
#define USE_UMAP       	   1	/* map virtual to physical address */
#define USE_UMAP_REMOTE	   1	/* sys_umap on behalf of another process */
#define USE_VUMAP      	   1	/* vectored virtual to physical mapping */
#define USE_VIRCOPY   	   1	/* copy using virtual addressing */ 
#define USE_PHYSCOPY  	   1 	/* copy using physical addressing */
#define USE_MEMSET  	   1	/* write char to a given memory area */
#define USE_RUNCTL         1	/* control stop flags of a process */

/* This section contains defines for valuable system resources that are used
 * by device drivers. The number of elements of the vectors is determined by 
 * the maximum needed by any given driver. The number of interrupt hooks may
 * be incremented on systems with many device drivers. 
 */
#ifndef USE_APIC
#define NR_IRQ_HOOKS	  16		/* number of interrupt hooks */
#else
#define NR_IRQ_HOOKS	  64		/* number of interrupt hooks */
#endif
#define VDEVIO_BUF_SIZE   64		/* max elements per VDEVIO request */

#define K_PARAM_SIZE     512

#endif /* CONFIG_H */

