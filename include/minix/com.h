/*===========================================================================*
 *          	    		Magic process numbers			     *
 *===========================================================================*/

#define ANY		0x7ace	/* used to indicate 'any process' */
#define NONE 		0x6ace  /* used to indicate 'no process at all' */
#define SELF		0x8ace 	/* used to indicate 'own process' */


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
#define IDLE             -4	/* runs when no one else can run */
#define CLOCK  		 -3	/* alarms and other clock functions */
#define SYSTEM           -2	/* request system functionality */
#define KERNEL           -1	/* pseudo-process for IPC and scheduling */
#define HARDWARE     KERNEL	/* for hardware interrupt handlers */

/* Number of tasks. Note that NR_PROCS is defined in <minix/config.h>. */
#define NR_TASKS	4 

/* Magic numbers for controllers. Device driver mapping is dynamic. */
#define CTRLR(n)	(NONE + (n))

/* User-space processes, that is, device drivers, servers, and INIT. */
#define PM_PROC_NR	 0	/* process manager */
#define FS_PROC_NR 	 1	/* file system */
#define SM_PROC_NR 	 2	/* system service manager */
#define IS_PROC_NR	 3 	/* information server */
#define TTY		 4	/* terminal (TTY) driver */
#define MEMORY	 	 5  	/* memory driver (RAM disk, null, etc.) */
#define AT_WINI		(MEMORY + ENABLE_AT_WINI)	/* AT Winchester */
#define FLOPPY		(AT_WINI + ENABLE_FLOPPY)	/* floppy disk */
#define PRINTER		(FLOPPY + ENABLE_PRINTER)	/* Centronics */
#define RTL8139		(PRINTER + ENABLE_RTL8139)	/* Realtek RTL8139 */
#define FXP		(RTL8139 + ENABLE_FXP)	/* Intel Pro/100 */
#define DPETH		(FXP + ENABLE_DPETH)	/* ISA Network task */
#define LOG_PROC_NR	(DPETH + ENABLE_LOG)	/* log device */
#define INIT_PROC_NR	(LOG_PROC_NR + 1)   	/* init -- goes multiuser */

/* Number of processes contained in the system image. */
#define NR_BOOT_PROCS 	(NR_TASKS + \
			6 + ENABLE_AT_WINI + ENABLE_FLOPPY + \
			ENABLE_PRINTER + ENABLE_RTL8139 + ENABLE_FXP + \
			ENABLE_DPETH + ENABLE_LOG + 1 )	


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
#define NOTIFY_FROM(p_nr)	 (NOTIFY_MESSAGE | ((p_nr) + NR_TASKS)) 
#  define SYN_ALARM	NOTIFY_FROM(CLOCK) 	/* synchronous alarm */
#  define SYS_EVENT	NOTIFY_FROM(SYSTEM) 	/* signal system event */
#  define HARD_INT	NOTIFY_FROM(HARDWARE) 	/* hardware interrupt */
#  define NEW_KSIG	NOTIFY_FROM(HARDWARE)  	/* new kernel signal */
#  define FKEY_PRESSED	NOTIFY_FROM(TTY)  	/* function key press */

#define NOTIFICATION  		  0x800 	/* flag for notifications */
#  define DEV_SELECTED (NOTIFICATION | 5)  	/* select() notification */
#define NR_NOTIFY_TYPES    	       6	/* nr of bits in mask */

/* Shorthands for message parameters passed with notifications. */
#define NOTIFY_SOURCE		m_source
#define NOTIFY_TYPE		m_type
#define NOTIFY_ARG		m2_l1
#define NOTIFY_TIMESTAMP	m2_l2
#define NOTIFY_FLAGS		m2_i1


/*===========================================================================*
 *                Messages for system management server 		     *
 *===========================================================================*/

#define START_SERVICE	0
#define STOP_SERVICE	1


/*===========================================================================*
 *                Messages for BLOCK and CHARACTER device drivers	     *
 *===========================================================================*/

#define DEV_RQ_BASE   0x400	/* base for device request types */
#define DEV_RS_BASE   0x500	/* base for device response types */

#define CANCEL       	 0	/* general req to force a task to cancel */
#define DEV_READ	 3	/* fcn code for reading from tty */
#define DEV_WRITE   	 4	/* fcn code for writing to tty */
#define DEV_IOCTL    	 5	/* fcn code for ioctl */
#define DEV_OPEN     	 6	/* fcn code for opening tty */
#define DEV_CLOSE    	 7	/* fcn code for closing tty */
#define DEV_SCATTER  	 8	/* fcn code for writing from a vector */
#define DEV_GATHER   	 9	/* fcn code for reading into a vector */
#define TTY_SETPGRP 	10	/* fcn code for setpgroup */
#define TTY_EXIT	11	/* a process group leader has exited */	
#define DEV_SELECT	12	/* request select() attention */
#define DEV_STATUS   	(DEV_RQ_BASE + 13)	/* request driver status */

#define DEV_REPLY       (DEV_RS_BASE + 0)	/* general task reply */
#define DEV_CLONED      (DEV_RS_BASE + 1)	/* return cloned minor */
#define DEV_REVIVE      (DEV_RS_BASE + 2)	/* driver revives process */
#define DEV_IO_READY    (DEV_RS_BASE + 3)	/* selected device ready */

#define SUSPEND	 	-998	/* used in interrupts when tty has no data */

/* Field names for messages to block and character device drivers. */
#define DEVICE    	m2_i1	/* major-minor device */
#define PROC_NR		m2_i2	/* which (proc) wants I/O? */
#define COUNT   	m2_i3	/* how many bytes to transfer */
#define REQUEST 	m2_i3	/* ioctl request code */
#define POSITION	m2_l1	/* file offset */
#define ADDRESS 	m2_p1	/* core buffer address */

/* Field names for DEV_SELECT messages to device drivers. */
#define DEVICE		m2_i1	/* minor device */
#define DEV_SEL_OPS	m2_i2	/* which select operations are requested */
#define DEV_SEL_WATCH	m2_i3	/* request notify if no operations are ready */

/* Field names used in reply messages from tasks. */
#define REP_PROC_NR	m2_i1	/* # of proc on whose behalf I/O was done */
#define REP_STATUS	m2_i2	/* bytes transferred or error number */

/* Field names for messages to TTY driver. */
#define TTY_LINE	DEVICE	/* message parameter: terminal line */
#define TTY_REQUEST	COUNT	/* message parameter: ioctl request code */
#define TTY_SPEK	POSITION/* message parameter: ioctl speed, erasing */
#define TTY_FLAGS	m2_l2	/* message parameter: ioctl tty mode */
#define TTY_PGRP 	m2_i3	/* message parameter: process group */	

/* Field names for the QIC 02 status reply from tape driver */
#define TAPE_STAT0	m2_l1
#define TAPE_STAT1	m2_l2

/* Major and minor device numbers for MEMORY driver. */
#  define MEMORY_MAJOR  1	/* major device for memory devices */
#  define RAM_DEV     0		/* minor device for /dev/ram */
#  define MEM_DEV     1		/* minor device for /dev/mem */
#  define KMEM_DEV    2		/* minor device for /dev/kmem */
#  define NULL_DEV    3		/* minor device for /dev/null */
#  define BOOT_DEV    4		/* minor device for /dev/boot */
#  define RANDOM_DEV  5		/* minor device for /dev/(u)random */
#  define URANDOM_DEV RANDOM_DEV 
#  define ZERO_DEV    6		/* minor device for /dev/zero */

#define TTY_MAJOR	4	/* major device no. for ttys */
#define CTTY_MAJOR	5	/* major device no. for /dev/tty */

#define INET_MAJOR	7	/* major device no. for inet */

#define LOG_MAJOR	15	/* major device no. for LOG */
#define  IS_KLOG_DEV	0	/* minor device for /dev/klog */

/* Full device numbers that are special to the boot monitor and FS. */
#  define DEV_RAM	0x0100	/* device number of /dev/ram */
#  define DEV_BOOT	0x0104	/* device number of /dev/boot */


/*===========================================================================*
 *                  	   Messages for networking layer		     *
 *===========================================================================*/

/* Message types for network layer requests. */
#define NW_OPEN		DEV_OPEN
#define NW_CLOSE	DEV_CLOSE
#define NW_READ		DEV_READ
#define NW_WRITE	DEV_WRITE
#define NW_IOCTL	DEV_IOCTL
#define NW_CANCEL	CANCEL

/* Message types for data link layer requests. */
#define DL_WRITE	3
#define DL_WRITEV	4
#define DL_READ		5
#define DL_READV	6
#define DL_INIT		7
#define DL_STOP		8
#define DL_GETSTAT	9

/* Message type for data link layer replies. */
#define DL_INIT_REPLY	20
#define DL_TASK_REPLY	21

/* Field names for data link layer messages. */
#define DL_PORT		m2_i1
#define DL_PROC		m2_i2
#define DL_COUNT	m2_i3
#define DL_MODE		m2_l1
#define DL_CLCK		m2_l2
#define DL_ADDR		m2_p1
#define DL_STAT		m2_l1

/* Bits in 'DL_STAT' field of DL replies. */
#  define DL_PACK_SEND		0x01
#  define DL_PACK_RECV		0x02
#  define DL_READ_IP		0x04

/* Bits in 'DL_MODE' field of DL requests. */
#  define DL_NOMODE		0x0
#  define DL_PROMISC_REQ	0x2
#  define DL_MULTI_REQ		0x4
#  define DL_BROAD_REQ		0x8


/*===========================================================================*
 *                  SYSTASK request types and field names                    *
 *===========================================================================*/

/* System library calls are dispatched via a call vector, so be careful when 
 * modifying the system call numbers. The numbers here determine which call
 * is made from the call vector.
 */ 
#define KERNEL_CALL	0x600	/* base for kernel calls to SYSTEM */ 

#  define SYS_TIMES	 0	/* sys_times(proc_nr, bufptr) */
#  define SYS_EXIT	 1	/* sys_exit(parent, proc) */
#  define SYS_GETKSIG    2	/* sys_getsig(proc_nr, sig_map) */
#  define SYS_ENDKSIG    3	/* sys_endsig(proc_nr) */
#  define SYS_FORK       4	/* sys_fork(parent, child, pid) */
#  define SYS_NEWMAP     5	/* sys_newmap(proc_nr, map_ptr) */

#  define SYS_EXEC       7	/* sys_exec(proc_nr, new_sp) */
#  define SYS_SIGSEND    8	/* sys_sigsend(proc_nr, ctxt_ptr) */
#  define SYS_ABORT      9	/* sys_abort() */
#  define SYS_KILL      10	/* sys_kill(proc_nr, sig) */
#  define SYS_UMAP      11	/* sys_umap(proc_nr, etc) */

#  define SYS_TRACE     13	/* sys_trace(req,pid,addr,data) */

#  define SYS_SETALARM	16	/* sys_setalarm(proc_nr,exp_time,abs_time) */

#  define SYS_PHYSVCOPY 18	/* sys_physvcopy(vec_ptr, vec_size) */
#  define SYS_PRIVCTL   19	/* sys_privctl(proc_nr, ...) */
#  define SYS_SDEVIO    20	/* sys_sdevio(port, proc_nr, buf, count) */
#  define SYS_SIGRETURN 21	/* sys_sigreturn(proc_nr, ctxt_ptr, flags) */
#  define SYS_GETINFO   22 	/* sys_getinfo(what, whereto) */
#  define SYS_DEVIO     23	/* sys_devio(port, value) */
#  define SYS_VDEVIO    24	/* sys_vdevio(buf_ptr, nr_ports) */
#  define SYS_IRQCTL    25	/* sys_irqctl() */

#  define SYS_SEGCTL    28	/* sys_segctl(*idx, *seg, *off, phys, size) */

#  define SYS_VIRCOPY   30	/* sys_vircopy(src,seg,addr,dst,seg,addr,cnt) */
#  define SYS_PHYSCOPY  31 	/* sys_physcopy(src_addr,dst_addr,count) */
#  define SYS_VIRVCOPY  32	/* sys_virvcopy(vec_ptr, vec_size) */
#  define SYS_MEMSET    33	/* sys_memset(char, addr, count) */
#  define SYS_NICE      34	/* sys_nice(who,prio) */
#define NR_SYS_CALLS	35	/* number of system calls */ 

/* Field names for SYS_MEMSET, SYS_SEGCTL. */
#define MEM_PTR		m1_p1	/* base */
#define MEM_COUNT	m1_i1	/* count */
#define MEM_PATTERN	m1_i2   /* pattern to write */
#define MEM_CHUNK_BASE	m4_l1	/* physical base address */
#define MEM_CHUNK_SIZE	m4_l2	/* size of mem chunk */
#define MEM_TOT_SIZE	m4_l3	/* total memory size */
#define MEM_CHUNK_TAG	m4_l4	/* tag to identify chunk of mem */


/* Field names for SYS_DEVIO, SYS_VDEVIO, SYS_SDEVIO. */
#define DIO_REQUEST	m2_i3	/* device in or output */
#   define DIO_INPUT	    0	/* input */
#   define DIO_OUTPUT	    1	/* output */
#define DIO_TYPE	m2_i1   /* flag indicating byte, word, or long */ 
#   define DIO_BYTE	  'b'	/* byte type values */
#   define DIO_WORD	  'w'	/* word type values */
#   define DIO_LONG	  'l'	/* long type values */
#define DIO_PORT	m2_l1	/* single port address */
#define DIO_VALUE	m2_l2	/* single I/O value */
#define DIO_VEC_ADDR	m2_p1   /* address of buffer or (p,v)-pairs */
#define DIO_VEC_SIZE	m2_l2   /* number of elements in vector */
#define DIO_VEC_PROC	m2_i2   /* number of process where vector is */

/* Field names for SYS_SIGNARLM, SYS_FLAGARLM, SYS_SYNCALRM. */
#define ALRM_EXP_TIME   m2_l1	/* expire time for the alarm call */
#define ALRM_ABS_TIME   m2_i2	/* set to 1 to use absolute alarm time */
#define ALRM_TIME_LEFT  m2_l1	/* how many ticks were remaining */
#define ALRM_PROC_NR    m2_i1	/* which process wants the alarm? */
#define ALRM_FLAG_PTR	m2_p1   /* virtual address of timeout flag */ 	

/* Field names for SYS_IRQCTL. */
#define IRQ_REQUEST     m5_c1	/* what to do? */
#  define IRQ_SETPOLICY     1	/* manage a slot of the IRQ table */
#  define IRQ_RMPOLICY      2	/* remove a slot of the IRQ table */
#  define IRQ_ENABLE        3	/* enable interrupts */
#  define IRQ_DISABLE       4	/* disable interrupts */
#define IRQ_VECTOR	m5_c2   /* irq vector */
#define IRQ_POLICY	m5_i1   /* options for IRQCTL request */
#  define IRQ_REENABLE     0x001    /* reenable IRQ line after interrupt */
#  define IRQ_BYTE         0x100    /* byte values */      
#  define IRQ_WORD         0x200    /* word values */
#  define IRQ_LONG         0x400    /* long values */
#define IRQ_PROC_NR	m5_i2   /* process number, SELF, NONE */
#define IRQ_HOOK_ID	m5_l3   /* id of irq hook at kernel */

/* Field names for SYS_SEGCTL. */
#define SEG_SELECT	m4_l1   /* segment selector returned */ 
#define SEG_OFFSET	m4_l2	/* offset in segment returned */
#define SEG_PHYS	m4_l3	/* physical address of segment */
#define SEG_SIZE	m4_l4	/* segment size */
#define SEG_INDEX	m4_l5	/* segment index in remote map */

/* Field names for SYS_VIDCOPY. */
#define VID_REQUEST	m4_l1	/* what to do? */
#  define VID_VID_COPY	   1	/* request vid_vid_copy() */
#  define MEM_VID_COPY     2	/* request mem_vid_copy() */
#define VID_SRC_ADDR	m4_l2	/* virtual address in memory */
#define VID_SRC_OFFSET	m4_l3	/* offset in video memory */
#define VID_DST_OFFSET	m4_l4	/* offset in video memory */
#define VID_CP_COUNT	m4_l5	/* number of words to be copied */

/* Field names for SYS_ABORT. */
#define ABRT_HOW	m1_i1	/* RBT_REBOOT, RBT_HALT, etc. */
#define ABRT_MON_PROC   m1_i2	/* process where monitor params are */
#define ABRT_MON_LEN	m1_i3	/* length of monitor params */
#define ABRT_MON_ADDR   m1_p1	/* virtual address of monitor params */

/* Field names for _UMAP, _VIRCOPY, _PHYSCOPY. */
#define CP_SRC_SPACE 	m5_c1	/* T or D space (stack is also D) */
#define CP_SRC_PROC_NR	m5_i1	/* process to copy from */
#define CP_SRC_ADDR	m5_l1	/* address where data come from */
#define CP_DST_SPACE	m5_c2	/* T or D space (stack is also D) */
#define CP_DST_PROC_NR	m5_i2	/* process to copy to */
#define CP_DST_ADDR	m5_l2	/* address where data go to */
#define CP_NR_BYTES	m5_l3	/* number of bytes to copy */

/* Field names for SYS_VCOPY and SYS_VVIRCOPY. */
#define VCP_SRC_PROC	m1_i1	/* process to copy from */
#define VCP_DST_PROC	m1_i2	/* process to copy to */
#define VCP_VEC_SIZE	m1_i3	/* size of copy vector */
#define VCP_VEC_ADDR	m1_p1	/* pointer to copy vector */

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
#define I_PROC_NR      m7_i4	/* calling process */
#define I_VAL_PTR      m7_p1	/* virtual address at caller */ 
#define I_VAL_LEN      m7_i1	/* max length of value */
#define I_VAL_PTR2      m7_p2	/* second virtual address */ 
#define I_VAL_LEN2      m7_i2	/* second length, or proc nr */

/* Field names for SYS_TIMES. */
#define T_PROC_NR   	m4_l1	/* process to request time info for */
#define T_USER_TIME 	m4_l1	/* user time consumed by process */
#define T_SYSTEM_TIME	m4_l2	/* system time consumed by process */
#define T_CHILD_UTIME	m4_l3	/* user time consumed by process' children */
#define T_CHILD_STIME	m4_l4	/* sys time consumed by process' children */
#define T_BOOT_TICKS 	m4_l5	/* number of clock ticks since boot time */

/* Field names for SYS_TRACE, SYS_SVRCTL. */
#define CTL_PROC_NR	m2_i1	/* process number of the caller */
#define CTL_REQUEST	m2_i2	/* server control request */
#define CTL_MM_PRIV	m2_i3	/* privilege as seen by PM */
#define CTL_ARG_PTR	m2_p1	/* pointer to argument */
#define CTL_ADDRESS	m2_l1	/* address at traced process' space */
#define CTL_DATA	m2_l2	/* data field for tracing */

/* Field names for SYS_KILL, SYS_SIGCTL */
#define SIG_REQUEST	m2_l2	/* PM signal control request */
#define S_GETSIG 	 0	/* get pending kernel signal */
#define S_ENDSIG 	 1	/* finish a kernel signal */
#define S_SENDSIG   	 2	/* POSIX style signal handling */
#define S_SIGRETURN	 3 	/* return from POSIX handling */
#define S_KILL		 4 	/* servers kills process with signal */
#define SIG_PROC	m2_i1	/* process number for inform */
#define SIG_NUMBER	m2_i2	/* signal number to send */
#define SIG_FLAGS 	m2_i3	/* signal flags field */
#define SIG_MAP  	m2_l1	/* used by kernel to pass signal bit map */
#define SIG_CTXT_PTR	m2_p1	/* pointer to info to restore signal context */

/* Field names for SYS_FORK, _EXEC, _EXIT, _NEWMAP. */
#define PR_PROC_NR	m1_i1	/* indicates a (child) process */
#define PR_PRIORITY	m1_i2	/* process priority */
#define PR_PPROC_NR	m1_i2	/* indicates a (parent) process */
#define PR_PID		m1_i3	/* process id at process manager */
#define PR_STACK_PTR	m1_p1	/* used for stack ptr in sys_exec, sys_getsp */
#define PR_TRACING	m1_i3	/* flag to indicate tracing is on/ off */
#define PR_NAME_PTR	m1_p2	/* tells where program name is for dmp */
#define PR_IP_PTR       m1_p3	/* initial value for ip after exec */
#define PR_MEM_PTR	m1_p1	/* tells where memory map is for sys_newmap */

/* Field names for SELECT (FS). */
#define SEL_NFDS	m8_i1
#define SEL_READFDS	m8_p1
#define SEL_WRITEFDS	m8_p2
#define SEL_ERRORFDS	m8_p3
#define SEL_TIMEOUT	m8_p4

/*===========================================================================*
 *                Miscellaneous messages, mainly used by IS		     *
 *===========================================================================*/

/* Miscellaneous request types and field names, e.g. used by IS server. */
#define PANIC_DUMPS  	97  	/* debug dumps at the TTY on RBT_PANIC */
#define FKEY_CONTROL 	98  	/* control a function key at the TTY */
#  define FKEY_REQUEST		m2_i1	/* request to perform at TTY */
#  define    FKEY_MAP		10	/* observe function key */
#  define    FKEY_UNMAP		11	/* stop observing function key */
#  define    FKEY_EVENTS	12	/* request open key presses */
#  define FKEY_FKEYS		m2_l1	/* F1-F12 keys pressed */
#  define FKEY_SFKEYS		m2_l2	/* Shift-F1-F12 keys pressed */
#define DIAGNOSTICS 	100 	/* output a string without FS in between */
#  define DIAG_PRINT_BUF 	m1_p1
#  define DIAG_BUF_COUNT 	m1_i1
#  define DIAG_PROC_NR   	m1_i2


