#ifndef _MINIX_CONST_H
#define _MINIX_CONST_H

#include <machine/archconst.h>

/* The UNUSED annotation tells the compiler or lint not to complain
 * about an unused variable or function parameter.
 *
 * A number of different annotations are used, depending on the
 * compiler or checker that is looking at the code.
 *
 * Note that some variants rename the parameter, so if you use
 * the parameter after all, you'll get a complaint about a missing
 * variable.
 *
 * You use it like this:
 *
 *   void foo(int UNUSED(x)){}
 */

#ifndef UNUSED
#if defined _lint
# define UNUSED(v) /*lint -e(715,818)*/ v
#elif defined(__GNUC__)
# define UNUSED(v) UNUSED_ ## v __attribute((unused))
#elif defined __LCLINT__
# define UNUSED(v) /*@unused@*/ v
#else
# define UNUSED(v) _UNUSED_ ## v
#endif
#endif

#define EXTERN        extern	/* used in *.h files */

#define TRUE               1	/* used for turning integers into Booleans */
#define FALSE              0	/* used for turning integers into Booleans */

#define SUPER_USER ((uid_t) 0)	/* uid_t of superuser */

#include <sys/null.h>      /* NULL Pointer */

#define SCPVEC_NR	  64	/* max # of entries in a SYS_VSAFECOPY request */
#define MAPVEC_NR	  64	/* max # of entries in a SYS_VUMAP request */
#define NR_IOREQS	  64	/* maximum number of entries in an iorequest */

#define VUA_READ	0x01	/* for SYS_VUMAP: read access */
#define VUA_WRITE	0x02	/* for SYS_VUMAP: write access */

/* Message passing constants. */
#define MESS_SIZE (sizeof(message))	/* might need usizeof from FS here */

/* Memory related constants. */
#define SEGMENT_TYPE  0xFF00	/* bit mask to get segment type */
#define SEGMENT_INDEX 0x00FF	/* bit mask to get segment index */

#define PHYS_SEG      0x0400	/* flag indicating entire physical memory */

#define LOCAL_VM_SEG  0x1000	/* same as LOCAL_SEG, but with vm lookup */
#define MEM_GRANT	3
#define VIR_ADDR	1
#define VM_D		(LOCAL_VM_SEG | VIR_ADDR)
#define VM_GRANT	(LOCAL_VM_SEG | MEM_GRANT)

/* Labels used to disable code sections for different reasons. */
#define DEAD_CODE	   0	/* unused code in normal configuration */
#define FUTURE_CODE	   0	/* new code to be activated + tested later */
#define TEMP_CODE	   1	/* active code to be removed later */

/* Miscellaneous */
#define BYTE            0377	/* mask for 8 bits */
#define READING            0	/* copy data to user */
#define WRITING            1	/* copy data from user */
#define PEEKING            2	/* retrieve FS data without copying */
#define HAVE_SCATTERED_IO  1	/* scattered I/O is now standard */

/* Memory is allocated in clicks. */
#if defined(__i386__) || defined(__arm__)
#define CLICK_SIZE      4096	/* unit in which memory is allocated */
#define CLICK_SHIFT       12	/* log2 of CLICK_SIZE */
#else
#error Unsupported arch
#endif

/* Click alignment macros. */
#define CLICK_FLOOR(n)  (((vir_bytes)(n) / CLICK_SIZE) * CLICK_SIZE)
#define CLICK_CEIL(n)   CLICK_FLOOR((vir_bytes)(n) + CLICK_SIZE-1)

/* Sizes of memory tables. The boot monitor distinguishes three memory areas,
 * namely low mem below 1M, 1M-16M, and mem after 16M. More chunks are needed
 * for DOS MINIX.
*/
#define NR_MEMS            16

#define CLICK2ABS(v) ((v) << CLICK_SHIFT)
#define ABS2CLICK(a) ((a) >> CLICK_SHIFT)

/* Flag bits for i_mode in the inode. */
#define I_TYPE          0170000	/* this field gives inode type */
#define I_UNIX_SOCKET	0140000 /* unix domain socket */
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
#define UMAX_FILE_POS ((unsigned) 0x7FFFFFFF)	/* largest legal file offset */

#define MAX_SYM_LOOPS	8	/* how many symbolic links are recursed */

#define NO_BLOCK              ((block_t) 0)	/* absence of a block number */
#define NO_ENTRY                ((ino_t) 0)	/* absence of a dir entry */
#define NO_ZONE                ((zone_t) 0)	/* absence of a zone number */
#define NO_DEV                  ((dev_t) 0)	/* absence of a device numb */
#define NO_LINK		      ((nlink_t) 0)	/* absence of incoming links */
#define INVAL_UID	       ((uid_t) -1)	/* invalid uid value */
#define INVAL_GID	       ((gid_t) -1)	/* invalid gid value */

#define SERVARNAME		"cttyline"
#define ARCHVARNAME		"arch"
#define BOARDVARNAME		"board"
#define SERBAUDVARNAME		"cttybaud"

/* Bits for s_flags in the privilege structure. */
#define PREEMPTIBLE     0x002   /* kernel tasks are not preemptible */
#define BILLABLE        0x004   /* some processes are not billable */
#define DYN_PRIV_ID     0x008   /* privilege id assigned dynamically */
 
#define SYS_PROC        0x010   /* system processes have own priv structure */
#define CHECK_IO_PORT   0x020   /* check if I/O request is allowed */
#define CHECK_IRQ       0x040   /* check if IRQ can be used */
#define CHECK_MEM       0x080   /* check if (VM) mem map request is allowed */
#define ROOT_SYS_PROC   0x100   /* this is a root system process instance */
#define VM_SYS_PROC     0x200   /* this is a vm system process instance */
#define LU_SYS_PROC     0x400   /* this is a live updated sys proc instance */
#define RST_SYS_PROC    0x800   /* this is a restarted sys proc instance */

/* Values for the "verbose" boot monitor variable */
#define VERBOSEBOOT_QUIET 0
#define VERBOSEBOOT_BASIC 1
#define VERBOSEBOOT_EXTRA 2
#define VERBOSEBOOT_MAX   3
#define VERBOSEBOOTVARNAME "verbose"

/* magic value to put in struct proc entries for sanity checks. */
#define PMAGIC 0xC0FFEE1

/* MINIX_KERNFLAGS flags */
#define MKF_I386_INTEL_SYSENTER	(1L << 0) /* SYSENTER available and supported */
#define MKF_I386_AMD_SYSCALL	(1L << 1) /* SYSCALL available and supported */

#endif /* _MINIX_CONST_H */
