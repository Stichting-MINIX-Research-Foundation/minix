
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

EXTERN struct vmproc vmproc[_NR_PROCS+1];

#if SANITYCHECKS
u32_t data1[200];
#define CHECKADDR 0
EXTERN long vm_sanitychecklevel;
#endif

int verbosealloc;

#define VMP_SYSTEM	_NR_PROCS

/* vm operation mode state and values */
EXTERN long vm_paged;

