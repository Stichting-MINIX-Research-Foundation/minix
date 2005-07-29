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

/* Define stack sizes for all tasks included in the system image. */
#define NO_STACK	0
#define SMALL_STACK	(128 * sizeof(char *))
#if (CHIP == INTEL)			/* 3 intr, 3 temps, 4 db */
#define	IDLE_S		((3+3+4) * sizeof(char *))  
#else
#define IDLE_S		SMALL_STACK
#endif
#define	HARDWARE_S	NO_STACK	/* dummy task, uses kernel stack */
#define	SYSTEM_S	SMALL_STACK
#define	CLOCK_S		SMALL_STACK

/* Stack space for all the task stacks.  Declared as (char *) to align it. */
#define	TOT_STACK_SPACE	(IDLE_S + HARDWARE_S + CLOCK_S + SYSTEM_S)
PUBLIC char *t_stack[TOT_STACK_SPACE / sizeof(char *)];
	

/* The system image table lists all programs that are part of the boot image. 
 * The order of the entries here MUST agree with the order of the programs
 * in the boot image and all kernel tasks must come first.
 * Each entry provides the process number, type, scheduling priority, send
 * mask, and a name for the process table. For kernel processes, the startup 
 * routine and stack size is also provided.
 */
#define USER_F		(PREEMPTIBLE | BILLABLE)
#define IDLE_F 		(BILLABLE | SYS_PROC)
#define SYS_F  		(PREEMPTIBLE | SYS_PROC)
#define TASK_F 		(SYS_PROC)	

#define IDLE_T		32		/* ticks */
#define USER_T		 8		/* ticks */
#define SYS_T		16		/* ticks */

PUBLIC struct boot_image image[] = {
 { IDLE,    idle_task,  IDLE_F, IDLE_T,   IDLE_Q,  IDLE_S,    EMPTY_MASK, EMPTY_MASK,    "IDLE"    },
 { CLOCK,   clock_task, TASK_F, SYS_T,   TASK_Q, CLOCK_S,   TASK_CALL_MASK, SYSTEM_SEND_MASK,   "CLOCK"   },
 { SYSTEM,  sys_task,   TASK_F, SYS_T,   TASK_Q, SYSTEM_S,     TASK_CALL_MASK, SYSTEM_SEND_MASK,  "SYSTEM"     },
 { HARDWARE,   0,       TASK_F, SYS_T,   TASK_Q, HARDWARE_S, EMPTY_MASK, SYSTEM_SEND_MASK, "KERNEL" },
 { PM_PROC_NR, 0,       SYS_F, SYS_T, 3, 0,          FILLED_MASK,   SERVER_SEND_MASK,      "PM"      },
 { FS_PROC_NR, 0,       SYS_F, SYS_T, 4, 0,          FILLED_MASK,   SERVER_SEND_MASK,      "FS"      },
 { SM_PROC_NR, 0,       SYS_F, SYS_T, 3, 0,          FILLED_MASK,   SYSTEM_SEND_MASK,      "SM"      },
 { IS_PROC_NR, 0,       SYS_F, SYS_T, 2, 0,           FILLED_MASK,  DRIVER_SEND_MASK,      "IS"      },
 { TTY, 0,              SYS_F, SYS_T, 1, 0,           FILLED_MASK, SYSTEM_SEND_MASK,      "TTY"      },
 { MEMORY, 0,           SYS_F, SYS_T, 2, 0,           FILLED_MASK,  DRIVER_SEND_MASK,     "MEMORY" },
#if ENABLE_AT_WINI
 { AT_WINI, 0,            SYS_F, SYS_T, 2, 0,          FILLED_MASK, DRIVER_SEND_MASK,      "AT_WINI" },
#endif
#if ENABLE_FLOPPY
 { FLOPPY, 0,            SYS_F, SYS_T, 2, 0,           FILLED_MASK,  DRIVER_SEND_MASK,  "FLOPPY" },
#endif
#if ENABLE_PRINTER
 { PRINTER, 0,            SYS_F, SYS_T, 3, 0,         FILLED_MASK,  DRIVER_SEND_MASK,     "PRINTER" },
#endif
#if ENABLE_RTL8139
 { RTL8139, 0,            SYS_F, SYS_T, 2, 0,           FILLED_MASK,  DRIVER_SEND_MASK,  "RTL8139" },
#endif
#if ENABLE_FXP
 { FXP, 0,                SYS_F, SYS_T, 2, 0,           FILLED_MASK,  DRIVER_SEND_MASK,  "FXP" },
#endif
#if ENABLE_DPETH
 { DPETH, 0,              SYS_F, SYS_T, 2, 0,           FILLED_MASK,  DRIVER_SEND_MASK,  "DPETH" },
#endif
#if ENABLE_LOG
 { LOG_PROC_NR, 0,     SYS_F, SYS_T, 2, 0,           FILLED_MASK,  SYSTEM_SEND_MASK,  "LOG" },
#endif
#if ENABLE_BIOS_WINI
 { BIOS_WINI, 0,     SYS_F, SYS_T, 2, 0,           FILLED_MASK,  SYSTEM_SEND_MASK,  "BIOS" },
#endif
 { INIT_PROC_NR, 0,    USER_F, USER_T, USER_Q, 0,         USER_CALL_MASK,    USER_SEND_MASK,  "INIT"    },
};

/* Verify the size of the system image table at compile time. If the number 
 * is not correct, the size of the 'dummy' array will be negative, causing
 * a compile time error. Note that no space is allocated because 'dummy' is
 * declared extern.
  */
extern int dummy[(NR_BOOT_PROCS==sizeof(image)/sizeof(struct boot_image))?1:-1];

