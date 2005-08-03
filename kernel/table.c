/* The object file of "table.c" contains most kernel data. Variables that 
 * are declared in the *.h files appear with EXTERN in front of them, as in
 *
 *    EXTERN int x;
 *
 * Normally EXTERN is defined as extern, so when they are included in another
 * file, no storage is allocated.  If EXTERN were not present, but just say,
 *
 *    int x;
 *
 * then including this file in several source files would cause 'x' to be
 * declared several times.  While some linkers accept this, others do not,
 * so they are declared extern when included normally.  However, it must be
 * declared for real somewhere.  That is done here, by redefining EXTERN as
 * the null string, so that inclusion of all *.h files in table.c actually
 * generates storage for them.  
 *
 * Various variables could not be declared EXTERN, but are declared PUBLIC
 * or PRIVATE. The reason for this is that extern variables cannot have a  
 * default initialization. If such variables are shared, they must also be
 * declared in one of the *.h files without the initialization.  Examples 
 * include 'boot_image' (this file) and 'idt' and 'gdt' (protect.c). 
 *
 * Changes:
 *    Nov 10, 2004   removed controller->driver mappings  (Jorrit N. Herder)
 *    Oct 17, 2004   updated above and tasktab comments  (Jorrit N. Herder)
 *    May 01, 2004   changed struct for system image  (Jorrit N. Herder)
 */
#define _TABLE

#include "kernel.h"
#include "proc.h"
#include "ipc.h"
#include <minix/com.h>
#include <ibm/int86.h>

/* Define stack sizes for the kernel tasks included in the system image. */
#define NO_STACK	0
#define SMALL_STACK	(128 * sizeof(char *))
#define IDLE_S		SMALL_STACK	/* 3 intr, 3 temps, 4 db for Intel */
#define	HRDW_S		NO_STACK	/* dummy task, uses kernel stack */
#define	TASK_S		SMALL_STACK	/* system and clock task */

/* Stack space for all the task stacks.  Declared as (char *) to align it. */
#define	TOT_STACK_SPACE	(IDLE_S + HRDW_S + (2 * TASK_S))
PUBLIC char *t_stack[TOT_STACK_SPACE / sizeof(char *)];
	
/* Define flags for the various process types. */
#define IDLE_F 		(BILLABLE | SYS_PROC)		/* idle task */
#define TASK_F 		(SYS_PROC)			/* kernel tasks */
#define SERV_F 		(PREEMPTIBLE | SYS_PROC)	/* system services */
#define USER_F		(PREEMPTIBLE | BILLABLE)	/* user processes */

/* Define system call traps for the various process types. These call masks
 * determine what system call traps a process is allowed to make.
 */
#define TASK_T		(1 << RECEIVE)			/* clock and system */
#define SERV_T		(~0)				/* system services */
#define USER_T          ((1 << SENDREC) | (1 << ECHO))	/* user processes */

/* Send masks determine to whom processes can send messages or notifications. 
 * The values here are used for the processes in the boot image. We rely on 
 * the initialization code in main() to match the s_nr_to_id() mapping for the
 * processes in the boot image, so that the send mask that is defined here 
 * can be directly copied onto map[0] of the actual send mask. Privilege
 * structure 0 is shared by user processes. 
 *
 * Note that process numbers in the boot image should not be higher than
 * "BITCHUNK_BITS - NR_TASKS", because a bitchunk_t field is used to store 
 * the send masks in the table that describes that processes in the image.  
 */
#define SERV_M		(~0)
#define SYST_M		(~0)
#define USER_M		(s(PM_PROC_NR)|s(FS_PROC_NR)|s(SM_PROC_NR))
#define DRIV_M		(USER_M | \
			 s(SYSTEM)|s(CLOCK)|s(LOG_PROC_NR)|s(TTY_PROC_NR))

/* Sanity check to make sure the send masks can be set. */
extern int dummy[(BITCHUNK_BITS-NR_TASKS > INIT_PROC_NR) ? 1 : -1];


/* The system image table lists all programs that are part of the boot image. 
 * The order of the entries here MUST agree with the order of the programs
 * in the boot image and all kernel tasks must come first.
 * Each entry provides the process number, flags, quantum size (qs), scheduling 
 * queue, allowed traps, ipc mask, and a name for the process table. The 
 * initial program counter and stack size is also provided for kernel tasks.
 */
PUBLIC struct boot_image image[] = {
/* process nr,    pc,  flags, qs,  queue,  stack,  traps, ipc mask,  name */ 
 { IDLE,   idle_task, IDLE_F, 32, IDLE_Q, IDLE_S,      0,      0, "IDLE"   },
 { CLOCK, clock_task, TASK_F,  0, TASK_Q, TASK_S, TASK_T,      0, "CLOCK"  },
 { SYSTEM,  sys_task, TASK_F,  0, TASK_Q, TASK_S, TASK_T,      0, "SYSTEM" },
 { HARDWARE,       0, TASK_F,  0, TASK_Q, HRDW_S,      0,      0, "KERNEL" },
 { PM_PROC_NR,     0, SERV_F, 16,      3, 0,      SERV_T, SERV_M, "pm"     },
 { FS_PROC_NR,     0, SERV_F, 16,      4, 0,      SERV_T, SERV_M, "fs"     },
 { SM_PROC_NR,     0, SERV_F, 16,      3, 0,      SERV_T, SYST_M, "sm"     },
 { TTY_PROC_NR,    0, SERV_F, 16,      1, 0,      SERV_T, SYST_M, "tty"    },
 { MEM_PROC_NR,    0, SERV_F, 16,      2, 0,      SERV_T, DRIV_M, "memory" },
 { LOG_PROC_NR,    0, SERV_F, 16,      2, 0,      SERV_T, SYST_M, "log"    },
 { DRVR_PROC_NR,   0, SERV_F, 16,      2, 0,      SERV_T, DRIV_M, "driver" },
 { INIT_PROC_NR,   0, USER_F,  8, USER_Q, 0,      USER_T, USER_M, "init"   },
};

/* Verify the size of the system image table at compile time. If the number 
 * is not correct, the size of the 'dummy' array will be negative, causing
 * a compile time error. Note that no space is allocated because 'dummy' is
 * declared extern.
  */
extern int dummy[(NR_BOOT_PROCS==sizeof(image)/sizeof(struct boot_image))?1:-1];

