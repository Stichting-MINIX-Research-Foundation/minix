
#ifndef _VM_H
#define _VM_H 1

#define _SYSTEM 1

/* Compile in asserts and custom sanity checks at all? */
#define SANITYCHECKS	0
#define CACHE_SANITY	0
#define VMSTATS		0

/* VM behaviour */
#define MEMPROTECT	0	/* Slab objects not mapped. Access with USE() */
#define JUNKFREE	0	/* Fill freed pages with junk */

#include <sys/errno.h>

#include "sanitycheck.h"
#include "region.h"

/* Memory flags to pt_allocmap() and alloc_mem(). */
#define PAF_CLEAR	0x01	/* Clear physical memory. */
#define PAF_CONTIG	0x02	/* Physically contiguous. */
#define PAF_ALIGN64K	0x04	/* Aligned to 64k boundary. */
#define PAF_LOWER16MB	0x08
#define PAF_LOWER1MB	0x10
#define PAF_ALIGN16K	0x40	/* Aligned to 16k boundary. */

#define MARK do { if(mark) { printf("%d\n", __LINE__); } } while(0)

/* special value for v in pt_allocmap */
#define AM_AUTO         ((u32_t) -1)

/* How noisy are we supposed to be? */
#define VERBOSE		0
#define LU_DEBUG	0

/* Minimum stack region size - 64MB. */
#define MINSTACKREGION	(64*1024*1024)

/* If so, this level: */
#define SCL_NONE	0	/* No sanity checks - assert()s only. */
#define SCL_TOP		1	/* Main loop and other high-level places. */
#define SCL_FUNCTIONS	2	/* Function entry/exit. */
#define SCL_DETAIL	3	/* Detailled steps. */
#define SCL_MAX		3	/* Highest value. */

/* Type of page allocations. */
#define VMP_SPARE	0
#define VMP_PAGETABLE	1
#define VMP_PAGEDIR	2
#define VMP_SLAB	3
#define VMP_CATEGORIES	4

/* Flags to pt_writemap(). */
#define WMF_OVERWRITE		0x01	/* Caller knows map may overwrite. */
#define WMF_WRITEFLAGSONLY	0x02	/* Copy physaddr and update flags. */
#define WMF_FREE		0x04	/* Free pages overwritten. */
#define WMF_VERIFY		0x08	/* Check pagetable contents. */

#define MAP_NONE	0xFFFFFFFE
#define NO_MEM ((phys_clicks) MAP_NONE)  /* returned by alloc_mem() with mem is up */

/* And what is the highest addressable piece of memory? */
#define VM_DATATOP	kernel_boot_info.user_end

#define VM_STACKTOP	kernel_boot_info.user_sp

/* Live update will work only with magic instrumentation. Live update requires
 * strict separation of regions within the process to succeed. Therefore,
 * apply this strict separation only if magic instrumentation is used.
 * Otherwise, do not place such limitations on processes.
 */
#ifdef _MINIX_MAGIC
#define VM_MMAPTOP	(VM_STACKTOP-DEFAULT_STACK_LIMIT)
#define VM_MMAPBASE	(VM_MMAPTOP/2)
#else
#define VM_MMAPTOP	VM_DATATOP
#define VM_MMAPBASE	VM_PAGE_SIZE
#endif

extern char _end;
#define VM_OWN_HEAPSTART ((vir_bytes) (&_end))
#define VM_OWN_HEAPBASE   roundup(VM_OWN_HEAPSTART, VM_PAGE_SIZE)
#define VM_OWN_MMAPBASE (VM_OWN_HEAPBASE+1024*1024*1024)
#define VM_OWN_MMAPTOP   (VM_OWN_MMAPBASE+100 * 1024 * 1024)

#endif
