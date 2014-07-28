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

#include "kernel/kernel.h"
#include <minix/com.h>

/* The system image table lists all programs that are part of the boot image. 
 * The order of the entries here MUST agree with the order of the programs
 * in the boot image and all kernel tasks must come first.
 * The order of the entries here matches the priority NOTIFY messages are
 * delivered to a given process. NOTIFY messages are always delivered with
 * the highest priority. DS must be the first system process in the list to
 * allow reliable asynchronous publishing of system events. RS comes right after
 * to prioritize ping messages periodically delivered to system processes.
 */

struct boot_image image[NR_BOOT_PROCS] = {
/* process nr, flags, stack size, name */
{ASYNCM,        "asyncm"},
{IDLE,          "idle"  },
{CLOCK,         "clock" },
{SYSTEM,        "system"},
{HARDWARE,      "kernel"},
                      
{DS_PROC_NR,    "ds"    },
{RS_PROC_NR,    "rs"    },
                      
{PM_PROC_NR,    "pm"    },
{SCHED_PROC_NR, "sched" },
{VFS_PROC_NR,   "vfs"   },
{MEM_PROC_NR,   "memory"},
{TTY_PROC_NR,   "tty"   },
{MFS_PROC_NR,   "mfs"   },
{VM_PROC_NR,    "vm"    },
{PFS_PROC_NR,   "pfs"   },
{INIT_PROC_NR,  "init"  },
};

