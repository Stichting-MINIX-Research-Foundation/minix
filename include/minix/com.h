/* This file defines constants for use in message communication (mostly)
 * between system processes.
 *
 * A number of protocol message request and response types are defined. For
 * debugging purposes, each protocol is assigned its own unique number range.
 * The following such message type ranges have been allocated:
 *
 *        1 -   0xFF	POSIX requests (see callnr.h)
 *    0x200 -  0x2FF	Data link layer requests and responses
 *    0x300 -  0x3FF	Bus controller requests and responses
 *    0x400 -  0x4FF	Block and character device requests
 *    0x500 -  0x5FF	Block and character device responses
 *    0x600 -  0x6FF	Kernel calls to SYSTEM task
 *    0x700 -  0x7FF	Reincarnation Server (RS) requests
 *    0x800 -  0x8FF	Data Store (DS) requests
 *    0x900 -  0x9FF	Requests from PM to VFS, and responses
 *   (0xA00 -  0xAFF	old TTY and LOG requests, being phased out)
 *    0xA00 -  0xAFF	Requests from VFS to file systems (see vfsif.h)
 *    0xB00 -  0xBFF	Requests from VM to VFS
 *    0xC00 -  0xCFF	Virtual Memory (VM) requests
 *    0xD00 -  0xDFF	IPC server requests
 *    0xE00 -  0xEFF	Common system messages (e.g. system signals)
 *    0xF00 -  0xFFF    Scheduling messages
 *   0x1000 - 0x10FF	Notify messages
 *
 * Zero and negative values are widely used for OK and error responses.
 */

#ifndef _MINIX_COM_H
#define _MINIX_COM_H 

/*===========================================================================*
 *          	    		Magic process numbers			     *
 *===========================================================================*/

/* These may not be any valid endpoint (see <minix/endpoint.h>). */
#define ANY	((endpoint_t) 0x7ace)	/* used to indicate 'any process' */
#define NONE 	((endpoint_t) 0x6ace)   /* used to indicate 'no process at all' */
#define SELF	((endpoint_t) 0x8ace) 	/* used to indicate 'own process' */
#define _MAX_MAGIC_PROC (SELF)	/* used by <minix/endpoint.h> 
				   to determine generation size */

/*===========================================================================*
 *            	Process numbers of processes in the system image	     *
 *===========================================================================*/

/* The values of several task numbers depend on whether they or other tasks
 * are enabled. They are defined as (PREVIOUS_TASK - ENABLE_TASK) in general.
 * ENABLE_TASK is either 0 or 1, so a task either gets a new number, or gets
 * the same number as the previous task and is further unused. Note that the
 * order should correspond to the order in the task table defined in table.c. 
 */

/* Kernel tasks. These all run in the same address space. */
#define IDLE    ((endpoint_t) -4) /* runs when no one else can run */
#define CLOCK  	((endpoint_t) -3) /* alarms and other clock functions */
#define SYSTEM  ((endpoint_t) -2) /* request system functionality */
#define KERNEL  ((endpoint_t) -1) /* pseudo-process for IPC and scheduling */
#define HARDWARE     KERNEL	/* for hardware interrupt handlers */

/* Number of tasks. Note that NR_PROCS is defined in <minix/config.h>. */
#define MAX_NR_TASKS	1023
#define NR_TASKS	  4 

/* User-space processes, that is, device drivers, servers, and INIT. */
#define PM_PROC_NR   ((endpoint_t) 0)	/* process manager */
#define VFS_PROC_NR  ((endpoint_t) 1)	/* file system */
#define RS_PROC_NR   ((endpoint_t) 2)  	/* reincarnation server */
#define MEM_PROC_NR  ((endpoint_t) 3)  	/* memory driver (RAM disk, null, etc.) */
#define LOG_PROC_NR  ((endpoint_t) 4)	/* log device driver */
#define TTY_PROC_NR  ((endpoint_t) 5)	/* terminal (TTY) driver */
#define DS_PROC_NR   ((endpoint_t) 6)   /* data store server */
#define MFS_PROC_NR  ((endpoint_t) 7)   /* minix root filesystem */
#define VM_PROC_NR   ((endpoint_t) 8)   /* memory server */
#define PFS_PROC_NR  ((endpoint_t) 9)  /* pipe filesystem */
#define SCHED_PROC_NR ((endpoint_t) 10)	/* scheduler */
#define LAST_SPECIAL_PROC_NR	11	/* An untyped version for
                                           computation in macros.*/
#define INIT_PROC_NR ((endpoint_t) LAST_SPECIAL_PROC_NR)  /* init
                                                        -- goes multiuser */

/* Root system process and root user process. */
#define ROOT_SYS_PROC_NR  RS_PROC_NR
#define ROOT_USR_PROC_NR  INIT_PROC_NR

/* Number of processes contained in the system image. */
#define NR_BOOT_PROCS 	(NR_TASKS + LAST_SPECIAL_PROC_NR + 1)

/*===========================================================================*
 *                	   Kernel notification types                         *
 *===========================================================================*/

/* Kernel notification types. In principle, these can be sent to any process,
 * so make sure that these types do not interfere with other message types.
 * Notifications are prioritized because of the way they are unhold() and
 * blocking notifications are delivered. The lowest numbers go first. The
 * offset are used for the per-process notification bit maps. 
 */
#define NOTIFY_MESSAGE		  0x1000
/* FIXME the old is_notify(a) should be replaced by is_ipc_notify(status). */
#define is_ipc_notify(ipc_status) (IPC_STATUS_CALL(ipc_status) == NOTIFY)
#define is_notify(a)		  ((unsigned) ((a) - NOTIFY_MESSAGE) < 0x100)
#define is_ipc_asynch(ipc_status) \
    (is_ipc_notify(ipc_status) || IPC_STATUS_CALL(ipc_status) == SENDA)
#define NOTIFY_FROM(p_nr)	 (NOTIFY_MESSAGE | ((p_nr) + NR_TASKS)) 

/* Shorthands for message parameters passed with notifications. */
#define NOTIFY_ARG		m2_l1
#define NOTIFY_TIMESTAMP	m2_l2

/*===========================================================================*
 *                Messages for BUS controller drivers 			     *
 *===========================================================================*/
#define BUSC_RQ_BASE	0x300	/* base for request types */
#define BUSC_RS_BASE	0x380	/* base for response types */

#define BUSC_PCI_INIT		(BUSC_RQ_BASE + 0)	/* First message to
							 * PCI driver
							 */
#define BUSC_PCI_FIRST_DEV	(BUSC_RQ_BASE + 1)	/* Get index (and
							 * vid/did) of the
							 * first PCI device
							 */
#define BUSC_PCI_NEXT_DEV	(BUSC_RQ_BASE + 2)	/* Get index (and
							 * vid/did) of the
							 * next PCI device
							 */
#define BUSC_PCI_FIND_DEV	(BUSC_RQ_BASE + 3)	/* Get index of a
							 * PCI device based on
							 * bus/dev/function
							 */
#define BUSC_PCI_IDS		(BUSC_RQ_BASE + 4)	/* Get vid/did from an
							 * index
							 */
#define BUSC_PCI_DEV_NAME	(BUSC_RQ_BASE + 5)	/* Get the name of a
							 * PCI device
							 */
#define BUSC_PCI_SLOT_NAME	(BUSC_RQ_BASE + 6)	/* Get the name of a
							 * PCI slot
							 */
#define BUSC_PCI_RESERVE	(BUSC_RQ_BASE + 7)	/* Reserve a PCI dev */
#define BUSC_PCI_ATTR_R8	(BUSC_RQ_BASE + 8)	/* Read 8-bit
							 * attribute value
							 */
#define BUSC_PCI_ATTR_R16	(BUSC_RQ_BASE + 9)	/* Read 16-bit
							 * attribute value
							 */
#define BUSC_PCI_ATTR_R32	(BUSC_RQ_BASE + 10)	/* Read 32-bit
							 * attribute value
							 */
#define BUSC_PCI_ATTR_W8	(BUSC_RQ_BASE + 11)	/* Write 8-bit
							 * attribute value
							 */
#define BUSC_PCI_ATTR_W16	(BUSC_RQ_BASE + 12)	/* Write 16-bit
							 * attribute value
							 */
#define BUSC_PCI_ATTR_W32	(BUSC_RQ_BASE + 13)	/* Write 32-bit
							 * attribute value
							 */
#define BUSC_PCI_RESCAN		(BUSC_RQ_BASE + 14)	/* Rescan bus */
#define BUSC_PCI_DEV_NAME_S	(BUSC_RQ_BASE + 15)	/* Get the name of a
							 * PCI device
							 * (safecopy)
							 */
#define BUSC_PCI_SLOT_NAME_S	(BUSC_RQ_BASE + 16)	/* Get the name of a
							 * PCI slot (safecopy)
							 */
#define BUSC_PCI_SET_ACL	(BUSC_RQ_BASE + 17)	/* Set the ACL for a
							 * driver (safecopy)
							 */
#define BUSC_PCI_DEL_ACL	(BUSC_RQ_BASE + 18)	/* Delete the ACL of a
							 * driver 
							 */
#define BUSC_PCI_GET_BAR	(BUSC_RQ_BASE + 19)	/* Get Base Address
							 * Register properties
							 */
#define   BUSC_PGB_DEVIND	m2_i1			/* device index */
#define   BUSC_PGB_PORT		m2_i2			/* port (BAR offset) */
#define   BUSC_PGB_BASE		m2_l1			/* BAR base address */
#define   BUSC_PGB_SIZE		m2_l2			/* BAR size */
#define   BUSC_PGB_IOFLAG	m2_i1			/* I/O space? */
#define IOMMU_MAP		(BUSC_RQ_BASE + 32)	/* Ask IOMMU to map
							 * a segment of memory
							 */



/*===========================================================================*
 *                Messages for BLOCK and CHARACTER device drivers	     *
 *===========================================================================*/

/* Message types for device drivers. */
#define DEV_RQ_BASE   0x400	/* base for device request types */
#define DEV_RS_BASE   0x500	/* base for device response types */

#define CANCEL       	(DEV_RQ_BASE +  0) /* force a task to cancel */
#define DEV_OPEN     	(DEV_RQ_BASE +  6) /* open a minor device */
#define DEV_CLOSE    	(DEV_RQ_BASE +  7) /* close a minor device */
#define TTY_SETPGRP 	(DEV_RQ_BASE + 10) /* set process group */
#define TTY_EXIT	(DEV_RQ_BASE + 11) /* process group leader exited */	
#define DEV_SELECT	(DEV_RQ_BASE + 12) /* request select() attention */
#define DEV_STATUS   	(DEV_RQ_BASE + 13) /* request driver status */
#define DEV_REOPEN     	(DEV_RQ_BASE + 14) /* reopen a minor device */

#define DEV_READ_S	(DEV_RQ_BASE + 20) /* (safecopy) read from minor */
#define DEV_WRITE_S   	(DEV_RQ_BASE + 21) /* (safecopy) write to minor */
#define DEV_SCATTER_S  	(DEV_RQ_BASE + 22) /* (safecopy) write from a vector */
#define DEV_GATHER_S   	(DEV_RQ_BASE + 23) /* (safecopy) read into a vector */
#define DEV_IOCTL_S    	(DEV_RQ_BASE + 24) /* (safecopy) I/O control code */
#define DEV_MMAP_S     	(DEV_RQ_BASE + 25) /* (safecopy) mmap interface */

#define IS_DEV_RQ(type) (((type) & ~0xff) == DEV_RQ_BASE)

#define DEV_REPLY       (DEV_RS_BASE + 0) /* general task reply */
#define DEV_CLONED      (DEV_RS_BASE + 1) /* return cloned minor */
#define DEV_REVIVE      (DEV_RS_BASE + 2) /* driver revives process */
#define DEV_IO_READY    (DEV_RS_BASE + 3) /* selected device ready */
#define DEV_NO_STATUS   (DEV_RS_BASE + 4) /* empty status reply */
#define DEV_REOPEN_REPL (DEV_RS_BASE + 5) /* reply to DEV_REOPEN */
#define DEV_CLOSE_REPL	(DEV_RS_BASE + 6) /* reply to DEV_CLOSE */
#define DEV_SEL_REPL1	(DEV_RS_BASE + 7) /* first reply to DEV_SELECT */
#define DEV_SEL_REPL2	(DEV_RS_BASE + 8) /* (opt) second reply to DEV_SELECT */

#define IS_DEV_RS(type) (((type) & ~0xff) == DEV_RS_BASE)

/* Field names for messages to block and character device drivers. */
#define DEVICE    	m2_i1	/* major-minor device */
#define IO_ENDPT	m2_i2	/* which (proc/endpoint) wants I/O? */
#define COUNT   	m2_i3	/* how many bytes to transfer */
#define REQUEST 	m2_i3 	/* ioctl request code */
#define POSITION	m2_l1	/* file offset (low 4 bytes) */
#define HIGHPOS		m2_l2	/* file offset (high 4 bytes) */
#define ADDRESS 	m2_p1	/* core buffer address */
#define IO_GRANT 	m2_p1	/* grant id (for DEV_*_S variants) */

/* Field names for DEV_SELECT messages to device drivers. */
#define DEV_MINOR	m2_i1	/* minor device */
#define DEV_SEL_OPS	m2_i2	/* which select operations are requested */

/* Field names used in reply messages from tasks. */
#define REP_ENDPT	m2_i1	/* # of proc on whose behalf I/O was done */
#define REP_STATUS	m2_i2	/* bytes transferred or error number */
#define REP_IO_GRANT	m2_i3	/* DEV_REVIVE: grant by which I/O was done */
#  define SUSPEND 	 -998 	/* status to suspend caller, reply later */

/* Field names for messages to TTY driver. */
#define TTY_LINE	DEVICE	/* message parameter: terminal line */
#define TTY_REQUEST	COUNT	/* message parameter: ioctl request code */
#define TTY_SPEK	POSITION/* message parameter: ioctl speed, erasing */
#define TTY_PGRP 	m2_i3	/* message parameter: process group */	

/* Field names for the QIC 02 status reply from tape driver */
#define TAPE_STAT0	m2_l1
#define TAPE_STAT1	m2_l2

/* Field names for the fstatvfs call */
#define FSTATVFS_FD m1_i1
#define FSTATVFS_BUF m1_p1

/* Field names for the statvfs call */
#define STATVFS_LEN m1_i1
#define STATVFS_NAME m1_p1
#define STATVFS_BUF m1_p2
/*===========================================================================*
 *                  	   Messages for networking layer		     *
 *===========================================================================*/

/* Message types for network layer requests. This layer acts like a driver. */
#define NW_OPEN		DEV_OPEN
#define NW_CLOSE	DEV_CLOSE
#define NW_READ		DEV_READ
#define NW_WRITE	DEV_WRITE
#define NW_IOCTL	DEV_IOCTL
#define NW_CANCEL	CANCEL

/* Base type for data link layer requests and responses. */
#define DL_RQ_BASE	0x200		
#define DL_RS_BASE	0x280		

/* Message types for data link layer requests. */
#define DL_CONF		(DL_RQ_BASE + 0)
#define DL_GETSTAT_S	(DL_RQ_BASE + 1)
#define DL_WRITEV_S	(DL_RQ_BASE + 2)
#define DL_READV_S	(DL_RQ_BASE + 3)

/* Message type for data link layer replies. */
#define DL_CONF_REPLY	(DL_RS_BASE + 0)
#define DL_STAT_REPLY	(DL_RS_BASE + 1)
#define DL_TASK_REPLY	(DL_RS_BASE + 2)

/* Field names for data link layer messages. */
#define DL_ENDPT	m2_i2
#define DL_COUNT	m2_i3
#define DL_MODE		m2_l1
#define DL_FLAGS	m2_l1
#define DL_GRANT	m2_l2
#define DL_STAT		m3_i1
#define DL_HWADDR	m3_ca1

/* Bits in 'DL_FLAGS' field of DL replies. */
#  define DL_NOFLAGS		0x00
#  define DL_PACK_SEND		0x01
#  define DL_PACK_RECV		0x02

/* Bits in 'DL_MODE' field of DL requests. */
#  define DL_NOMODE		0x0
#  define DL_PROMISC_REQ	0x1
#  define DL_MULTI_REQ		0x2
#  define DL_BROAD_REQ		0x4

/*===========================================================================*
 *                  SYSTASK request types and field names                    *
 *===========================================================================*/

/* System library calls are dispatched via a call vector, so be careful when 
 * modifying the system call numbers. The numbers here determine which call
 * is made from the call vector.
 */ 
#define KERNEL_CALL	0x600	/* base for kernel calls to SYSTEM */ 

#  define SYS_FORK       (KERNEL_CALL + 0)	/* sys_fork() */
#  define SYS_EXEC       (KERNEL_CALL + 1)	/* sys_exec() */
#  define SYS_CLEAR	 (KERNEL_CALL + 2)	/* sys_clear() */
#  define SYS_SCHEDULE 	 (KERNEL_CALL + 3)	/* sys_schedule() */
#  define SYS_PRIVCTL    (KERNEL_CALL + 4)	/* sys_privctl() */
#  define SYS_TRACE      (KERNEL_CALL + 5)	/* sys_trace() */
#  define SYS_KILL       (KERNEL_CALL + 6)	/* sys_kill() */

#  define SYS_GETKSIG    (KERNEL_CALL + 7)	/* sys_getsig() */
#  define SYS_ENDKSIG    (KERNEL_CALL + 8)	/* sys_endsig() */
#  define SYS_SIGSEND    (KERNEL_CALL + 9)	/* sys_sigsend() */
#  define SYS_SIGRETURN  (KERNEL_CALL + 10)	/* sys_sigreturn() */

#  define SYS_NEWMAP     (KERNEL_CALL + 11)	/* sys_newmap() */
#  define SYS_SEGCTL     (KERNEL_CALL + 12)	/* sys_segctl() */
#  define SYS_MEMSET     (KERNEL_CALL + 13)	/* sys_memset() */

#  define SYS_UMAP       (KERNEL_CALL + 14)	/* sys_umap() */
#  define SYS_VIRCOPY    (KERNEL_CALL + 15)	/* sys_vircopy() */
#  define SYS_PHYSCOPY   (KERNEL_CALL + 16) 	/* sys_physcopy() */

#  define SYS_IRQCTL     (KERNEL_CALL + 19)	/* sys_irqctl() */
#  define SYS_INT86      (KERNEL_CALL + 20)	/* sys_int86() */
#  define SYS_DEVIO      (KERNEL_CALL + 21)	/* sys_devio() */
#  define SYS_SDEVIO     (KERNEL_CALL + 22)	/* sys_sdevio() */
#  define SYS_VDEVIO     (KERNEL_CALL + 23)	/* sys_vdevio() */

#  define SYS_SETALARM	 (KERNEL_CALL + 24)	/* sys_setalarm() */
#  define SYS_TIMES	 (KERNEL_CALL + 25)	/* sys_times() */
#  define SYS_GETINFO    (KERNEL_CALL + 26) 	/* sys_getinfo() */
#  define SYS_ABORT      (KERNEL_CALL + 27)	/* sys_abort() */
#  define SYS_IOPENABLE  (KERNEL_CALL + 28)	/* sys_enable_iop() */
#  define SYS_SAFECOPYFROM (KERNEL_CALL + 31)	/* sys_safecopyfrom() */
#  define SYS_SAFECOPYTO   (KERNEL_CALL + 32)	/* sys_safecopyto() */
#  define SYS_VSAFECOPY  (KERNEL_CALL + 33)	/* sys_vsafecopy() */
#  define SYS_SETGRANT   (KERNEL_CALL + 34)	/* sys_setgrant() */
#  define SYS_READBIOS   (KERNEL_CALL + 35)	/* sys_readbios() */

#  define SYS_SPROF      (KERNEL_CALL + 36)     /* sys_sprof() */ 
#  define SYS_CPROF      (KERNEL_CALL + 37)     /* sys_cprof() */
#  define SYS_PROFBUF    (KERNEL_CALL + 38)     /* sys_profbuf() */

#  define SYS_STIME      (KERNEL_CALL + 39)	/* sys_stime() */

#  define SYS_VMCTL      (KERNEL_CALL + 43)	/* sys_vmctl() */
#  define SYS_SYSCTL     (KERNEL_CALL + 44)	/* sys_sysctl() */

#  define SYS_VTIMER     (KERNEL_CALL + 45)	/* sys_vtimer() */
#  define SYS_RUNCTL     (KERNEL_CALL + 46)	/* sys_runctl() */
#  define SYS_SAFEMAP	 (KERNEL_CALL + 47)	/* sys_safemap() */
#  define SYS_SAFEREVMAP (KERNEL_CALL + 48)	/* sys_saferevmap() sys_saferevmap2() */
#  define SYS_SAFEUNMAP	 (KERNEL_CALL + 49)	/* sys_safeunmap() */
#  define SYS_GETMCONTEXT (KERNEL_CALL + 50)    /* sys_getmcontext() */
#  define SYS_SETMCONTEXT (KERNEL_CALL + 51)    /* sys_setmcontext() */

#  define SYS_UPDATE	 (KERNEL_CALL + 52)	/* sys_update() */
#  define SYS_EXIT	 (KERNEL_CALL + 53)	/* sys_exit() */

#  define SYS_SCHEDCTL (KERNEL_CALL + 54)	/* sys_schedctl() */
#  define SYS_STATECTL (KERNEL_CALL + 55)	/* sys_statectl() */

/* Total */
#define NR_SYS_CALLS	56	/* number of kernel calls */

#define SYS_CALL_MASK_SIZE BITMAP_CHUNKS(NR_SYS_CALLS)

/* Basic kernel calls allowed to every system process. */
#define SYS_BASIC_CALLS \
    SYS_EXIT, SYS_SAFECOPYFROM, SYS_SAFECOPYTO, SYS_VSAFECOPY, SYS_GETINFO, \
    SYS_TIMES, SYS_SETALARM, SYS_SETGRANT, SYS_SAFEMAP, SYS_SAFEREVMAP, \
    SYS_SAFEUNMAP, SYS_PROFBUF, SYS_SYSCTL, SYS_STATECTL

/* Field names for SYS_MEMSET. */
#define MEM_PTR		m2_p1	/* base */
#define MEM_COUNT	m2_l1	/* count */
#define MEM_PATTERN	m2_l2   /* pattern to write */

/* Field names for SYS_DEVIO, SYS_VDEVIO, SYS_SDEVIO. */
#define DIO_REQUEST	m2_i3	/* device in or output */
#   define _DIO_INPUT		0x001
#   define _DIO_OUTPUT		0x002
#   define _DIO_DIRMASK		0x00f
#   define _DIO_BYTE		0x010
#   define _DIO_WORD		0x020
#   define _DIO_LONG		0x030
#   define _DIO_TYPEMASK	0x0f0
#   define _DIO_SAFE		0x100
#   define _DIO_SAFEMASK	0xf00
#   define DIO_INPUT_BYTE	    (_DIO_INPUT|_DIO_BYTE)
#   define DIO_INPUT_WORD	    (_DIO_INPUT|_DIO_WORD)
#   define DIO_INPUT_LONG	    (_DIO_INPUT|_DIO_LONG)
#   define DIO_OUTPUT_BYTE	    (_DIO_OUTPUT|_DIO_BYTE)
#   define DIO_OUTPUT_WORD	    (_DIO_OUTPUT|_DIO_WORD)
#   define DIO_OUTPUT_LONG	    (_DIO_OUTPUT|_DIO_LONG)
#   define DIO_SAFE_INPUT_BYTE      (_DIO_INPUT|_DIO_BYTE|_DIO_SAFE)
#   define DIO_SAFE_INPUT_WORD      (_DIO_INPUT|_DIO_WORD|_DIO_SAFE)
#   define DIO_SAFE_INPUT_LONG      (_DIO_INPUT|_DIO_LONG|_DIO_SAFE)
#   define DIO_SAFE_OUTPUT_BYTE     (_DIO_OUTPUT|_DIO_BYTE|_DIO_SAFE)
#   define DIO_SAFE_OUTPUT_WORD     (_DIO_OUTPUT|_DIO_WORD|_DIO_SAFE)
#   define DIO_SAFE_OUTPUT_LONG     (_DIO_OUTPUT|_DIO_LONG|_DIO_SAFE)
#define DIO_PORT	m2_l1	/* single port address */
#define DIO_VALUE	m2_l2	/* single I/O value */
#define DIO_VEC_ADDR	m2_p1   /* address of buffer or (p,v)-pairs */
#define DIO_VEC_SIZE	m2_l2   /* number of elements in vector */
#define DIO_VEC_ENDPT	m2_i2   /* number of process where vector is */
#define DIO_OFFSET	m2_i1	/* offset from grant */

/* Field names for SYS_SETALARM. */
#define ALRM_EXP_TIME   m2_l1	/* expire time for the alarm call */
#define ALRM_ABS_TIME   m2_i2	/* set to 1 to use absolute alarm time */
#define ALRM_TIME_LEFT  m2_l1	/* how many ticks were remaining */

/* Field names for SYS_IRQCTL. */
#define IRQ_REQUEST     m5_s1	/* what to do? */
#  define IRQ_SETPOLICY     1	/* manage a slot of the IRQ table */
#  define IRQ_RMPOLICY      2	/* remove a slot of the IRQ table */
#  define IRQ_ENABLE        3	/* enable interrupts */
#  define IRQ_DISABLE       4	/* disable interrupts */
#define IRQ_VECTOR	m5_s2   /* irq vector */
#define IRQ_POLICY	m5_i1   /* options for IRQCTL request */
#  define IRQ_REENABLE  0x001	/* reenable IRQ line after interrupt */
#  define IRQ_BYTE      0x100	/* byte values */      
#  define IRQ_WORD      0x200	/* word values */
#  define IRQ_LONG      0x400	/* long values */
#define IRQ_HOOK_ID	m5_l3   /* id of irq hook at kernel */

/* Field names for SYS_SEGCTL. */
#define SEG_SELECT	m4_l1   /* segment selector returned */ 
#define SEG_OFFSET	m4_l2	/* offset in segment returned */
#define SEG_PHYS	m4_l3	/* physical address of segment */
#define SEG_SIZE	m4_l4	/* segment size */
#define SEG_INDEX	m4_l5	/* segment index in remote map */

/* Field names for SYS_ABORT. */
#define ABRT_HOW	m1_i1	/* RBT_REBOOT, RBT_HALT, etc. */
#define ABRT_MON_ENDPT  m1_i2	/* process where monitor params are */
#define ABRT_MON_LEN	m1_i3	/* length of monitor params */
#define ABRT_MON_ADDR   m1_p1	/* virtual address of monitor params */

/* Field names for _UMAP, _VIRCOPY, _PHYSCOPY. */
#define CP_SRC_SPACE 	m5_s1	/* T or D space (stack is also D) */
#define CP_SRC_ENDPT	m5_i1	/* process to copy from */
#define CP_SRC_ADDR	m5_l1	/* address where data come from */
#define CP_DST_SPACE	m5_s2	/* T or D space (stack is also D) */
#define CP_DST_ENDPT	m5_i2	/* process to copy to */
#define CP_DST_ADDR	m5_l2	/* address where data go to */
#define CP_NR_BYTES	m5_l3	/* number of bytes to copy */

/* Field names for SYS_GETINFO. */
#define I_REQUEST      m7_i3	/* what info to get */
#   define GET_KINFO	   0	/* get kernel information structure */
#   define GET_IMAGE	   1	/* get system image table */
#   define GET_PROCTAB	   2	/* get kernel process table */
#   define GET_RANDOMNESS  3	/* get randomness buffer */
#   define GET_MONPARAMS   4	/* get monitor parameters */
#   define GET_KENV	   5	/* get kernel environment string */
#   define GET_IRQHOOKS	   6	/* get the IRQ table */
#   define GET_KMESSAGES   7	/* get kernel messages */
#   define GET_PRIVTAB	   8	/* get kernel privileges table */
#   define GET_KADDRESSES  9	/* get various kernel addresses */
#   define GET_SCHEDINFO  10	/* get scheduling queues */
#   define GET_PROC 	  11	/* get process slot if given process */
#   define GET_MACHINE 	  12	/* get machine information */
#   define GET_LOCKTIMING 13	/* get lock()/unlock() latency timing */
#   define GET_BIOSBUFFER 14	/* get a buffer for BIOS calls */
#   define GET_LOADINFO   15	/* get load average information */
#   define GET_IRQACTIDS  16	/* get the IRQ masks */
#   define GET_PRIV	  17	/* get privilege structure */
#   define GET_HZ	  18	/* get HZ value */
#   define GET_WHOAMI	  19	/* get own name, endpoint, and privileges */
#   define GET_RANDOMNESS_BIN 20 /* get one randomness bin */
#   define GET_IDLETSC	  21	/* get cumulative idle time stamp counter */
#   define GET_AOUTHEADER 22    /* get a.out headers from the boot image */
#define I_ENDPT        m7_i4	/* calling process (may only be SELF) */
#define I_VAL_PTR      m7_p1	/* virtual address at caller */ 
#define I_VAL_LEN      m7_i1	/* max length of value */
#define I_VAL_PTR2     m7_p2	/* second virtual address */ 
#define I_VAL_LEN2_E   m7_i2	/* second length, or proc nr */

/* GET_WHOAMI fields. */
#define GIWHO_EP	m3_i1
#define GIWHO_NAME 	m3_ca1
#define GIWHO_PRIVFLAGS	m3_i2

/* Field names for SYS_TIMES. */
#define T_ENDPT		m4_l1	/* process to request time info for */
#define T_USER_TIME	m4_l1	/* user time consumed by process */
#define T_SYSTEM_TIME	m4_l2	/* system time consumed by process */
#define T_BOOTTIME	m4_l3	/* Boottime in seconds (also for SYS_STIME) */
#define T_BOOT_TICKS	m4_l5	/* number of clock ticks since boot time */

/* Field names for SYS_TRACE, SYS_PRIVCTL, SYS_STATECTL. */
#define CTL_ENDPT      m2_i1	/* process number of the caller */
#define CTL_REQUEST    m2_i2	/* server control request */
#define CTL_ARG_PTR    m2_p1	/* pointer to argument */
#define CTL_ADDRESS    m2_l1	/* address at traced process' space */
#define CTL_DATA       m2_l2	/* data field for tracing */

/* SYS_PRIVCTL with CTL_REQUEST == SYS_PRIV_QUERY_MEM */
#define CTL_PHYSSTART  m2_l1	/* physical memory start in bytes*/
#define CTL_PHYSLEN    m2_l2	/* length in bytes */

/* Subfunctions for SYS_PRIVCTL */
#define SYS_PRIV_ALLOW		1	/* Allow process to run */
#define SYS_PRIV_DISALLOW	2	/* Disallow process to run */
#define SYS_PRIV_SET_SYS	3	/* Set a system privilege structure */
#define SYS_PRIV_SET_USER	4	/* Set a user privilege structure */
#define SYS_PRIV_ADD_IO 	5	/* Add I/O range (struct io_range) */
#define SYS_PRIV_ADD_MEM	6	/* Add memory range (struct mem_range)
					 */
#define SYS_PRIV_ADD_IRQ	7	/* Add IRQ */
#define SYS_PRIV_QUERY_MEM	8	/* Verify memory privilege. */
#define SYS_PRIV_UPDATE_SYS	9	/* Update a sys privilege structure. */
#define SYS_PRIV_YIELD	       10	/* Allow process to run and suspend */

/* Field names for SYS_SETGRANT */
#define SG_ADDR		m2_p1	/* address */
#define SG_SIZE		m2_i2	/* no. of entries */

/* Field names for SYS_GETKSIG, _ENDKSIG, _KILL, _SIGSEND, _SIGRETURN. */
#define SIG_ENDPT      m2_i1	/* process number for inform */
#define SIG_NUMBER     m2_i2	/* signal number to send */
#define SIG_FLAGS      m2_i3	/* signal flags field */
#define SIG_MAP        m2_l1	/* used by kernel to pass signal bit map */
#define SIG_CTXT_PTR   m2_p1	/* pointer to info to restore signal context */

/* Field names for SYS_FORK, _EXEC, _EXIT, _NEWMAP, GETMCONTEXT, SETMCONTEXT.*/
#define PR_ENDPT        m1_i1	/* indicates a process */
#define PR_PRIORITY     m1_i2	/* process priority */
#define PR_SLOT         m1_i2	/* indicates a process slot */
#define PR_STACK_PTR    m1_p1	/* used for stack ptr in sys_exec, sys_getsp */
#define PR_NAME_PTR     m1_p2	/* tells where program name is for dmp */
#define PR_IP_PTR       m1_p3	/* initial value for ip after exec */
#define PR_MEM_PTR      m1_p1	/* tells where memory map is for sys_newmap
				 * and sys_fork
				 */
#define PR_FORK_FLAGS	m1_i3	/* optional flags for fork operation */
#define PR_FORK_MSGADDR m1_p1	/* reply message address of forked child */
#define PR_CTX_PTR	m1_p1	/* pointer to mcontext_t structure */

/* Flags for PR_FORK_FLAGS. */
#define PFF_VMINHIBIT	0x01	/* Don't schedule until release by VM. */

/* Field names for SYS_INT86 */
#define INT86_REG86    m1_p1	/* pointer to registers */

/* Field names for SYS_SAFECOPY* */
#define SCP_FROM_TO	m2_i1	/* from/to whom? */
#define SCP_SEG		m2_i2	/* my own segment */
#define SCP_GID		m2_i3	/* grant id */
#define SCP_OFFSET	m2_l1	/* offset within grant */
#define	SCP_ADDRESS	m2_p1	/* my own address */
#define	SCP_BYTES	m2_l2	/* bytes from offset */

/* Field names for SYS_VSAFECOPY* */
#define VSCP_VEC_ADDR	m2_p1	/* start of vector */
#define VSCP_VEC_SIZE	m2_l2	/* elements in vector */

/* Field names for SYS_SAFEMAPs */
#define SMAP_EP		m2_i1
#define SMAP_GID	m2_i2
#define SMAP_OFFSET	m2_i3
#define SMAP_SEG	m2_p1
#define SMAP_ADDRESS	m2_l1
#define SMAP_BYTES	m2_l2
#define SMAP_FLAG	m2_s1

/* Field names for SYS_SPROF, _CPROF, _PROFBUF. */
#define PROF_ACTION    m7_i1    /* start/stop/reset/get */
#define PROF_MEM_SIZE  m7_i2    /* available memory for data */ 
#define PROF_FREQ      m7_i3    /* sample frequency */
#define PROF_ENDPT     m7_i4    /* endpoint of caller */
#define PROF_CTL_PTR   m7_p1    /* location of info struct */
#define PROF_MEM_PTR   m7_p2    /* location of profiling data */

/* Field names for SYS_READBIOS. */
#define RDB_SIZE	m2_i1
#define RDB_ADDR	m2_l1
#define RDB_BUF		m2_p1

/* Field names for SYS_VMCTL. */
#define SVMCTL_WHO	m1_i1
#define SVMCTL_PARAM	m1_i2	/* All SYS_VMCTL requests. */
#define SVMCTL_VALUE	m1_i3
#define	SVMCTL_MRG_TARGET	m2_i1	/* MEMREQ_GET reply: target process */
#define	SVMCTL_MRG_ADDR		m2_i2	/* MEMREQ_GET reply: address */
#define	SVMCTL_MRG_LENGTH	m2_i3	/* MEMREQ_GET reply: length */
#define	SVMCTL_MRG_FLAG		m2_s1	/* MEMREQ_GET reply: flag */
#define	SVMCTL_MRG_EP2		m2_l1	/* MEMREQ_GET reply: source process */
#define	SVMCTL_MRG_ADDR2	m2_l2	/* MEMREQ_GET reply: source address */
#define SVMCTL_MRG_REQUESTOR	m2_p1	/* MEMREQ_GET reply: requestor */
#define SVMCTL_MAP_VIR_ADDR	m1_p1
#define SVMCTL_PTROOT		m1_i3
#define SVMCTL_PTROOT_V		m1_p1

/* Reply message for VMCTL_KERN_PHYSMAP */
#define SVMCTL_MAP_FLAGS	m2_i1	/* VMMF_* */
#define SVMCTL_MAP_PHYS_ADDR	m2_l1
#define SVMCTL_MAP_PHYS_LEN	m2_l2

#define VMMF_UNCACHED		(1L << 0)

/* Values for SVMCTL_PARAM. */
#define VMCTL_CLEAR_PAGEFAULT	12
#define VMCTL_I386_GETCR3	13
#define VMCTL_MEMREQ_GET	14
#define VMCTL_MEMREQ_REPLY	15
#define VMCTL_INCSP		16
#define VMCTL_NOPAGEZERO	18
#define VMCTL_I386_KERNELLIMIT	19
#define VMCTL_I386_FREEPDE	23
#define VMCTL_ENABLE_PAGING	24
#define VMCTL_I386_INVLPG	25
#define VMCTL_FLUSHTLB		26
#define VMCTL_KERN_PHYSMAP	27
#define VMCTL_KERN_MAP_REPLY	28
#define VMCTL_SETADDRSPACE	29

/* Codes and field names for SYS_SYSCTL. */
#define SYSCTL_CODE		m1_i1	/* SYSCTL_CODE_* below */
#define SYSCTL_ARG1		m1_p1
#define SYSCTL_ARG2		m1_i2
#define SYSCTL_CODE_DIAG	1	/* Print diagnostics. */
#define SYSCTL_CODE_STACKTRACE	2	/* Print process stack. */
#define DIAG_BUFSIZE	(80*25)

/* Field names for SYS_VTIMER. */
#define VT_WHICH	m2_i1	/* which timer to set/retrieve */
#  define VT_VIRTUAL        1	/* the ITIMER_VIRTUAL timer */
#  define VT_PROF           2	/* the ITIMER_PROF timer */
#define VT_SET		m2_i2	/* 1 for setting a timer, 0 retrieval only */
#define VT_VALUE	m2_l1	/* new/previous value of the timer */
#define VT_ENDPT	m2_l2	/* process to set/retrieve the timer for */

/* Field names for SYS_RUNCTL. */
#define RC_ENDPT	m1_i1	/* which process to stop or resume */
#define RC_ACTION	m1_i2	/* set or clear stop flag */
#  define RC_STOP           0	/* stop the process */
#  define RC_RESUME         1	/* clear the stop flag */
#define RC_FLAGS	m1_i3	/* request flags */
#  define RC_DELAY          1	/* delay stop if process is sending */

/* Field names for SYS_UPDATE. */
#define SYS_UPD_SRC_ENDPT	m1_i1	/* source endpoint */
#define SYS_UPD_DST_ENDPT	m1_i2	/* destination endpoint */

/* Subfunctions for SYS_STATECTL */
#define SYS_STATE_CLEAR_IPC_REFS    1	/* clear IPC references */

/* Subfunctions for SYS_SCHEDCTL */
#define SCHEDCTL_FLAGS		m9_l1	/* flags for setting the scheduler */
#  define SCHEDCTL_FLAG_KERNEL	1	/* mark kernel scheduler and remove 
					 * RTS_NO_QUANTUM; otherwise caller is 
					 * marked scheduler 
					 */
#define SCHEDCTL_ENDPOINT	m9_l2	/* endpt of process to be scheduled */
#define SCHEDCTL_QUANTUM	m9_l3   /* current scheduling quantum */
#define SCHEDCTL_PRIORITY	m9_s4   /* current scheduling priority */

/*===========================================================================*
 *                Messages for the Reincarnation Server 		     *
 *===========================================================================*/

#define RS_RQ_BASE		0x700

#define RS_UP		(RS_RQ_BASE + 0)	/* start system service */
#define RS_DOWN		(RS_RQ_BASE + 1)	/* stop system service */
#define RS_REFRESH	(RS_RQ_BASE + 2)	/* refresh system service */
#define RS_RESTART	(RS_RQ_BASE + 3)	/* restart system service */
#define RS_SHUTDOWN	(RS_RQ_BASE + 4)	/* alert about shutdown */
#define RS_UPDATE	(RS_RQ_BASE + 5)	/* update system service */
#define RS_CLONE	(RS_RQ_BASE + 6)	/* clone system service */
#define RS_EDIT		(RS_RQ_BASE + 7)	/* edit system service */

#define RS_LOOKUP	(RS_RQ_BASE + 8)	/* lookup server name */

#define RS_INIT 	(RS_RQ_BASE + 20)	/* service init message */
#define RS_LU_PREPARE	(RS_RQ_BASE + 21)	/* prepare to update message */

#  define RS_CMD_ADDR		m1_p1		/* command string */
#  define RS_CMD_LEN		m1_i1		/* length of command */
#  define RS_PERIOD 	        m1_i2		/* heartbeat period */
#  define RS_DEV_MAJOR          m1_i3           /* major device number */

#  define RS_ENDPOINT		m1_i1		/* endpoint number in reply */

#  define RS_NAME		m1_p1		/* name */
#  define RS_NAME_LEN		m1_i1		/* namelen */

#  define RS_INIT_RESULT        m7_i1           /* init result */
#  define RS_INIT_TYPE          m7_i2           /* init type */
#  define RS_INIT_RPROCTAB_GID  m7_i3           /* init rproc table gid */
#  define RS_INIT_OLD_ENDPOINT  m7_i4           /* init old endpoint */

#  define RS_LU_RESULT          m1_i1           /* live update result */
#  define RS_LU_STATE           m1_i2           /* state required to update */
#  define RS_LU_PREPARE_MAXTIME m1_i3           /* the max time to prepare */

/*===========================================================================*
 *                Messages for the Data Store Server			     *
 *===========================================================================*/

#define DS_RQ_BASE		0x800

#define DS_PUBLISH	(DS_RQ_BASE + 0)	/* publish data */
#define DS_RETRIEVE	(DS_RQ_BASE + 1)	/* retrieve data by name */
#define DS_SUBSCRIBE	(DS_RQ_BASE + 2)	/* subscribe to data updates */
#define DS_CHECK	(DS_RQ_BASE + 3)	/* retrieve updated data */
#define DS_DELETE	(DS_RQ_BASE + 4)	/* delete data */
#define DS_SNAPSHOT	(DS_RQ_BASE + 5)	/* take a snapshot */
#define DS_RETRIEVE_LABEL  (DS_RQ_BASE + 6)	/* retrieve label's name */

/* DS field names */
#  define DS_KEY_GRANT		m2_i1		/* key for the data */
#  define DS_KEY_LEN		m2_s1		/* length of key incl. '\0' */
#  define DS_FLAGS		m2_i2		/* flags provided by caller */

#  define DS_VAL		m2_l1		/* data (u32, char *, etc.) */
#  define DS_VAL_LEN		m2_l2		/* data length */
#  define DS_NR_SNAPSHOT	m2_i3		/* number of snapshot */
#  define DS_OWNER		m2_i3		/* owner */

/*===========================================================================*
 *                Miscellaneous messages used by TTY			     *
 *===========================================================================*/

/* Miscellaneous request types and field names, e.g. used by IS server. */
#define FKEY_CONTROL 		98  	/* control a function key at the TTY */
#  define FKEY_REQUEST	     m2_i1	/* request to perform at TTY */
#  define    FKEY_MAP		10	/* observe function key */
#  define    FKEY_UNMAP		11	/* stop observing function key */
#  define    FKEY_EVENTS	12	/* request open key presses */
#  define FKEY_FKEYS	      m2_l1	/* F1-F12 keys pressed */
#  define FKEY_SFKEYS	      m2_l2	/* Shift-F1-F12 keys pressed */
#define DIAG_BASE	0xa00
#define DIAGNOSTICS_OLD 	(DIAG_BASE+1) 	/* output a string without FS in between */
#define DIAGNOSTICS_S_OLD 	(DIAG_BASE+2) 	/* grant-based version of DIAGNOSTICS */
#  define DIAG_PRINT_BUF_G    m1_p1
#  define DIAG_BUF_COUNT      m1_i1
#define GET_KMESS	(DIAG_BASE+3)	/* get kmess from TTY */
#  define GETKM_PTR	      m1_p1
#define GET_KMESS_S	(DIAG_BASE+4)	/* get kmess from TTY */
#  define GETKM_GRANT	      m1_i1
#define ASYN_DIAGNOSTICS_OLD (DIAG_BASE+5) 	/* grant-based, replyless DIAGNOSTICS */

#define DIAG_REPL_OLD 	(DIAG_BASE+0x80+0) 	/* reply to DIAGNOSTICS(_S) */

/*===========================================================================*
 *                Messages used between PM and VFS			     *
 *===========================================================================*/

#define PM_RQ_BASE	0x900
#define PM_RS_BASE	0x980

/* Requests from PM to VFS */
#define PM_INIT		(PM_RQ_BASE + 0)	/* Process table exchange */
#define PM_SETUID	(PM_RQ_BASE + 1)	/* Set new user ID */
#define PM_SETGID	(PM_RQ_BASE + 2)	/* Set group ID */
#define PM_SETSID	(PM_RQ_BASE + 3)	/* Set session leader */
#define PM_EXIT		(PM_RQ_BASE + 4)	/* Process exits */
#define PM_DUMPCORE	(PM_RQ_BASE + 5)	/* Process is to dump core */
#define PM_EXEC		(PM_RQ_BASE + 6)	/* Forwarded exec call */
#define PM_FORK		(PM_RQ_BASE + 7)	/* Newly forked process */
#define PM_SRV_FORK	(PM_RQ_BASE + 8)	/* fork for system services */
#define PM_UNPAUSE	(PM_RQ_BASE + 9)	/* Interrupt process call */
#define PM_REBOOT	(PM_RQ_BASE + 10)	/* System reboot */
#define PM_SETGROUPS	(PM_RQ_BASE + 11)	/* Tell VFS about setgroups */

/* Replies from VFS to PM */
#define PM_SETUID_REPLY	(PM_RS_BASE + 1)
#define PM_SETGID_REPLY	(PM_RS_BASE + 2)
#define PM_SETSID_REPLY	(PM_RS_BASE + 3)
#define PM_EXIT_REPLY	(PM_RS_BASE + 4)
#define PM_CORE_REPLY	(PM_RS_BASE + 5)
#define PM_EXEC_REPLY	(PM_RS_BASE + 6)
#define PM_FORK_REPLY	(PM_RS_BASE + 7)
#define PM_SRV_FORK_REPLY	(PM_RS_BASE + 8)
#define PM_UNPAUSE_REPLY	(PM_RS_BASE + 9)
#define PM_REBOOT_REPLY	(PM_RS_BASE + 10)
#define PM_SETGROUPS_REPLY	(PM_RS_BASE + 11)

/* Standard parameters for all requests and replies, except PM_REBOOT */
#  define PM_PROC		m1_i1	/* process endpoint */

/* Additional parameters for PM_INIT */
#  define PM_SLOT		m1_i2	/* process slot number */
#  define PM_PID		m1_i3	/* process pid */

/* Additional parameters for PM_SETUID and PM_SETGID */
#  define PM_EID		m1_i2	/* effective user/group id */
#  define PM_RID		m1_i3	/* real user/group id */

/* Additional parameter for PM_SETGROUPS */
#  define PM_GROUP_NO		m1_i2	/* number of groups */
#  define PM_GROUP_ADDR		m1_p1	/* struct holding group data */

/* Additional parameters for PM_EXEC */
#  define PM_PATH		m1_p1	/* executable */
#  define PM_PATH_LEN		m1_i2	/* length of path including
					 * terminating null character
					 */
#  define PM_FRAME		m1_p2	/* arguments and environment */
#  define PM_FRAME_LEN		m1_i3	/* size of frame */

/* Additional parameters for PM_EXEC_REPLY and PM_CORE_REPLY */
#  define PM_STATUS		m1_i2	/* OK or failure */
#  define PM_PC	        m1_p1	/* program counter */

/* Additional parameters for PM_FORK and PM_SRV_FORK */
#  define PM_PPROC		m1_i2	/* parent process endpoint */
#  define PM_CPID		m1_i3	/* child pid */

/* Parameters for the EXEC_NEWMEM call */
#define EXC_NM_PROC	m1_i1		/* process that needs new map */
#define EXC_NM_PTR	m1_p1		/* parameters in struct exec_newmem */
/* Results:
 * the status will be in m_type.
 * the top of the stack will be in m1_i1.
 * the following flags will be in m1_i2:
 */
#define EXC_NM_RF_LOAD_TEXT	1	/* Load text segment (otherwise the
					 * text segment is already present)
					 */
#define EXC_NM_RF_ALLOW_SETUID	2	/* Setuid execution is allowed (tells
					 * FS to update its uid and gid 
					 * fields.
					 */
#define EXC_NM_RF_FULLVM	4	

/* Parameters for the EXEC_RESTART call */
#define EXC_RS_PROC	m1_i1		/* process that needs to be restarted */
#define EXC_RS_RESULT	m1_i2		/* result of the exec */
#define EXC_RS_PC	m1_p1		/* program counter */

/*===========================================================================*
 *                Messages used from VFS to file servers		     *
 *===========================================================================*/

#define VFS_BASE	0xA00		/* Requests sent by VFS to filesystem
					 * implementations. See <minix/vfsif.h>
					 */

/*===========================================================================*
 *                Miscellaneous field names				     *
 *===========================================================================*/

/* PM field names */
/* BRK */
#define PMBRK_ADDR				m1_p1

/* TRACE */
#define PMTRACE_ADDR				m2_l1

#define PM_ENDPT				m1_i1
#define PM_PENDPT				m1_i2

#define PM_NUID					m2_i1
#define PM_NGID					m2_i2

/* Field names for GETSYSINFO_UP (PM). */
#define SIU_WHAT	m2_i1
#define SIU_LEN		m2_i2
#define SIU_WHERE	m2_p1

/* Field names for SELECT (FS). */
#define SEL_NFDS       m8_i1
#define SEL_READFDS    m8_p1
#define SEL_WRITEFDS   m8_p2
#define SEL_ERRORFDS   m8_p3
#define SEL_TIMEOUT    m8_p4

#define COMMON_RQ_BASE		0xE00

/* Field names for system signals (sent by a signal manager). */
#define SIGS_SIGNAL_RECEIVED (COMMON_RQ_BASE+0)
#	define SIGS_SIG_NUM      m2_i1

/* Common request to all processes: gcov data. */
#define COMMON_REQ_GCOV_DATA (COMMON_RQ_BASE+1)
#	define GCOV_GRANT   m1_i2
#	define GCOV_PID     m1_i3
#	define GCOV_BUFF_P  m1_p1
#	define GCOV_BUFF_SZ m1_i1

/*===========================================================================*
 *                Messages for VM server				     *
 *===========================================================================*/
#define VM_RQ_BASE		0xC00

/* Calls from PM */
#define VM_EXIT			(VM_RQ_BASE+0)
#	define VME_ENDPOINT		m1_i1
#define VM_FORK			(VM_RQ_BASE+1)
#	define VMF_ENDPOINT		m1_i1
#	define VMF_SLOTNO		m1_i2
#	define VMF_CHILD_ENDPOINT	m1_i3	/* result */
#define VM_BRK			(VM_RQ_BASE+2)
#	define VMB_ENDPOINT		m1_i1
#	define VMB_ADDR			m1_p1
#	define VMB_RETADDR		m1_p2	/* result */
#define VM_EXEC_NEWMEM		(VM_RQ_BASE+3)
#	define VMEN_ENDPOINT		m1_i1
#	define VMEN_ARGSPTR		m1_p1
#	define VMEN_ARGSSIZE		m1_i2
#	define VMEN_FLAGS		m1_i3	/* result */
#	define VMEN_STACK_TOP		m1_p2	/* result */
#define VM_PUSH_SIG		(VM_RQ_BASE+4)
#	define VMPS_ENDPOINT		m1_i1
#	define VMPS_OLD_SP		m1_p1	/* result */
#define VM_WILLEXIT		(VM_RQ_BASE+5)
#	define VMWE_ENDPOINT		m1_i1

/* General calls. */
#define VM_MMAP			(VM_RQ_BASE+10)
#	define VMM_ADDR			m5_l1
#	define VMM_LEN			m5_l2
#	define VMM_PROT			m5_s1
#	define VMM_FLAGS		m5_s2
#	define VMM_FD			m5_i1
#	define VMM_OFFSET		m5_i2
#	define VMM_RETADDR		m5_l1	/* result */
#define VM_UMAP			(VM_RQ_BASE+11)
#	define VMU_SEG			m1_i1
#	define VMU_OFFSET		m1_p1
#	define VMU_LENGTH		m1_p2
#	define VMU_RETADDR		m1_p3

/* to VM: inform VM about a region of memory that is used for
 * bus-master DMA
 */
#define VM_ADDDMA	(VM_RQ_BASE+12)
#	define VMAD_REQ			m2_i2
#	define VMAD_EP			m2_i1
#	define VMAD_START		m2_l1
#	define VMAD_SIZE		m2_l2

/* to VM: inform VM that a region of memory that is no longer
 * used for bus-master DMA
 */
#define VM_DELDMA       (VM_RQ_BASE+13)
#	define VMDD_REQ			m2_i2
#	define VMDD_EP			m2_i1
#	define VMDD_START		m2_l1
#	define VMDD_SIZE		m2_l2

/* to VM: ask VM for a region of memory that should not
 * be used for bus-master DMA any longer
 */
#define VM_GETDMA       (VM_RQ_BASE+14)
#	define VMGD_REQ			m2_i2
#	define VMGD_PROCP		m2_i1
#	define VMGD_BASEP		m2_l1
#	define VMGD_SIZEP		m2_l2

#define VM_MAP_PHYS		(VM_RQ_BASE+15)
#	define VMMP_EP			m1_i1
#	define VMMP_PHADDR		m1_p2
#	define VMMP_LEN			m1_i2
#	define VMMP_VADDR_REPLY		m1_p3

#define VM_UNMAP_PHYS		(VM_RQ_BASE+16)
#	define VMUP_EP			m1_i1
#	define VMUP_VADDR		m1_p1

#define VM_MUNMAP		(VM_RQ_BASE+17)
#	define VMUM_ADDR		m1_p1
#	define VMUM_LEN			m1_i1

#define VM_MUNMAP_TEXT		(VM_RQ_BASE+19)

/* To VM: forget all my yielded blocks. */
#define VM_FORGETBLOCKS		(VM_RQ_BASE+22)

/* To VM: forget this block. */
#define VM_FORGETBLOCK		(VM_RQ_BASE+23)
#define VMFB_IDHI			m1_i1
#define VMFB_IDLO			m1_i2

/* To VM: combined yield+get call. */
#define VM_YIELDBLOCKGETBLOCK	(VM_RQ_BASE+25)
#define VMYBGB_VADDR			m2_p1
#define VMYBGB_GETIDHI			m2_i1
#define VMYBGB_GETIDLO			m2_i2
#define VMYBGB_LEN			m2_i3
#define VMYBGB_YIELDIDHI		m2_l1
#define VMYBGB_YIELDIDLO		m2_l2

/* Calls from VFS. */
#	define VMV_ENDPOINT		m1_i1	/* for all VM_VFS_REPLY_* */
#define VM_VFS_REPLY_OPEN	(VM_RQ_BASE+30)
#	define VMVRO_FD			m1_i2
#define VM_VFS_REPLY_MMAP	(VM_RQ_BASE+31)
#define VM_VFS_REPLY_CLOSE	(VM_RQ_BASE+32)

#define VM_REMAP		(VM_RQ_BASE+33)
#	define VMRE_D			m1_i1
#	define VMRE_S			m1_i2
#	define VMRE_DA			m1_p1
#	define VMRE_SA			m1_p2
#	define VMRE_RETA		m1_p3
#	define VMRE_SIZE		m1_i3

#define VM_SHM_UNMAP		(VM_RQ_BASE+34)
#	define VMUN_ENDPT		m2_i1
#	define VMUN_ADDR		m2_l1

#define VM_GETPHYS		(VM_RQ_BASE+35)
#	define VMPHYS_ENDPT		m2_i1
#	define VMPHYS_ADDR		m2_l1
#	define VMPHYS_RETA		m2_l2

#define VM_GETREF		(VM_RQ_BASE+36)
#	define VMREFCNT_ENDPT		m2_i1
#	define VMREFCNT_ADDR		m2_l1
#	define VMREFCNT_RETC		m2_i2

#define VM_RS_SET_PRIV		(VM_RQ_BASE+37)
#	define VM_RS_NR			m2_i1
#	define VM_RS_BUF		m2_l1

#define VM_QUERY_EXIT		(VM_RQ_BASE+38)
#	define VM_QUERY_RET_PT	m2_i1
#	define VM_QUERY_IS_MORE	m2_i2

#define VM_NOTIFY_SIG		(VM_RQ_BASE+39)
#	define VM_NOTIFY_SIG_ENDPOINT	m1_i1
#	define VM_NOTIFY_SIG_IPC	m1_i2

#define VM_INFO			(VM_RQ_BASE+40)
#	define VMI_WHAT			m2_i1
#	define VMI_EP			m2_i2
#	define VMI_COUNT		m2_i3
#	define VMI_PTR			m2_p1
#	define VMI_NEXT			m2_l1

/* VMI_WHAT values. */
#define VMIW_STATS			1
#define VMIW_USAGE			2
#define VMIW_REGION			3

#define VM_RS_UPDATE		(VM_RQ_BASE+41)
#	define VM_RS_SRC_ENDPT		m1_i1
#	define VM_RS_DST_ENDPT		m1_i2

#define VM_RS_MEMCTL		(VM_RQ_BASE+42)
#	define VM_RS_CTL_ENDPT		m1_i1
#	define VM_RS_CTL_REQ		m1_i2
#		define VM_RS_MEM_PIN	    0	/* pin memory */
#		define VM_RS_MEM_MAKE_VM    1	/* make VM instance */

/* Total. */
#define NR_VM_CALLS				43
#define VM_CALL_MASK_SIZE			BITMAP_CHUNKS(NR_VM_CALLS)

/* not handled as a normal VM call, thus at the end of the reserved rage */
#define VM_PAGEFAULT		(VM_RQ_BASE+0xff)
#	define VPF_ADDR		m1_i1
#	define VPF_FLAGS	m1_i2

/* Basic vm calls allowed to every process. */
#define VM_BASIC_CALLS \
    VM_MMAP, VM_MUNMAP, VM_MUNMAP_TEXT, VM_MAP_PHYS, VM_UNMAP_PHYS, \
    VM_FORGETBLOCKS, VM_FORGETBLOCK, VM_YIELDBLOCKGETBLOCK

/*===========================================================================*
 *                Messages for IPC server				     *
 *===========================================================================*/
#define IPC_BASE	0xD00

/* Shared Memory */
#define IPC_SHMGET	(IPC_BASE+1)
#	define SHMGET_KEY	m2_l1
#	define SHMGET_SIZE	m2_l2
#	define SHMGET_FLAG	m2_i1
#	define SHMGET_RETID	m2_i2
#define IPC_SHMAT	(IPC_BASE+2)
#	define SHMAT_ID		m2_i1
#	define SHMAT_ADDR	m2_l1
#	define SHMAT_FLAG	m2_i2
#	define SHMAT_RETADDR	m2_l2
#define IPC_SHMDT	(IPC_BASE+3)
#	define SHMDT_ADDR	m2_l1
#define IPC_SHMCTL	(IPC_BASE+4)
#	define SHMCTL_ID	m2_i1
#	define SHMCTL_CMD	m2_i2
#	define SHMCTL_BUF	m2_l1
#	define SHMCTL_RET	m2_i3

/* Semaphore */
#define IPC_SEMGET	(IPC_BASE+5)
#	define SEMGET_KEY	m2_l1
#	define SEMGET_NR	m2_i1
#	define SEMGET_FLAG	m2_i2
#	define SEMGET_RETID	m2_i3
#define IPC_SEMCTL	(IPC_BASE+6)
#	define SEMCTL_ID	m2_i1
#	define SEMCTL_NUM	m2_i2
#	define SEMCTL_CMD	m2_i3
#	define SEMCTL_OPT	m2_l1
#define IPC_SEMOP	(IPC_BASE+7)
#	define SEMOP_ID		m2_i1
#	define SEMOP_OPS	m2_l1
#	define SEMOP_SIZE	m2_i2

/*===========================================================================*
 *                Messages for Scheduling				     *
 *===========================================================================*/
#define SCHEDULING_BASE	0xF00

#define SCHEDULING_NO_QUANTUM	(SCHEDULING_BASE+1)
#	define SCHEDULING_ENDPOINT	m9_l1
#	define SCHEDULING_QUANTUM	m9_l2
#	define SCHEDULING_PRIORITY	m9_s1

/* SCHEDULING_START uses _ENDPOINT, _PRIORITY and _QUANTUM from
 * SCHEDULING_NO_QUANTUM */
#define SCHEDULING_START	(SCHEDULING_BASE+2)
#	define SCHEDULING_SCHEDULER	m9_l1 /* Overrides _ENDPOINT on return*/
#	define SCHEDULING_PARENT	m9_l3
#	define SCHEDULING_MAXPRIO	m9_l4

#define SCHEDULING_STOP		(SCHEDULING_BASE+3)

#define SCHEDULING_SET_NICE	(SCHEDULING_BASE+4)

/* SCHEDULING_INHERIT is like SCHEDULING_START, but without _QUANTUM field */
#define SCHEDULING_INHERIT	(SCHEDULING_BASE+5)

#endif

/* _MINIX_COM_H */
