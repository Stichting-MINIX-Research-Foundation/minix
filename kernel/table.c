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
 *    Nov 22, 2009   rewrite of privilege management (Cristiano Giuffrida)
 *    Aug 02, 2005   set privileges and minimal boot image (Jorrit N. Herder)
 *    Oct 17, 2004   updated above and tasktab comments  (Jorrit N. Herder)
 *    May 01, 2004   changed struct for system image  (Jorrit N. Herder)
 */
#define _TABLE

#include "kernel.h"
#include "proc.h"
#include "ipc.h"
#include <minix/com.h>

/* Define stack sizes for the kernel tasks included in the system image. */
#define NO_STACK	0
#define SMALL_STACK	(1024 * sizeof(char *))
#define IDL_S	SMALL_STACK	/* 3 intr, 3 temps, 4 db for Intel */
#define	HRD_S	NO_STACK	/* dummy task, uses kernel stack */
#define	TSK_S	SMALL_STACK	/* system and clock task */

/* Stack space for all the task stacks.  Declared as (char *) to align it. */
#define	TOT_STACK_SPACE	(IDL_S + HRD_S + (2 * TSK_S))
PUBLIC char *t_stack[TOT_STACK_SPACE / sizeof(char *)];

/* Define boot process flags. */
#define BVM_F    (PROC_FULLVM)                    /* boot processes with VM */

/* The system image table lists all programs that are part of the boot image. 
 * The order of the entries here MUST agree with the order of the programs
 * in the boot image and all kernel tasks must come first.
 *
 * Each entry provides the process number, flags, quantum size, scheduling
 * queue, and a name for the process table. The initial program counter and
 * stack size is also provided for kernel tasks.
 *
 * Note: the quantum size must be positive in all cases! 
 */

PUBLIC struct boot_image image[] = {
/* process nr, pc, flags, qs,  queue, stack,   name */ 
{IDLE,       NULL,     0,  0,      0, IDL_S, "idle"  },
{CLOCK,clock_task,     0,  8, TASK_Q, TSK_S, "clock" },
{SYSTEM, sys_task,     0,  8, TASK_Q, TSK_S, "system"},
{HARDWARE,      0,     0,  8, TASK_Q, HRD_S, "kernel"},
{PM_PROC_NR,    0,     0, 32,      4, 0,     "pm"    },
{FS_PROC_NR,    0,     0, 32,      5, 0,     "vfs"   },
{RS_PROC_NR,    0,     0,  4,      4, 0,     "rs"    },
{MEM_PROC_NR,   0, BVM_F,  4,      3, 0,     "memory"},
{LOG_PROC_NR,   0, BVM_F,  4,      2, 0,     "log"   },
{TTY_PROC_NR,   0, BVM_F,  4,      1, 0,     "tty"   },
{DS_PROC_NR,    0, BVM_F,  4,      4, 0,     "ds"    },
{MFS_PROC_NR,   0, BVM_F, 32,      5, 0,     "mfs"   },
{VM_PROC_NR,    0,     0, 32,      2, 0,     "vm"    },
{PFS_PROC_NR,   0, BVM_F, 32,      5, 0,     "pfs"   },
{INIT_PROC_NR,  0, BVM_F,  8, USER_Q, 0,     "init"  },
};

/* Verify the size of the system image table at compile time. Also verify that 
 * the first chunk of the ipc mask has enough bits to accommodate the processes
 * in the image.  
 * If a problem is detected, the size of the 'dummy' array will be negative, 
 * causing a compile time error. Note that no space is actually allocated 
 * because 'dummy' is declared extern.
 */
extern int dummy[(NR_BOOT_PROCS==sizeof(image)/
	sizeof(struct boot_image))?1:-1];
extern int dummy[(BITCHUNK_BITS > NR_BOOT_PROCS - 1) ? 1 : -1];
