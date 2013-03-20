
#include <minix/sys_config.h>
#include <minix/type.h>
#include <minix/param.h>
#include <sys/stat.h>
#include <a.out.h>

#include "vm.h"
#include "vmproc.h"

#if _MAIN
#undef EXTERN
#define EXTERN
#endif

#define VMP_EXECTMP	_NR_PROCS
#define VMP_NR		_NR_PROCS+1

EXTERN struct vmproc vmproc[VMP_NR];

EXTERN kinfo_t kernel_boot_info;

#if SANITYCHECKS
EXTERN int nocheck;
EXTERN int incheck;
EXTERN int sc_lastline;
EXTERN char *sc_lastfile;
#endif

extern struct minix_kerninfo *_minix_kerninfo;

/* mem types */
EXTERN  mem_type_t mem_type_anon,       /* anonymous memory */
        mem_type_directphys,		/* direct physical mapping memory */
	mem_type_anon_contig,		/* physically contig anon memory */
	mem_type_cache,			/* disk cache */
	mem_type_mappedfile,		/* memory with file contents */
	mem_type_shared;		/* memory shared by multiple processes */

/* total number of memory pages */
EXTERN int total_pages;
