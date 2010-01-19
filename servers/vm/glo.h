
#include <minix/sys_config.h>
#include <sys/stat.h>
#include <a.out.h>
#include <tools.h>

#include "vm.h"
#include "vmproc.h"

#if _MAIN
#undef EXTERN
#define EXTERN
#endif

#define VMP_SYSTEM	_NR_PROCS
#define VMP_EXECTMP	_NR_PROCS+1
#define VMP_NR		_NR_PROCS+2

EXTERN struct vmproc vmproc[VMP_NR];

#if SANITYCHECKS
EXTERN int nocheck;
EXTERN int incheck;
EXTERN long vm_sanitychecklevel;
#endif

/* total number of memory pages */
EXTERN int total_pages;

/* vm operation mode state and values */
EXTERN long vm_paged;

EXTERN int meminit_done;
