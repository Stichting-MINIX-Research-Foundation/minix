/* This file defines constants for use in message communication (mostly)
 * between system processes.
 *
 * A number of protocol message request and response types are defined. For
 * debugging purposes, each protocol is assigned its own unique number range.
 * The following such message type ranges have been allocated:
 *
 *     0x00 -   0xFF	Process Manager (PM) requests (see callnr.h)
 *    0x100 -  0x1FF	Virtual File System (VFS) requests (see callnr.h)
 *    0x200 -  0x2FF	Data link layer requests and responses
 *    0x300 -  0x3FF	Bus controller requests and responses
 *    0x400 -  0x4FF	Character device requests and responses
 *    0x500 -  0x5FF	Block device requests and responses
 *    0x600 -  0x6FF	Kernel calls
 *    0x700 -  0x7FF	Reincarnation Server (RS) requests
 *    0x800 -  0x8FF	Data Store (DS) requests
 *    0x900 -  0x9FF	Requests from PM to VFS, and responses
 *    0xA00 -  0xAFF	Requests from VFS to file systems (see vfsif.h)
 *    0xB00 -  0xBFF	Transaction IDs from VFS to file systems (see vfsif.h)
 *    0xC00 -  0xCFF	Virtual Memory (VM) requests
 *    0xD00 -  0xDFF	IPC server requests
 *    0xE00 -  0xEFF	Common system messages (e.g. system signals)
 *    0xF00 -  0xFFF	Scheduling messages
 *   0x1000 - 0x10FF	Notify messages
 *   0x1100 - 0x11FF	USB  
 *   0x1200 - 0x12FF	Devman
 *   0x1300 - 0x13FF	TTY requests
 *   0x1400 - 0x14FF	Real Time Clock requests and responses
 *   0x1500 - 0x15FF	Input server messages
 *   0x1600 - 0x16FF	VirtualBox (VBOX) requests (see vboxif.h)
 *
 * Zero and negative values are widely used for OK and error responses.
 */

#ifndef _MINIX_COM_H
#define _MINIX_COM_H 

/*===========================================================================*
 *            	Process numbers of processes in the system image	     *
 *===========================================================================*/

/* Kernel tasks. These all run in the same address space. */
#define ASYNCM	((endpoint_t) -5) /* notifies about finished async sends */
#define IDLE    ((endpoint_t) -4) /* runs when no one else can run */
#define CLOCK  	((endpoint_t) -3) /* alarms and other clock functions */
#define SYSTEM  ((endpoint_t) -2) /* request system functionality */
#define KERNEL  ((endpoint_t) -1) /* pseudo-process for IPC and scheduling */
#define HARDWARE     KERNEL	/* for hardware interrupt handlers */

/* Number of tasks. Note that NR_PROCS is defined in <minix/config.h>. */
#define MAX_NR_TASKS	1023
#define NR_TASKS	  5 

/* User-space processes, that is, device drivers, servers, and INIT. */
#define PM_PROC_NR   ((endpoint_t) 0)	/* process manager */
#define VFS_PROC_NR  ((endpoint_t) 1)	/* file system */
#define RS_PROC_NR   ((endpoint_t) 2)  	/* reincarnation server */
#define MEM_PROC_NR  ((endpoint_t) 3)  	/* memory driver (RAM disk, null, etc.) */
#define SCHED_PROC_NR ((endpoint_t) 4)	/* scheduler */
#define TTY_PROC_NR  ((endpoint_t) 5)	/* terminal (TTY) driver */
#define DS_PROC_NR   ((endpoint_t) 6)   /* data store server */
#define MFS_PROC_NR  ((endpoint_t) 7)   /* minix root filesystem */
#define VM_PROC_NR   ((endpoint_t) 8)   /* memory server */
#define PFS_PROC_NR  ((endpoint_t) 9)  /* pipe filesystem */
#define LAST_SPECIAL_PROC_NR	10	/* An untyped version for
                                           computation in macros.*/
#define INIT_PROC_NR ((endpoint_t) LAST_SPECIAL_PROC_NR)  /* init
                                                        -- goes multiuser */
#define NR_BOOT_MODULES (INIT_PROC_NR+1)

/* Root system process and root user process. */
#define ROOT_SYS_PROC_NR  RS_PROC_NR
#define ROOT_USR_PROC_NR  INIT_PROC_NR

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
#define IOMMU_MAP		(BUSC_RQ_BASE + 32)	/* Ask IOMMU to map
							 * a segment of memory
							 */

#define BUSC_I2C_RESERVE	(BUSC_RQ_BASE + 64)	/* reserve i2c device */
#define BUSC_I2C_EXEC		(BUSC_RQ_BASE + 65)	/* perform i2c action */

/*===========================================================================*
 *                  	   Messages for networking layer		     *
 *===========================================================================*/

/* Base type for data link layer requests and responses. */
#define DL_RQ_BASE	0x200		
#define DL_RS_BASE	0x280		

#define IS_DL_RQ(type) (((type) & ~0x7f) == DL_RQ_BASE)
#define IS_DL_RS(type) (((type) & ~0x7f) == DL_RS_BASE)

/* Message types for data link layer requests. */
#define DL_CONF		(DL_RQ_BASE + 0)
#define DL_GETSTAT_S	(DL_RQ_BASE + 1)
#define DL_WRITEV_S	(DL_RQ_BASE + 2)
#define DL_READV_S	(DL_RQ_BASE + 3)

/* Message type for data link layer replies. */
#define DL_CONF_REPLY	(DL_RS_BASE + 0)
#define DL_STAT_REPLY	(DL_RS_BASE + 1)
#define DL_TASK_REPLY	(DL_RS_BASE + 2)

/* Bits in 'flags' field of DL replies. */
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

#  define SYS_MEMSET     (KERNEL_CALL + 13)	/* sys_memset() */

#  define SYS_UMAP       (KERNEL_CALL + 14)	/* sys_umap() */
#  define SYS_VIRCOPY    (KERNEL_CALL + 15)	/* sys_vircopy() */
#  define SYS_PHYSCOPY   (KERNEL_CALL + 16) 	/* sys_physcopy() */
#  define SYS_UMAP_REMOTE (KERNEL_CALL + 17)	/* sys_umap_remote() */
#  define SYS_VUMAP      (KERNEL_CALL + 18)	/* sys_vumap() */

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
#  define SYS_SETTIME    (KERNEL_CALL + 40)	/* sys_settime() */

#  define SYS_VMCTL      (KERNEL_CALL + 43)	/* sys_vmctl() */

#  define SYS_DIAGCTL    (KERNEL_CALL + 44)	/* sys_diagctl() */

#  define SYS_VTIMER     (KERNEL_CALL + 45)	/* sys_vtimer() */
#  define SYS_RUNCTL     (KERNEL_CALL + 46)	/* sys_runctl() */
#  define SYS_GETMCONTEXT (KERNEL_CALL + 50)    /* sys_getmcontext() */
#  define SYS_SETMCONTEXT (KERNEL_CALL + 51)    /* sys_setmcontext() */

#  define SYS_UPDATE	 (KERNEL_CALL + 52)	/* sys_update() */
#  define SYS_EXIT	 (KERNEL_CALL + 53)	/* sys_exit() */

#  define SYS_SCHEDCTL (KERNEL_CALL + 54)	/* sys_schedctl() */
#  define SYS_STATECTL (KERNEL_CALL + 55)	/* sys_statectl() */

#  define SYS_SAFEMEMSET (KERNEL_CALL + 56)	/* sys_safememset() */

#  define SYS_PADCONF (KERNEL_CALL + 57)	/* sys_padconf() */

/* Total */
#define NR_SYS_CALLS	58	/* number of kernel calls */

#define SYS_CALL_MASK_SIZE BITMAP_CHUNKS(NR_SYS_CALLS)

/* Basic kernel calls allowed to every system process. */
#define SYS_BASIC_CALLS \
    SYS_EXIT, SYS_SAFECOPYFROM, SYS_SAFECOPYTO, SYS_VSAFECOPY, SYS_GETINFO, \
    SYS_TIMES, SYS_SETALARM, SYS_SETGRANT, \
    SYS_PROFBUF, SYS_DIAGCTL, SYS_STATECTL, SYS_SAFEMEMSET

/* Field names for SYS_DEVIO, SYS_VDEVIO, SYS_SDEVIO. */
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

/* Field names for SYS_IRQCTL. */
#  define IRQ_SETPOLICY     1	/* manage a slot of the IRQ table */
#  define IRQ_RMPOLICY      2	/* remove a slot of the IRQ table */
#  define IRQ_ENABLE        3	/* enable interrupts */
#  define IRQ_DISABLE       4	/* disable interrupts */
#  define IRQ_REENABLE  0x001	/* reenable IRQ line after interrupt */
#  define IRQ_BYTE      0x100	/* byte values */      
#  define IRQ_WORD      0x200	/* word values */
#  define IRQ_LONG      0x400	/* long values */

#define CP_FLAG_TRY	0x01	/* do not transparently map */

/* Field names for SYS_GETINFO. */
#   define GET_KINFO	   0	/* get kernel information structure */
#   define GET_IMAGE	   1	/* get system image table */
#   define GET_PROCTAB	   2	/* get kernel process table */
#   define GET_RANDOMNESS  3	/* get randomness buffer */
#   define GET_MONPARAMS   4	/* get monitor parameters */
#   define GET_KENV	   5	/* get kernel environment string */
#   define GET_IRQHOOKS	   6	/* get the IRQ table */
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
#   define GET_CPUINFO    23    /* get information about cpus */
#   define GET_REGS	  24	/* get general process registers */
#   define GET_RUSAGE	  25	/* get resource usage */

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

/* Constants for exec. FIXME: these do not belong here. */
#define PMEF_AUXVECTORS	20
#define PMEF_EXECNAMELEN1 PATH_MAX

/* Flags for PR_FORK_FLAGS. */
#define PFF_VMINHIBIT	0x01	/* Don't schedule until release by VM. */

/* SYS_SAFEMEMSET */
#define SMS_DST		m2_i1	/* dst endpoint */
#define SMS_GID		m2_i3	/* grant id */
#define SMS_OFFSET	m2_l1	/* offset within grant */
#define SMS_BYTES	m2_l2	/* bytes from offset */
#define SMS_PATTERN	m2_i2	/* memset() pattern */

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
#define VMMF_USER		(1L << 1)
#define VMMF_WRITE		(1L << 2)
#define VMMF_GLO		(1L << 3)

/* Values for SVMCTL_PARAM. */
#define VMCTL_CLEAR_PAGEFAULT	12
#define VMCTL_GET_PDBR		13
#define VMCTL_MEMREQ_GET 	14
#define VMCTL_MEMREQ_REPLY	15
#define VMCTL_NOPAGEZERO	18
#define VMCTL_I386_KERNELLIMIT	19
#define VMCTL_I386_INVLPG	25
#define VMCTL_FLUSHTLB		26
#define VMCTL_KERN_PHYSMAP	27
#define VMCTL_KERN_MAP_REPLY	28
#define VMCTL_SETADDRSPACE	29
#define VMCTL_VMINHIBIT_SET	30
#define VMCTL_VMINHIBIT_CLEAR	31
#define VMCTL_CLEARMAPCACHE	32
#define VMCTL_BOOTINHIBIT_CLEAR	33

/* Codes and field names for SYS_DIAGCTL. */
#define DIAGCTL_CODE_DIAG	1	/* Print diagnostics. */
#define DIAGCTL_CODE_STACKTRACE	2	/* Print process stack. */
#define DIAGCTL_CODE_REGISTER	3	/* Register for diagnostic signals */
#define DIAGCTL_CODE_UNREGISTER	4	/* Unregister for diagnostic signals */
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
#  define SCHEDCTL_FLAG_KERNEL	1	/* mark kernel scheduler and remove 
					 * RTS_NO_QUANTUM; otherwise caller is 
					 * marked scheduler 
					 */

/* Field names for SYS_PADCONF */
#define PADCONF_PADCONF		m2_i1	/* pad to configure */
#define PADCONF_MASK		m2_i2	/* mask to apply */
#define PADCONF_VALUE		m2_i3	/* value to write */

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

#define RS_GETSYSINFO	(RS_RQ_BASE + 9)	/* get system information */

#define RS_INIT 	(RS_RQ_BASE + 20)	/* service init message */
#define RS_LU_PREPARE	(RS_RQ_BASE + 21)	/* prepare to update message */

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
#define DS_GETSYSINFO	(DS_RQ_BASE + 7)	/* get system information */

/*===========================================================================*
 *                Messages used between PM and VFS			     *
 *===========================================================================*/

#define VFS_PM_RQ_BASE	0x900
#define VFS_PM_RS_BASE	0x980

#define IS_VFS_PM_RQ(type) (((type) & ~0x7f) == VFS_PM_RQ_BASE)
#define IS_VFS_PM_RS(type) (((type) & ~0x7f) == VFS_PM_RS_BASE)

/* Requests from PM to VFS. */
#define VFS_PM_INIT	(VFS_PM_RQ_BASE + 0)	/* Process table exchange */
#define VFS_PM_SETUID	(VFS_PM_RQ_BASE + 1)	/* Set new user ID */
#define VFS_PM_SETGID	(VFS_PM_RQ_BASE + 2)	/* Set group ID */
#define VFS_PM_SETSID	(VFS_PM_RQ_BASE + 3)	/* Set session leader */
#define VFS_PM_EXIT	(VFS_PM_RQ_BASE + 4)	/* Process exits */
#define VFS_PM_DUMPCORE	(VFS_PM_RQ_BASE + 5)	/* Process is to dump core */
#define VFS_PM_EXEC	(VFS_PM_RQ_BASE + 6)	/* Forwarded exec call */
#define VFS_PM_FORK	(VFS_PM_RQ_BASE + 7)	/* Newly forked process */
#define VFS_PM_SRV_FORK	(VFS_PM_RQ_BASE + 8)	/* fork for system services */
#define VFS_PM_UNPAUSE	(VFS_PM_RQ_BASE + 9)	/* Interrupt process call */
#define VFS_PM_REBOOT	(VFS_PM_RQ_BASE + 10)	/* System reboot */
#define VFS_PM_SETGROUPS	(VFS_PM_RQ_BASE + 11)	/* Set groups */

/* Replies from VFS to PM */
#define VFS_PM_SETUID_REPLY	(VFS_PM_RS_BASE + 1)
#define VFS_PM_SETGID_REPLY	(VFS_PM_RS_BASE + 2)
#define VFS_PM_SETSID_REPLY	(VFS_PM_RS_BASE + 3)
#define VFS_PM_EXIT_REPLY	(VFS_PM_RS_BASE + 4)
#define VFS_PM_CORE_REPLY	(VFS_PM_RS_BASE + 5)
#define VFS_PM_EXEC_REPLY	(VFS_PM_RS_BASE + 6)
#define VFS_PM_FORK_REPLY	(VFS_PM_RS_BASE + 7)
#define VFS_PM_SRV_FORK_REPLY	(VFS_PM_RS_BASE + 8)
#define VFS_PM_UNPAUSE_REPLY	(VFS_PM_RS_BASE + 9)
#define VFS_PM_REBOOT_REPLY	(VFS_PM_RS_BASE + 10)
#define VFS_PM_SETGROUPS_REPLY	(VFS_PM_RS_BASE + 11)

/* Standard parameters for all requests and replies, except PM_REBOOT */
#  define VFS_PM_ENDPT		m7_i1	/* process endpoint */

/* Additional parameters for PM_INIT */
#  define VFS_PM_SLOT		m7_i2	/* process slot number */
#  define VFS_PM_PID		m7_i3	/* process pid */

/* Additional parameters for PM_SETUID and PM_SETGID */
#  define VFS_PM_EID		m7_i2	/* effective user/group id */
#  define VFS_PM_RID		m7_i3	/* real user/group id */

/* Additional parameter for PM_SETGROUPS */
#  define VFS_PM_GROUP_NO	m7_i2	/* number of groups */
#  define VFS_PM_GROUP_ADDR	m7_p1	/* struct holding group data */

/* Additional parameters for PM_EXEC */
#  define VFS_PM_PATH		m7_p1	/* executable */
#  define VFS_PM_PATH_LEN	m7_i2	/* length of path including
					 * terminating null character
					 */
#  define VFS_PM_FRAME		m7_p2	/* arguments and environment */
#  define VFS_PM_FRAME_LEN	m7_i3	/* size of frame */
#  define VFS_PM_PS_STR		m7_i5	/* ps_strings pointer */

/* Additional parameters for PM_EXEC_REPLY and PM_CORE_REPLY */
#  define VFS_PM_STATUS		m7_i2	/* OK or failure */
#  define VFS_PM_PC		m7_p1	/* program counter */
#  define VFS_PM_NEWSP		m7_p2	/* possibly-changed stack ptr */
#  define VFS_PM_NEWPS_STR	m7_i5	/* possibly-changed ps_strings ptr */

/* Additional parameters for PM_FORK and PM_SRV_FORK */
#  define VFS_PM_PENDPT		m7_i2	/* parent process endpoint */
#  define VFS_PM_CPID		m7_i3	/* child pid */
#  define VFS_PM_REUID		m7_i4	/* real and effective uid */
#  define VFS_PM_REGID		m7_i5	/* real and effective gid */

/* Additional parameters for PM_DUMPCORE */
#  define VFS_PM_TERM_SIG	m7_i2	/* process's termination signal */

/*===========================================================================*
 *                Messages used from VFS to file servers		     *
 *===========================================================================*/

#define FS_BASE		0xA00		/* Requests sent by VFS to filesystem
					 * implementations. See <minix/vfsif.h>
					 */

/*===========================================================================*
 *                Common requests and miscellaneous field names		     *
 *===========================================================================*/

#define COMMON_RQ_BASE		0xE00

/* Field names for system signals (sent by a signal manager). */
#define SIGS_SIGNAL_RECEIVED (COMMON_RQ_BASE+0)

/* Common request to all processes: gcov data. */
#define COMMON_REQ_GCOV_DATA (COMMON_RQ_BASE+1)

/* Common fault injection ctl request to all processes. */
#define COMMON_REQ_FI_CTL (COMMON_RQ_BASE+2)

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
#define VM_EXEC_NEWMEM		(VM_RQ_BASE+3)
#	define VMEN_ENDPOINT		m1_i1
#	define VMEN_ARGSPTR		m1_p1
#	define VMEN_ARGSSIZE		m1_i2
#	define VMEN_FLAGS		m1_i3	/* result */
#	define VMEN_STACK_TOP		m1_p2	/* result */
#define VM_WILLEXIT		(VM_RQ_BASE+5)
#	define VMWE_ENDPOINT		m1_i1

/* General calls. */
#define VM_MMAP			(VM_RQ_BASE+10)

#define VM_MUNMAP		(VM_RQ_BASE+17)
#	define VMUM_ADDR		m_mmap.addr
#	define VMUM_LEN			m_mmap.len

/* to VM: inform VM about a region of memory that is used for
 * bus-master DMA
 */
#define VM_ADDDMA	(VM_RQ_BASE+12)
#	define VMAD_EP			m2_i1
#	define VMAD_START		m2_l1
#	define VMAD_SIZE		m2_l2

/* to VM: inform VM that a region of memory that is no longer
 * used for bus-master DMA
 */
#define VM_DELDMA       (VM_RQ_BASE+13)
#	define VMDD_EP			m2_i1
#	define VMDD_START		m2_l1
#	define VMDD_SIZE		m2_l2

/* to VM: ask VM for a region of memory that should not
 * be used for bus-master DMA any longer
 */
#define VM_GETDMA       (VM_RQ_BASE+14)
#	define VMGD_PROCP		m2_i1
#	define VMGD_BASEP		m2_l1
#	define VMGD_SIZEP		m2_l2

#define VM_MAP_PHYS		(VM_RQ_BASE+15)

#define VM_UNMAP_PHYS		(VM_RQ_BASE+16)

/* To VM: map in cache block by FS */
#define VM_MAPCACHEPAGE		(VM_RQ_BASE+26)

/* To VM: identify cache block in FS */
#define VM_SETCACHEPAGE		(VM_RQ_BASE+27)

/* To VM: clear all cache blocks for a device */
#define VM_CLEARCACHE		(VM_RQ_BASE+28)

/* To VFS: fields for request from VM. */
#	define VFS_VMCALL_REQ		m10_i1
#	define VFS_VMCALL_FD		m10_i2
#	define VFS_VMCALL_REQID		m10_i3
#	define VFS_VMCALL_ENDPOINT	m10_i4
#	define VFS_VMCALL_OFFSET	m10_ull1
#	define VFS_VMCALL_LENGTH	m10_l3

/* Request codes to from VM to VFS */
#define VMVFSREQ_FDLOOKUP		101
#define VMVFSREQ_FDCLOSE		102
#define VMVFSREQ_FDIO			103

/* Calls from VFS. */
#define VM_VFS_REPLY		(VM_RQ_BASE+30)
#	define VMV_ENDPOINT		m10_i1
#	define VMV_RESULT		m10_i2
#	define VMV_REQID		m10_i3
#	define VMV_DEV			m10_i4
#	define VMV_INO			m10_l1
#	define VMV_FD			m10_l2
#	define VMV_SIZE_PAGES		m10_l3

#define VM_REMAP		(VM_RQ_BASE+33)

#define VM_SHM_UNMAP		(VM_RQ_BASE+34)

#define VM_GETPHYS		(VM_RQ_BASE+35)

#define VM_GETREF		(VM_RQ_BASE+36)

#define VM_RS_SET_PRIV		(VM_RQ_BASE+37)
#	define VM_RS_NR			m2_i1
#	define VM_RS_BUF		m2_l1
#	define VM_RS_SYS		m2_i2

#define VM_QUERY_EXIT		(VM_RQ_BASE+38)

#define VM_NOTIFY_SIG		(VM_RQ_BASE+39)
#	define VM_NOTIFY_SIG_ENDPOINT	m1_i1
#	define VM_NOTIFY_SIG_IPC	m1_i2

#define VM_INFO			(VM_RQ_BASE+40)

/* VM_INFO 'what' values. */
#define VMIW_STATS			1
#define VMIW_USAGE			2
#define VMIW_REGION			3

#define VM_RS_UPDATE		(VM_RQ_BASE+41)

#define VM_RS_MEMCTL		(VM_RQ_BASE+42)
#	define VM_RS_CTL_ENDPT		m1_i1
#	define VM_RS_CTL_REQ		m1_i2
#		define VM_RS_MEM_PIN	    0	/* pin memory */
#		define VM_RS_MEM_MAKE_VM    1	/* make VM instance */

#define VM_WATCH_EXIT		(VM_RQ_BASE+43)

#define VM_REMAP_RO		(VM_RQ_BASE+44)
/* same args as VM_REMAP */

#define VM_PROCCTL		(VM_RQ_BASE+45)
#define VMPCTL_PARAM		m9_l1
#define VMPCTL_WHO		m9_l2
#define VMPCTL_M1		m9_l3
#define VMPCTL_LEN		m9_l4
#define VMPCTL_FLAGS		m9_l5

#define VMPPARAM_CLEAR		1	/* values for VMPCTL_PARAM */
#define VMPPARAM_HANDLEMEM	2

#define VM_VFS_MMAP             (VM_RQ_BASE+46)

#define VM_GETRUSAGE		(VM_RQ_BASE+47)

/* Total. */
#define NR_VM_CALLS				48
#define VM_CALL_MASK_SIZE			BITMAP_CHUNKS(NR_VM_CALLS)

/* not handled as a normal VM call, thus at the end of the reserved rage */
#define VM_PAGEFAULT		(VM_RQ_BASE+0xff)
#	define VPF_ADDR		m1_i1
#	define VPF_FLAGS	m1_i2

/* Basic vm calls allowed to every process. */
#define VM_BASIC_CALLS \
    VM_BRK, VM_MMAP, VM_MUNMAP, VM_MAP_PHYS, VM_UNMAP_PHYS, VM_INFO, \
    VM_GETRUSAGE

/*===========================================================================*
 *                Messages for IPC server				     *
 *===========================================================================*/
#define IPC_BASE	0xD00

/* Shared Memory */
#define IPC_SHMGET	(IPC_BASE+1)
#define IPC_SHMAT	(IPC_BASE+2)
#define IPC_SHMDT	(IPC_BASE+3)
#define IPC_SHMCTL	(IPC_BASE+4)

/* Semaphore */
#define IPC_SEMGET	(IPC_BASE+5)
#define IPC_SEMCTL	(IPC_BASE+6)
#define IPC_SEMOP	(IPC_BASE+7)

/*===========================================================================*
 *                Messages for Scheduling				     *
 *===========================================================================*/
#define SCHEDULING_BASE	0xF00

#define SCHEDULING_NO_QUANTUM	(SCHEDULING_BASE+1)
#define SCHEDULING_START	(SCHEDULING_BASE+2)
#define SCHEDULING_STOP		(SCHEDULING_BASE+3)
#define SCHEDULING_SET_NICE	(SCHEDULING_BASE+4)
#define SCHEDULING_INHERIT	(SCHEDULING_BASE+5)

/*===========================================================================*
 *              Messages for USB                                             *
 *===========================================================================*/

#define USB_BASE 0x1100

/* those are from driver to USBD */
#define USB_RQ_INIT          (USB_BASE +  0) /* First message to HCD driver */
#define USB_RQ_DEINIT        (USB_BASE +  1) /* Quit the session */
#define USB_RQ_SEND_URB      (USB_BASE +  2) /* Send URB */
#define USB_RQ_CANCEL_URB    (USB_BASE +  3) /* Cancel URB */
#define USB_RQ_SEND_INFO     (USB_BASE +  4) /* Sends various information */
#define USB_REPLY            (USB_BASE +  5)


/* those are from USBD to driver */
#define USB_COMPLETE_URB    (USB_BASE +  6)
#define USB_ANNOUCE_DEV     (USB_BASE +  7) /* Announce a new USB Device */
#define USB_WITHDRAW_DEV    (USB_BASE +  8) /* Withdraw a allready anncounced
                                              USB device*/
#   define USB_GRANT_ID     m4_l1
#   define USB_GRANT_SIZE   m4_l2

#   define USB_URB_ID       m4_l1
#   define USB_RESULT       m4_l2
#   define USB_DEV_ID       m4_l1
#   define USB_DRIVER_EP    m4_l2
#   define USB_INTERFACES   m4_l3
#   define USB_RB_INIT_NAME m3_ca1

#   define USB_INFO_TYPE    m4_l1
#   define USB_INFO_VALUE   m4_l2

/*===========================================================================*
 *              Messages for DeviceManager (s/t like SysFS)                  *
 *===========================================================================*/

#define DEVMAN_BASE 0x1200

#define DEVMAN_ADD_DEV     (DEVMAN_BASE + 0)
#define DEVMAN_DEL_DEV     (DEVMAN_BASE + 1)
#define DEVMAN_ADD_BUS     (DEVMAN_BASE + 2)
#define DEVMAN_DEL_BUS     (DEVMAN_BASE + 3)
#define DEVMAN_ADD_DEVFILE (DEVMAN_BASE + 4)
#define DEVMAN_DEL_DEVFILE (DEVMAN_BASE + 5)

#define DEVMAN_REQUEST     (DEVMAN_BASE + 6)
#define DEVMAN_REPLY       (DEVMAN_BASE + 7)

#define DEVMAN_BIND        (DEVMAN_BASE + 8)
#define DEVMAN_UNBIND      (DEVMAN_BASE + 9)

#   define DEVMAN_GRANT_ID       m4_l1
#   define DEVMAN_GRANT_SIZE     m4_l2

#   define DEVMAN_ENDPOINT       m4_l3
#   define DEVMAN_DEVICE_ID      m4_l2
#   define DEVMAN_RESULT         m4_l1

/*===========================================================================*
 *			Messages for TTY				     *
 *===========================================================================*/

#define TTY_RQ_BASE 0x1300

#define TTY_FKEY_CONTROL	(TTY_RQ_BASE + 1) /* control an F-key at TTY */
#  define    FKEY_MAP		10	/* observe function key */
#  define    FKEY_UNMAP		11	/* stop observing function key */
#  define    FKEY_EVENTS	12	/* request open key presses */

#define TTY_INPUT_UP		(TTY_RQ_BASE + 2) /* input server is up */
#define TTY_INPUT_EVENT		(TTY_RQ_BASE + 3) /* relayed input event */

/*===========================================================================*
 *			Messages for input server and drivers		     *
 *===========================================================================*/

/* The input protocol has no real replies. All messages are one-way. */
#define INPUT_RQ_BASE 0x1500	/* from TTY to server, or server to driver */
#define INPUT_RS_BASE 0x1580	/* from input driver to input server */

#define INPUT_CONF		(INPUT_RQ_BASE + 0)	/* configure driver */
#define INPUT_SETLEDS		(INPUT_RQ_BASE + 1)	/* set keyboard LEDs */

#define INPUT_EVENT		(INPUT_RS_BASE + 0)	/* send input event */

/*===========================================================================*
 *			VFS-FS TRANSACTION IDs				     *
 *===========================================================================*/

#define VFS_TRANSACTION_BASE 0xB00

#define VFS_TRANSID	(VFS_TRANSACTION_BASE + 1)
#define IS_VFS_FS_TRANSID(type) (((type) & ~0xff) == VFS_TRANSACTION_BASE)

/*===========================================================================*
 *			Messages for character devices			     *
 *===========================================================================*/

/* Base type for character device requests and responses. */
#define CDEV_RQ_BASE	0x400
#define CDEV_RS_BASE	0x480

#define IS_CDEV_RQ(type) (((type) & ~0x7f) == CDEV_RQ_BASE)
#define IS_CDEV_RS(type) (((type) & ~0x7f) == CDEV_RS_BASE)

/* Message types for character device requests. */
#define CDEV_OPEN	(CDEV_RQ_BASE + 0)	/* open a minor device */
#define CDEV_CLOSE	(CDEV_RQ_BASE + 1)	/* close a minor device */
#define CDEV_READ	(CDEV_RQ_BASE + 2)	/* read into a buffer */
#define CDEV_WRITE	(CDEV_RQ_BASE + 3)	/* write from a buffer */
#define CDEV_IOCTL	(CDEV_RQ_BASE + 4)	/* I/O control operation */
#define CDEV_CANCEL	(CDEV_RQ_BASE + 5)	/* cancel suspended request */
#define CDEV_SELECT	(CDEV_RQ_BASE + 6)	/* test for ready operations */

/* Message types for character device responses. */
#define CDEV_REPLY	(CDEV_RS_BASE + 0)	/* general reply code */
#define CDEV_SEL1_REPLY	(CDEV_RS_BASE + 1)	/* immediate select reply */
#define CDEV_SEL2_REPLY	(CDEV_RS_BASE + 2)	/* select notification reply */

/* Bits in 'CDEV_ACCESS' field of block device open requests. */
#  define CDEV_R_BIT		0x01	/* open with read access */
#  define CDEV_W_BIT		0x02	/* open with write access */
#  define CDEV_NOCTTY		0x04	/* not to become the controlling TTY */

/* Bits in 'CDEV_FLAGS' field of block device transfer requests. */
#  define CDEV_NOFLAGS		0x00	/* no flags are set */
#  define CDEV_NONBLOCK		0x01	/* do not suspend I/O request */

/* Bits in 'CDEV_OPS', 'CDEV_STATUS' fields of block device select messages. */
#  define CDEV_OP_RD		0x01	/* selected for read operation */
#  define CDEV_OP_WR		0x02	/* selected for write operation */
#  define CDEV_OP_ERR		0x04	/* selected for error operation */
#  define CDEV_NOTIFY		0x08	/* notification requested */

/* Bits in 'CDEV_STATUS' field of block device open responses. */
#  define CDEV_CLONED		0x20000000	/* device is cloned */
#  define CDEV_CTTY		0x40000000	/* device is controlling TTY */

/*===========================================================================*
 *			Messages for block devices			     *
 *===========================================================================*/

/* Base type for block device requests and responses. */
#define BDEV_RQ_BASE	0x500
#define BDEV_RS_BASE	0x580

#define IS_BDEV_RQ(type) (((type) & ~0x7f) == BDEV_RQ_BASE)
#define IS_BDEV_RS(type) (((type) & ~0x7f) == BDEV_RS_BASE)

/* Message types for block device requests. */
#define BDEV_OPEN	(BDEV_RQ_BASE + 0)	/* open a minor device */
#define BDEV_CLOSE	(BDEV_RQ_BASE + 1)	/* close a minor device */
#define BDEV_READ	(BDEV_RQ_BASE + 2)	/* read into a buffer */
#define BDEV_WRITE	(BDEV_RQ_BASE + 3)	/* write from a buffer */
#define BDEV_GATHER	(BDEV_RQ_BASE + 4)	/* read into a vector */
#define BDEV_SCATTER	(BDEV_RQ_BASE + 5)	/* write from a vector */
#define BDEV_IOCTL	(BDEV_RQ_BASE + 6)	/* I/O control operation */

/* Message types for block device responses. */
#define BDEV_REPLY	(BDEV_RS_BASE + 0)	/* general reply code */

/* Bits in 'BDEV_ACCESS' field of block device open requests. */
#  define BDEV_R_BIT		0x01	/* open with read access */
#  define BDEV_W_BIT		0x02	/* open with write access */

/* Bits in 'BDEV_FLAGS' field of block device transfer requests. */
#  define BDEV_NOFLAGS		0x00	/* no flags are set */
#  define BDEV_FORCEWRITE	0x01	/* force write to disk immediately */
#  define BDEV_NOPAGE		0x02	/* eeprom: don't send page address */

/*===========================================================================*
 *			Messages for Real Time Clocks			     *
 *===========================================================================*/

/* Base type for real time clock requests and responses. */
#define RTCDEV_RQ_BASE	0x1400
#define RTCDEV_RS_BASE	0x1480

#define IS_RTCDEV_RQ(type) (((type) & ~0x7f) == RTCDEV_RQ_BASE)
#define IS_RTCDEV_RS(type) (((type) & ~0x7f) == RTCDEV_RS_BASE)

/* Message types for real time clock requests. */
#define RTCDEV_GET_TIME	(RTCDEV_RQ_BASE + 0)	/* get time from hw clock */
#define RTCDEV_SET_TIME	(RTCDEV_RQ_BASE + 1)	/* set time in hw clock */
#define RTCDEV_PWR_OFF	(RTCDEV_RQ_BASE + 2)	/* set time to cut the power */

/* Same as GET/SET above but using grants */
#define RTCDEV_GET_TIME_G (RTCDEV_RQ_BASE + 3)	/* get time from hw clock */
#define RTCDEV_SET_TIME_G (RTCDEV_RQ_BASE + 4)	/* set time in hw clock */

/* Message types for real time clock responses. */
#define RTCDEV_REPLY	(RTCDEV_RS_BASE + 0)	/* general reply code */

/* Bits in 'lc_readclock_rtcdev.flags' field of real time clock requests. */
#define RTCDEV_NOFLAGS	0x00	/* no flags are set */
#define RTCDEV_Y2KBUG	0x01	/* Interpret 1980 as 2000 for RTC w/Y2K bug */
#define RTCDEV_CMOSREG	0x02	/* Also set the CMOS clock register bits. */

/*===========================================================================*
 *		Internal codes used by several services			     *
 *===========================================================================*/

#define SUSPEND 	 -998 	/* status to suspend caller, reply later */

#endif /* !_MINIX_COM_H */
