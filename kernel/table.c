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
#define SMALL_STACK	(256 * sizeof(char *))
#define IDL_S	SMALL_STACK	/* 3 intr, 3 temps, 4 db for Intel */
#define	HRD_S	NO_STACK	/* dummy task, uses kernel stack */
#define	TSK_S	SMALL_STACK	/* system and clock task */

/* Stack space for all the task stacks.  Declared as (char *) to align it. */
#define	TOT_STACK_SPACE	(IDL_S + HRD_S + (2 * TSK_S))
PUBLIC char *t_stack[TOT_STACK_SPACE / sizeof(char *)];
	
/* Define flags for the various process types. */
#define IDL_F 	(SYS_PROC | PREEMPTIBLE | BILLABLE)	/* idle task */
#define TSK_F 	(SYS_PROC)				/* kernel tasks */
#define SRV_F 	(SYS_PROC | PREEMPTIBLE)		/* system services */
#define USR_F	(BILLABLE | PREEMPTIBLE)		/* user processes */

/* Define system call traps for the various process types. These call masks
 * determine what system call traps a process is allowed to make.
 */
#define TSK_T	(1 << RECEIVE)			/* clock and system */
#define SRV_T	(~0)				/* system services */
#define USR_T   ((1 << SENDREC) | (1 << ECHO))	/* user processes */

/* Send masks determine to whom processes can send messages or notifications. 
 * The values here are used for the processes in the boot image. We rely on 
 * the initialization code in main() to match the s_nr_to_id() mapping for the
 * processes in the boot image, so that the send mask that is defined here 
 * can be directly copied onto map[0] of the actual send mask. Privilege
 * structure 0 is shared by user processes. 
 */
#define s(n)	(1 << (s_nr_to_id(n)))
#define NUL_M   0
#define SRV_M	(~0)
#define SYS_M	(~0)
#define USR_M (s(PM_PROC_NR) | s(FS_PROC_NR) | s(RS_PROC_NR) | s(SYSTEM))
#define DRV_M (USR_M | s(SYSTEM) | s(CLOCK) | s(DS_PROC_NR) | s(LOG_PROC_NR) | s(TTY_PROC_NR))

/* Define kernel calls that processes are allowed to make. This is not looking
 * very nice, but we need to define the access rights on a per call basis. 
 * Note that the reincarnation server has all bits on, because it should
 * be allowed to distribute rights to services that it starts. 
 * 
 * Calls are unordered lists, converted by the kernel to bitmasks
 * once at runtime.
 */
#define FS_C SYS_KILL, SYS_VIRCOPY, SYS_SAFECOPYFROM, SYS_SAFECOPYTO, \
    SYS_VIRVCOPY, SYS_UMAP, SYS_GETINFO, SYS_EXIT, SYS_TIMES, SYS_SETALARM, \
    SYS_PRIVCTL, SYS_TRACE , SYS_SETGRANT, SYS_PROFBUF
#define DRV_C	FS_C, SYS_SEGCTL, SYS_IRQCTL, SYS_INT86, SYS_DEVIO, \
	SYS_SDEVIO, SYS_VDEVIO, SYS_SETGRANT, SYS_PROFBUF

PRIVATE int
  fs_c[] = { FS_C },
  pm_c[] = { SYS_ALL_CALLS },
  rs_c[] = { SYS_ALL_CALLS },
  ds_c[] = { SYS_ALL_CALLS },
  drv_c[] = { DRV_C },
  tty_c[] = { DRV_C, SYS_PHYSCOPY, SYS_ABORT, SYS_VM_MAP, SYS_IOPENABLE,
		SYS_READBIOS },
  mem_c[] = { DRV_C, SYS_PHYSCOPY, SYS_PHYSVCOPY, SYS_VM_MAP, SYS_IOPENABLE };

/* The system image table lists all programs that are part of the boot image. 
 * The order of the entries here MUST agree with the order of the programs
 * in the boot image and all kernel tasks must come first.
 *
 * Each entry provides the process number, flags, quantum size, scheduling
 * queue, allowed traps, ipc mask, and a name for the process table. The 
 * initial program counter and stack size is also provided for kernel tasks.
 *
 * Note: the quantum size must be positive in all cases! 
 */
#define c(calls) calls, (sizeof(calls) / sizeof((calls)[0]))
#define no_c { 0 }, 0

PUBLIC struct boot_image image[] = {
/* process nr, pc,flags, qs,  queue, stack, traps, ipcto, call,  name */ 
{IDLE,  idle_task,IDL_F,  8, IDLE_Q, IDL_S,     0,     0, no_c,"idle"  },
{CLOCK,clock_task,TSK_F,  8, TASK_Q, TSK_S, TSK_T,     0, no_c,"clock" },
{SYSTEM, sys_task,TSK_F,  8, TASK_Q, TSK_S, TSK_T,     0, no_c,"system"},
{HARDWARE,      0,TSK_F,  8, TASK_Q, HRD_S,     0,     0, no_c,"kernel"},
{PM_PROC_NR,    0,SRV_F, 32,      3, 0,     SRV_T, SRV_M, c(pm_c),"pm"    },
{FS_PROC_NR,    0,SRV_F, 32,      4, 0,     SRV_T, SRV_M, c(fs_c),"vfs"   },
{RS_PROC_NR,    0,SRV_F,  4,      3, 0,     SRV_T, SYS_M, c(rs_c),"rs"    },
{DS_PROC_NR,    0,SRV_F,  4,      3, 0,     SRV_T, SYS_M, c(ds_c),"ds"    },
{TTY_PROC_NR,   0,SRV_F,  4,      1, 0,     SRV_T, SYS_M,c(tty_c),"tty"   },
{MEM_PROC_NR,   0,SRV_F,  4,      2, 0,     SRV_T, SYS_M,c(mem_c),"memory"},
{LOG_PROC_NR,   0,SRV_F,  4,      2, 0,     SRV_T, SYS_M,c(drv_c),"log"   },
{MFS_PROC_NR,   0,SRV_F, 32,      4, 0,     SRV_T, SRV_M, c(fs_c),"mfs"   },
{INIT_PROC_NR,  0,USR_F,  8, USER_Q, 0,     USR_T, USR_M, no_c,"init"  },
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

