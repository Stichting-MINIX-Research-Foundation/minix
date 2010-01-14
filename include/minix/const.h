
#ifndef CHIP
#error CHIP is not defined
#endif

#define EXTERN        extern	/* used in *.h files */
#define PRIVATE       static	/* PRIVATE x limits the scope of x */
#define PUBLIC			/* PUBLIC is the opposite of PRIVATE */
#define FORWARD       static	/* some compilers require this to be 'static'*/

#define TRUE               1	/* used for turning integers into Booleans */
#define FALSE              0	/* used for turning integers into Booleans */

#define DEFAULT_HZ        60	/* clock freq (software settable on IBM-PC) */

#define SUPER_USER (uid_t) 0	/* uid_t of superuser */

#define NULL     ((void *)0)	/* null pointer */
#define SCPVEC_NR	  64	/* max # of entries in a SYS_VSAFECOPY* request */
#define NR_IOREQS	  64
				/* maximum number of entries in an iorequest */

/* Message passing constants. */
#define MESS_SIZE (sizeof(message))	/* might need usizeof from FS here */
#define NIL_MESS ((message *) 0)	/* null pointer */

/* Memory related constants. */
#define SEGMENT_TYPE  0xFF00	/* bit mask to get segment type */
#define SEGMENT_INDEX 0x00FF	/* bit mask to get segment index */

#define LOCAL_SEG     0x0000	/* flags indicating local memory segment */
#define NR_LOCAL_SEGS      3	/* # local segments per process (fixed) */
#define T                  0	/* proc[i].mem_map[T] is for text */
#define D                  1	/* proc[i].mem_map[D] is for data */
#define S                  2	/* proc[i].mem_map[S] is for stack */

#define REMOTE_SEG    0x0100	/* flags indicating remote memory segment */
#define NR_REMOTE_SEGS     3    /* # remote memory regions (variable) */

#define BIOS_SEG      0x0200	/* flags indicating BIOS memory segment */
#define NR_BIOS_SEGS       3    /* # BIOS memory regions (variable) */

#define PHYS_SEG      0x0400	/* flag indicating entire physical memory */

#define LOCAL_VM_SEG  0x1000	/* same as LOCAL_SEG, but with vm lookup */
#define VM_D		(LOCAL_VM_SEG | D)
#define VM_T		(LOCAL_VM_SEG | T)
#define MEM_GRANT	3
#define VM_GRANT	(LOCAL_VM_SEG | MEM_GRANT)

/* Labels used to disable code sections for different reasons. */
#define DEAD_CODE	   0	/* unused code in normal configuration */
#define FUTURE_CODE	   0	/* new code to be activated + tested later */
#define TEMP_CODE	   1	/* active code to be removed later */

/* Process name length in the PM process table, including '\0'. */
#define PROC_NAME_LEN	16

/* Miscellaneous */
#define BYTE            0377	/* mask for 8 bits */
#define READING            0	/* copy data to user */
#define WRITING            1	/* copy data from user */
#define NO_NUM        0x8000	/* used as numerical argument to panic() */
#define NIL_PTR   (char *) 0	/* generally useful expression */
#define HAVE_SCATTERED_IO  1	/* scattered I/O is now standard */

/* Macros. */
#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#define MIN(a, b)   ((a) < (b) ? (a) : (b))

/* Memory is allocated in clicks. */
#if (CHIP == INTEL)
#define CLICK_SIZE      4096	/* unit in which memory is allocated */
#define CLICK_SHIFT       12	/* log2 of CLICK_SIZE */
#endif

#if (CHIP == SPARC) || (CHIP == M68000)
#define CLICK_SIZE	4096	/* unit in which memory is allocated */
#define CLICK_SHIFT	  12	/* log2 of CLICK_SIZE */
#endif

/* Click alignment macros. */
#define CLICK_FLOOR(n)  (((vir_bytes)(n) / CLICK_SIZE) * CLICK_SIZE)
#define CLICK_CEIL(n)   CLICK_FLOOR((vir_bytes)(n) + CLICK_SIZE-1)

/* Sizes of memory tables. The boot monitor distinguishes three memory areas,
 * namely low mem below 1M, 1M-16M, and mem after 16M. More chunks are needed
 * for DOS MINIX.
*/
#define NR_MEMS            8 


/* Click to byte conversions (and vice versa). */
#define HCLICK_SHIFT       4	/* log2 of HCLICK_SIZE */
#define HCLICK_SIZE       16	/* hardware segment conversion magic */
#if CLICK_SIZE >= HCLICK_SIZE
#define click_to_hclick(n) ((n) << (CLICK_SHIFT - HCLICK_SHIFT))
#else
#define click_to_hclick(n) ((n) >> (HCLICK_SHIFT - CLICK_SHIFT))
#endif
#define hclick_to_physb(n) ((phys_bytes) (n) << HCLICK_SHIFT)
#define physb_to_hclick(n) ((n) >> HCLICK_SHIFT)

#define ABS             -999	/* this process means absolute memory */

/* Flag bits for i_mode in the inode. */
#define I_TYPE          0170000	/* this field gives inode type */
#define I_SYMBOLIC_LINK 0120000	/* file is a symbolic link */
#define I_REGULAR       0100000	/* regular file, not dir or special */
#define I_BLOCK_SPECIAL 0060000	/* block special file */
#define I_DIRECTORY     0040000	/* file is a directory */
#define I_CHAR_SPECIAL  0020000	/* character special file */
#define I_NAMED_PIPE    0010000	/* named pipe (FIFO) */
#define I_SET_UID_BIT   0004000	/* set effective uid_t on exec */
#define I_SET_GID_BIT   0002000	/* set effective gid_t on exec */
#define I_SET_STCKY_BIT 0001000	/* sticky bit */ 
#define ALL_MODES       0007777	/* all bits for user, group and others */
#define RWX_MODES       0000777	/* mode bits for RWX only */
#define R_BIT           0000004	/* Rwx protection bit */
#define W_BIT           0000002	/* rWx protection bit */
#define X_BIT           0000001	/* rwX protection bit */
#define I_NOT_ALLOC     0000000	/* this inode is free */

/* Some limits. */
#define MAX_INODE_NR ((ino_t) 037777777777)	/* largest inode number */
#define MAX_FILE_POS ((off_t) 0x7FFFFFFF)	/* largest legal file offset */

#define MAX_SYM_LOOPS	8	/* how many symbolic links are recursed */

#define NO_BLOCK              ((block_t) 0)	/* absence of a block number */
#define NO_ENTRY                ((ino_t) 0)	/* absence of a dir entry */
#define NO_ZONE                ((zone_t) 0)	/* absence of a zone number */
#define NO_DEV                  ((dev_t) 0)	/* absence of a device numb */

#define SERVARNAME		"cttyline"

/* Bits for the system property flags in boot image processes. */
#define PROC_FULLVM    0x100    /* VM sets and manages full pagetable */

/* Bits for s_flags in the privilege structure. */
#define PREEMPTIBLE     0x02    /* kernel tasks are not preemptible */
#define BILLABLE        0x04    /* some processes are not billable */
#define DYN_PRIV_ID     0x08    /* privilege id assigned dynamically */
 
#define SYS_PROC        0x10    /* system processes have own priv structure */
#define CHECK_IO_PORT   0x20    /* check if I/O request is allowed */
#define CHECK_IRQ       0x40    /* check if IRQ can be used */
#define CHECK_MEM       0x80    /* check if (VM) mem map request is allowed */
