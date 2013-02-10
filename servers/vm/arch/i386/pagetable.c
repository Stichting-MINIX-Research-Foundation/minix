
#define _SYSTEM 1
#define _POSIX_SOURCE 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/keymap.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/safecopies.h>
#include <minix/cpufeature.h>
#include <minix/bitmap.h>
#include <minix/debug.h>

#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include "proto.h"
#include "glo.h"
#include "util.h"
#include "vm.h"
#include "sanitycheck.h"

static int vm_self_pages;

/* PDE used to map in kernel, kernel physical address. */
#define MAX_PAGEDIR_PDES 5
static struct pdm {
	int		pdeno;
	u32_t		val;
	phys_bytes	phys;
	u32_t		*page_directories;
} pagedir_mappings[MAX_PAGEDIR_PDES];

static u32_t global_bit = 0;

static multiboot_module_t *kern_mb_mod = NULL;
static size_t kern_size = 0;
static int kern_start_pde = -1;

/* big page size available in hardware? */
static int bigpage_ok = 1;

/* Our process table entry. */
struct vmproc *vmprocess = &vmproc[VM_PROC_NR];

/* Spare memory, ready to go after initialization, to avoid a
 * circular dependency on allocating memory and writing it into VM's
 * page table.
 */
#if SANITYCHECKS
#define SPAREPAGES 100
#define STATIC_SPAREPAGES 90
#else
#ifdef __arm__
# define SPAREPAGES 150
# define STATIC_SPAREPAGES 140 
#else
# define SPAREPAGES 20
# define STATIC_SPAREPAGES 15 
#endif /* __arm__ */
#endif

#define SPAREPAGEDIRS 1
#define STATIC_SPAREPAGEDIRS 1

int missing_sparedirs = SPAREPAGEDIRS;
static struct {
	void *pagedir;
	phys_bytes phys;
} sparepagedirs[SPAREPAGEDIRS];

int missing_spares = SPAREPAGES;
static struct {
	void *page;
	phys_bytes phys;
} sparepages[SPAREPAGES];

extern char _end;	
#define is_staticaddr(v) ((vir_bytes) (v) < (vir_bytes) &_end)

#define MAX_KERNMAPPINGS 10
static struct {
	phys_bytes	phys_addr;	/* Physical addr. */
	phys_bytes	len;		/* Length in bytes. */
	vir_bytes	vir_addr;	/* Offset in page table. */
	int		flags;
} kern_mappings[MAX_KERNMAPPINGS];
int kernmappings = 0;

/* Clicks must be pages, as
 *  - they must be page aligned to map them
 *  - they must be a multiple of the page size
 *  - it's inconvenient to have them bigger than pages, because we often want
 *    just one page
 * May as well require them to be equal then.
 */
#if CLICK_SIZE != VM_PAGE_SIZE
#error CLICK_SIZE must be page size.
#endif

static char static_sparepages[VM_PAGE_SIZE*STATIC_SPAREPAGES] 
	__aligned(VM_PAGE_SIZE);

#if defined(__arm__)
static char static_sparepagedirs[ARCH_PAGEDIR_SIZE*STATIC_SPAREPAGEDIRS + ARCH_PAGEDIR_SIZE] __aligned(ARCH_PAGEDIR_SIZE);
#endif

#if SANITYCHECKS
/*===========================================================================*
 *				pt_sanitycheck		     		     *
 *===========================================================================*/
void pt_sanitycheck(pt_t *pt, char *file, int line)
{
/* Basic pt sanity check. */
	int slot;

	MYASSERT(pt);
	MYASSERT(pt->pt_dir);
	MYASSERT(pt->pt_dir_phys);

	for(slot = 0; slot < ELEMENTS(vmproc); slot++) {
		if(pt == &vmproc[slot].vm_pt)
			break;
	}

	if(slot >= ELEMENTS(vmproc)) {
		panic("pt_sanitycheck: passed pt not in any proc");
	}

	MYASSERT(usedpages_add(pt->pt_dir_phys, VM_PAGE_SIZE) == OK);
}
#endif

/*===========================================================================*
 *				findhole		     		     *
 *===========================================================================*/
static u32_t findhole(int pages)
{
/* Find a space in the virtual address space of VM. */
	u32_t curv;
	int pde = 0, try_restart;
	static u32_t lastv = 0;
	pt_t *pt = &vmprocess->vm_pt;
	vir_bytes vmin, vmax;
#if defined(__arm__)
	u32_t holev;
#endif

	vmin = (vir_bytes) (&_end); /* marks end of VM BSS */
	vmin += 1024*1024*1024;	/* reserve 1GB virtual address space for VM heap */
	vmin &= ARCH_VM_ADDR_MASK; 
	vmax = VM_STACKTOP;

	/* Input sanity check. */
	assert(vmin + VM_PAGE_SIZE >= vmin);
	assert(vmax >= vmin + VM_PAGE_SIZE);
	assert((vmin % VM_PAGE_SIZE) == 0);
	assert((vmax % VM_PAGE_SIZE) == 0);
#if defined(__arm__)
	assert(pages > 0);
#endif

	curv = lastv;
	if(curv < vmin || curv >= vmax)
		curv = vmin;

	try_restart = 1;

	/* Start looking for a free page starting at vmin. */
	while(curv < vmax) {
		int pte;
#if defined(__arm__)
		int i, nohole;
#endif

		assert(curv >= vmin);
		assert(curv < vmax);

#if defined(__i386__)
		pde = I386_VM_PDE(curv);
		pte = I386_VM_PTE(curv);
#elif defined(__arm__)
		holev = curv; /* the candidate hole */
		nohole = 0;
		for (i = 0; i < pages && !nohole; ++i) {
		    if(curv >= vmax) {
			break;
		    }
#endif

#if defined(__i386__)
		if(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT) ||
		   !(pt->pt_pt[pde][pte] & ARCH_VM_PAGE_PRESENT)) {
#elif defined(__arm__)
		    pde = ARM_VM_PDE(curv);
		    pte = ARM_VM_PTE(curv);

		    /* if page present, no hole */
		    if((pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT) &&
		       (pt->pt_pt[pde][pte] & ARCH_VM_PTE_PRESENT))
			nohole = 1;

		    /* if not contiguous, no hole */
		    if (curv != holev + i * VM_PAGE_SIZE)
			nohole = 1;

		    curv+=VM_PAGE_SIZE;
		}

		/* there's a large enough hole */
		if (!nohole && i == pages) {
#endif
			lastv = curv;
#if defined(__i386__)
			return curv;
#elif defined(__arm__)
			return holev;
#endif
		}

#if defined(__i386__)
		curv+=VM_PAGE_SIZE;

#elif defined(__arm__)
		/* Reset curv */
#endif
		if(curv >= vmax && try_restart) {
			curv = vmin;
			try_restart = 0;
		}
	}

	printf("VM: out of virtual address space in vm\n");

	return NO_MEM;
}

/*===========================================================================*
 *				vm_freepages		     		     *
 *===========================================================================*/
void vm_freepages(vir_bytes vir, int pages)
{
	assert(!(vir % VM_PAGE_SIZE)); 

	if(is_staticaddr(vir)) {
		printf("VM: not freeing static page\n");
		return;
	}

	if(pt_writemap(vmprocess, &vmprocess->vm_pt, vir,
		MAP_NONE, pages*VM_PAGE_SIZE, 0,
		WMF_OVERWRITE | WMF_FREE) != OK)
		panic("vm_freepages: pt_writemap failed");

	vm_self_pages--;

#if SANITYCHECKS
	/* If SANITYCHECKS are on, flush tlb so accessing freed pages is
	 * always trapped, also if not in tlb.
	 */
	if((sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("VMCTL_FLUSHTLB failed");
	}
#endif
}

/*===========================================================================*
 *				vm_getsparepage		     		     *
 *===========================================================================*/
static void *vm_getsparepage(phys_bytes *phys)
{
	int s;
	assert(missing_spares >= 0 && missing_spares <= SPAREPAGES);
	for(s = 0; s < SPAREPAGES; s++) {
		if(sparepages[s].page) {
			void *sp;
			sp = sparepages[s].page;
			*phys = sparepages[s].phys;
			sparepages[s].page = NULL;
			missing_spares++;
			assert(missing_spares >= 0 && missing_spares <= SPAREPAGES);
			return sp;
		}
	}
	printf("no spare found, %d missing\n", missing_spares);
	return NULL;
}

/*===========================================================================*
 *				vm_getsparepagedir	      		     *
 *===========================================================================*/
static void *vm_getsparepagedir(phys_bytes *phys)
{
	int s;
	assert(missing_sparedirs >= 0 && missing_sparedirs <= SPAREPAGEDIRS);
	for(s = 0; s < SPAREPAGEDIRS; s++) {
		if(sparepagedirs[s].pagedir) {
			void *sp;
			sp = sparepagedirs[s].pagedir;
			*phys = sparepagedirs[s].phys;
			sparepagedirs[s].pagedir = NULL;
			missing_sparedirs++;
			assert(missing_sparedirs >= 0 && missing_sparedirs <= SPAREPAGEDIRS);
			return sp;
		}
	}
	return NULL;
}

/*===========================================================================*
 *				vm_checkspares		     		     *
 *===========================================================================*/
static void *vm_checkspares(void)
{
	int s, n = 0;
	static int total = 0, worst = 0;
	assert(missing_spares >= 0 && missing_spares <= SPAREPAGES);
	for(s = 0; s < SPAREPAGES && missing_spares > 0; s++) {
	    if(!sparepages[s].page) {
		n++;
		if((sparepages[s].page = vm_allocpage(&sparepages[s].phys, 
			VMP_SPARE))) {
			missing_spares--;
			assert(missing_spares >= 0);
			assert(missing_spares <= SPAREPAGES);
		} else {
			printf("VM: warning: couldn't get new spare page\n");
		}
	   }
	}
	if(worst < n) worst = n;
	total += n;

	return NULL;
}

static int pt_init_done;

/*===========================================================================*
 *				vm_allocpage		     		     *
 *===========================================================================*/
void *vm_allocpages(phys_bytes *phys, int reason, int pages)
{
/* Allocate a page for use by VM itself. */
	phys_bytes newpage;
	vir_bytes loc;
	pt_t *pt;
	int r;
	static int level = 0;
	void *ret;
	u32_t mem_flags = 0;

	pt = &vmprocess->vm_pt;
	assert(reason >= 0 && reason < VMP_CATEGORIES);

	assert(pages > 0);

	level++;

	assert(level >= 1);
	assert(level <= 2);

	if((level > 1) || !pt_init_done) {
		void *s;

		if(pages == 1) s=vm_getsparepage(phys);
		else if(pages == 4) s=vm_getsparepagedir(phys);
		else panic("%d pages", pages);

		level--;
		if(!s) {
			util_stacktrace();
			printf("VM: warning: out of spare pages\n");
		}
		if(!is_staticaddr(s)) vm_self_pages++;
		return s;
	}

#if defined(__arm__)
	if (reason == VMP_PAGEDIR) {
		mem_flags |= PAF_ALIGN16K;
	}
#endif

	/* VM does have a pagetable, so get a page and map it in there.
	 * Where in our virtual address space can we put it?
	 */
	loc = findhole(pages);
	if(loc == NO_MEM) {
		level--;
		printf("VM: vm_allocpage: findhole failed\n");
		return NULL;
	}

	/* Allocate page of memory for use by VM. As VM
	 * is trusted, we don't have to pre-clear it.
	 */
	if((newpage = alloc_mem(pages, mem_flags)) == NO_MEM) {
		level--;
		printf("VM: vm_allocpage: alloc_mem failed\n");
		return NULL;
	}

	*phys = CLICK2ABS(newpage);

	/* Map this page into our address space. */
	if((r=pt_writemap(vmprocess, pt, loc, *phys, VM_PAGE_SIZE*pages,
		ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW
#if defined(__arm__)
		| ARM_VM_PTE_WB | ARM_VM_PTE_SHAREABLE
#endif
		, 0)) != OK) {
		free_mem(newpage, pages);
		printf("vm_allocpage writemap failed\n");
		level--;
		return NULL;
	}

	if((r=sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("VMCTL_FLUSHTLB failed: %d", r);
	}

	level--;

	/* Return user-space-ready pointer to it. */
	ret = (void *) loc;

	vm_self_pages++;
	return ret;
}

void *vm_allocpage(phys_bytes *phys, int reason)
{
	return vm_allocpages(phys, reason, 1);
}

/*===========================================================================*
 *				vm_pagelock		     		     *
 *===========================================================================*/
void vm_pagelock(void *vir, int lockflag)
{
/* Mark a page allocated by vm_allocpage() unwritable, i.e. only for VM. */
	vir_bytes m = (vir_bytes) vir;
	int r;
	u32_t flags = ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER;
	pt_t *pt;

	pt = &vmprocess->vm_pt;

	assert(!(m % VM_PAGE_SIZE));

	if(!lockflag)
		flags |= ARCH_VM_PTE_RW;
#if defined(__arm__)
	else
		flags |= ARCH_VM_PTE_RO;
	flags |= ARM_VM_PTE_WB | ARM_VM_PTE_SHAREABLE;
#endif

	/* Update flags. */
	if((r=pt_writemap(vmprocess, pt, m, 0, VM_PAGE_SIZE,
		flags, WMF_OVERWRITE | WMF_WRITEFLAGSONLY)) != OK) {
		panic("vm_lockpage: pt_writemap failed");
	}

	if((r=sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("VMCTL_FLUSHTLB failed: %d", r);
	}

	return;
}

/*===========================================================================*
 *				vm_addrok		     		     *
 *===========================================================================*/
int vm_addrok(void *vir, int writeflag)
{
	pt_t *pt = &vmprocess->vm_pt;
	int pde, pte;
	vir_bytes v = (vir_bytes) vir;

#if defined(__i386__)
	pde = I386_VM_PDE(v);
	pte = I386_VM_PTE(v);
#elif defined(__arm__)
	pde = ARM_VM_PDE(v);
	pte = ARM_VM_PTE(v);
#endif

	if(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT)) {
		printf("addr not ok: missing pde %d\n", pde);
		return 0;
	}

#if defined(__i386__)
	if(writeflag &&
		!(pt->pt_dir[pde] & ARCH_VM_PTE_RW)) {
		printf("addr not ok: pde %d present but pde unwritable\n", pde);
		return 0;
	}

#endif
	if(!(pt->pt_pt[pde][pte] & ARCH_VM_PTE_PRESENT)) {
		printf("addr not ok: missing pde %d / pte %d\n",
			pde, pte);
		return 0;
	}

#if defined(__i386__)
	if(writeflag &&
		!(pt->pt_pt[pde][pte] & ARCH_VM_PTE_RW)) {
		printf("addr not ok: pde %d / pte %d present but unwritable\n",
#elif defined(__arm__)
	if(!writeflag &&
		!(pt->pt_pt[pde][pte] & ARCH_VM_PTE_RO)) {
		printf("addr not ok: pde %d / pte %d present but writable\n",
#endif
			pde, pte);
		return 0;
	}

	return 1;
}

/*===========================================================================*
 *				pt_ptalloc		     		     *
 *===========================================================================*/
static int pt_ptalloc(pt_t *pt, int pde, u32_t flags)
{
/* Allocate a page table and write its address into the page directory. */
	int i;
	phys_bytes pt_phys;

	/* Argument must make sense. */
	assert(pde >= 0 && pde < ARCH_VM_DIR_ENTRIES);
	assert(!(flags & ~(PTF_ALLFLAGS)));

	/* We don't expect to overwrite page directory entry, nor
	 * storage for the page table.
	 */
	assert(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT));
	assert(!pt->pt_pt[pde]);

	/* Get storage for the page table. */
        if(!(pt->pt_pt[pde] = vm_allocpage(&pt_phys, VMP_PAGETABLE)))
		return ENOMEM;

	for(i = 0; i < ARCH_VM_PT_ENTRIES; i++)
		pt->pt_pt[pde][i] = 0;	/* Empty entry. */

	/* Make page directory entry.
	 * The PDE is always 'present,' 'writable,' and 'user accessible,'
	 * relying on the PTE for protection.
	 */
#if defined(__i386__)
	pt->pt_dir[pde] = (pt_phys & ARCH_VM_ADDR_MASK) | flags
		| ARCH_VM_PDE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW;
#elif defined(__arm__)
	pt->pt_dir[pde] = (pt_phys & ARCH_VM_PDE_MASK)
		| ARCH_VM_PDE_PRESENT | ARM_VM_PDE_DOMAIN;
#endif

	return OK;
}

/*===========================================================================*
 *			    pt_ptalloc_in_range		     		     *
 *===========================================================================*/
int pt_ptalloc_in_range(pt_t *pt, vir_bytes start, vir_bytes end,
	u32_t flags, int verify)
{
/* Allocate all the page tables in the range specified. */
	int pde, first_pde, last_pde;

#if defined(__i386__)
	first_pde = I386_VM_PDE(start);
	last_pde = I386_VM_PDE(end-1);
#elif defined(__arm__)
	first_pde = ARM_VM_PDE(start);
	last_pde = ARM_VM_PDE(end-1);
#endif
	assert(first_pde >= 0);
	assert(last_pde < ARCH_VM_DIR_ENTRIES);

	/* Scan all page-directory entries in the range. */
	for(pde = first_pde; pde <= last_pde; pde++) {
		assert(!(pt->pt_dir[pde] & ARCH_VM_BIGPAGE));
		if(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT)) {
			int r;
			if(verify) {
				printf("pt_ptalloc_in_range: no pde %d\n", pde);
				return EFAULT;
			}
			assert(!pt->pt_dir[pde]);
			if((r=pt_ptalloc(pt, pde, flags)) != OK) {
				/* Couldn't do (complete) mapping.
				 * Don't bother freeing any previously
				 * allocated page tables, they're
				 * still writable, don't point to nonsense,
				 * and pt_ptalloc leaves the directory
				 * and other data in a consistent state.
				 */
				return r;
			}
			assert(pt->pt_pt[pde]);
		}
		assert(pt->pt_pt[pde]);
		assert(pt->pt_dir[pde]);
		assert(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT);
	}

	return OK;
}

static char *ptestr(u32_t pte)
{
#define FLAG(constant, name) {						\
	if(pte & (constant)) { strcat(str, name); strcat(str, " "); }	\
}

	static char str[30];
	if(!(pte & ARCH_VM_PTE_PRESENT)) {
		return "not present";
	}
	str[0] = '\0';
#if defined(__i386__)
	FLAG(ARCH_VM_PTE_RW, "W");
#elif defined(__arm__)
	if(pte & ARCH_VM_PTE_RO) {
	    strcat(str, "R ");
	} else {
	    strcat(str, "W ");
	}
#endif
	FLAG(ARCH_VM_PTE_USER, "U");
#if defined(__i386__)
	FLAG(I386_VM_PWT, "PWT");
	FLAG(I386_VM_PCD, "PCD");
	FLAG(I386_VM_ACC, "ACC");
	FLAG(I386_VM_DIRTY, "DIRTY");
	FLAG(I386_VM_PS, "PS");
	FLAG(I386_VM_GLOBAL, "G");
	FLAG(I386_VM_PTAVAIL1, "AV1");
	FLAG(I386_VM_PTAVAIL2, "AV2");
	FLAG(I386_VM_PTAVAIL3, "AV3");
#elif defined(__arm__)
	FLAG(ARM_VM_PTE_SUPER, "S");
	FLAG(ARM_VM_PTE_SHAREABLE, "SH");
	FLAG(ARM_VM_PTE_WB, "WB");
	FLAG(ARM_VM_PTE_WT, "WT");
#endif

	return str;
}

/*===========================================================================*
 *			     pt_map_in_range		     		     *
 *===========================================================================*/
int pt_map_in_range(struct vmproc *src_vmp, struct vmproc *dst_vmp,
	vir_bytes start, vir_bytes end)
{
/* Transfer all the mappings from the pt of the source process to the pt of
 * the destination process in the range specified.
 */
	int pde, pte;
	vir_bytes viraddr;
	pt_t *pt, *dst_pt;

	pt = &src_vmp->vm_pt;
	dst_pt = &dst_vmp->vm_pt;

	end = end ? end : VM_DATATOP;
	assert(start % VM_PAGE_SIZE == 0);
	assert(end % VM_PAGE_SIZE == 0);
#if defined(__i386__)
	assert(start <= end);
	assert(I386_VM_PDE(end) < ARCH_VM_DIR_ENTRIES);
#elif defined(__arm__)
	assert(ARM_VM_PDE(start) >= 0 && start <= end);
	assert(ARM_VM_PDE(end) < ARCH_VM_DIR_ENTRIES);
#endif

#if LU_DEBUG
	printf("VM: pt_map_in_range: src = %d, dst = %d\n",
		src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
	printf("VM: pt_map_in_range: transferring from 0x%08x (pde %d pte %d) to 0x%08x (pde %d pte %d)\n",
#if defined(__i386__)
		start, I386_VM_PDE(start), I386_VM_PTE(start),
		end, I386_VM_PDE(end), I386_VM_PTE(end));
#elif defined(__arm__)
		start, ARM_VM_PDE(start), ARM_VM_PTE(start),
		end, ARM_VM_PDE(end), ARM_VM_PTE(end));
#endif
#endif

	/* Scan all page-table entries in the range. */
	for(viraddr = start; viraddr <= end; viraddr += VM_PAGE_SIZE) {
#if defined(__i386__)
		pde = I386_VM_PDE(viraddr);
#elif defined(__arm__)
		pde = ARM_VM_PDE(viraddr);
#endif
		if(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT)) {
			if(viraddr == VM_DATATOP) break;
			continue;
		}
#if defined(__i386__)
		pte = I386_VM_PTE(viraddr);
#elif defined(__arm__)
		pte = ARM_VM_PTE(viraddr);
#endif
		if(!(pt->pt_pt[pde][pte] & ARCH_VM_PTE_PRESENT)) {
			if(viraddr == VM_DATATOP) break;
			continue;
		}

		/* Transfer the mapping. */
		dst_pt->pt_pt[pde][pte] = pt->pt_pt[pde][pte];

                if(viraddr == VM_DATATOP) break;
	}

	return OK;
}

/*===========================================================================*
 *				pt_ptmap		     		     *
 *===========================================================================*/
int pt_ptmap(struct vmproc *src_vmp, struct vmproc *dst_vmp)
{
/* Transfer mappings to page dir and page tables from source process and
 * destination process. Make sure all the mappings are above the stack, not
 * to corrupt valid mappings in the data segment of the destination process.
 */
	int pde, r;
	phys_bytes physaddr;
	vir_bytes viraddr;
	pt_t *pt;

	pt = &src_vmp->vm_pt;

#if LU_DEBUG
	printf("VM: pt_ptmap: src = %d, dst = %d\n",
		src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif

	/* Transfer mapping to the page directory. */
	viraddr = (vir_bytes) pt->pt_dir;
	physaddr = pt->pt_dir_phys & ARCH_VM_ADDR_MASK;
#if defined(__i386__)
	if((r=pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr, physaddr, VM_PAGE_SIZE,
		ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW,
#elif defined(__arm__)
	if((r=pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr, physaddr, ARCH_PAGEDIR_SIZE,
		ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW |
		ARM_VM_PTE_WB | ARM_VM_PTE_SHAREABLE,
#endif
		WMF_OVERWRITE)) != OK) {
		return r;
	}
#if LU_DEBUG
	printf("VM: pt_ptmap: transferred mapping to page dir: 0x%08x (0x%08x)\n",
		viraddr, physaddr);
#endif

	/* Scan all non-reserved page-directory entries. */
	for(pde=0; pde < ARCH_VM_DIR_ENTRIES; pde++) {
		if(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT)) {
			continue;
		}

		/* Transfer mapping to the page table. */
		viraddr = (vir_bytes) pt->pt_pt[pde];
#if defined(__i386__)
		physaddr = pt->pt_dir[pde] & ARCH_VM_ADDR_MASK;
#elif defined(__arm__)
		physaddr = pt->pt_dir[pde] & ARCH_VM_PDE_MASK;
#endif
		if((r=pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr, physaddr, VM_PAGE_SIZE,
			ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW
#ifdef __arm__
			| ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW |
			ARM_VM_PTE_WB | ARM_VM_PTE_SHAREABLE
#endif
			,
			WMF_OVERWRITE)) != OK) {
			return r;
		}
	}

	return OK;
}

void pt_clearmapcache(void)
{
	/* Make sure kernel will invalidate tlb when using current
	 * pagetable (i.e. vm's) to make new mappings before new cr3
	 * is loaded.
	 */
	if(sys_vmctl(SELF, VMCTL_CLEARMAPCACHE, 0) != OK)
		panic("VMCTL_CLEARMAPCACHE failed");
}

/*===========================================================================*
 *				pt_writemap		     		     *
 *===========================================================================*/
int pt_writemap(struct vmproc * vmp,
			pt_t *pt,
			vir_bytes v,
			phys_bytes physaddr,
			size_t bytes,
			u32_t flags,
			u32_t writemapflags)
{
/* Write mapping into page table. Allocate a new page table if necessary. */
/* Page directory and table entries for this virtual address. */
	int p, pages;
	int verify = 0;
	int ret = OK;

#ifdef CONFIG_SMP
	int vminhibit_clear = 0;
	/* FIXME
	 * don't do it everytime, stop the process only on the first change and
	 * resume the execution on the last change. Do in a wrapper of this
	 * function
	 */
	if (vmp && vmp->vm_endpoint != NONE && vmp->vm_endpoint != VM_PROC_NR &&
			!(vmp->vm_flags & VMF_EXITING)) {
		sys_vmctl(vmp->vm_endpoint, VMCTL_VMINHIBIT_SET, 0);
		vminhibit_clear = 1;
	}
#endif

	if(writemapflags & WMF_VERIFY)
		verify = 1;

	assert(!(bytes % VM_PAGE_SIZE));
	assert(!(flags & ~(PTF_ALLFLAGS)));

	pages = bytes / VM_PAGE_SIZE;

	/* MAP_NONE means to clear the mapping. It doesn't matter
	 * what's actually written into the PTE if PRESENT
	 * isn't on, so we can just write MAP_NONE into it.
	 */
	assert(physaddr == MAP_NONE || (flags & ARCH_VM_PTE_PRESENT));
	assert(physaddr != MAP_NONE || !flags);

	/* First make sure all the necessary page tables are allocated,
	 * before we start writing in any of them, because it's a pain
	 * to undo our work properly.
	 */
	ret = pt_ptalloc_in_range(pt, v, v + VM_PAGE_SIZE*pages, flags, verify);
	if(ret != OK) {
		printf("VM: writemap: pt_ptalloc_in_range failed\n");
		goto resume_exit;
	}

	/* Now write in them. */
	for(p = 0; p < pages; p++) {
		u32_t entry;
#if defined(__i386__)
		int pde = I386_VM_PDE(v);
		int pte = I386_VM_PTE(v);
#elif defined(__arm__)
		int pde = ARM_VM_PDE(v);
		int pte = ARM_VM_PTE(v);
#endif

		assert(!(v % VM_PAGE_SIZE));
		assert(pte >= 0 && pte < ARCH_VM_PT_ENTRIES);
		assert(pde >= 0 && pde < ARCH_VM_DIR_ENTRIES);

		/* Page table has to be there. */
		assert(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT);

		/* We do not expect it to be a bigpage. */
		assert(!(pt->pt_dir[pde] & ARCH_VM_BIGPAGE));

		/* Make sure page directory entry for this page table
		 * is marked present and page table entry is available.
		 */
		assert(pt->pt_pt[pde]);

#if SANITYCHECKS
		/* We don't expect to overwrite a page. */
		if(!(writemapflags & (WMF_OVERWRITE|WMF_VERIFY)))
			assert(!(pt->pt_pt[pde][pte] & ARCH_VM_PTE_PRESENT));
#endif
		if(writemapflags & (WMF_WRITEFLAGSONLY|WMF_FREE)) {
#if defined(__i386__)
			physaddr = pt->pt_pt[pde][pte] & ARCH_VM_ADDR_MASK;
#elif defined(__arm__)
			physaddr = pt->pt_pt[pde][pte] & ARM_VM_PTE_MASK;
#endif
		}

		if(writemapflags & WMF_FREE) {
			free_mem(ABS2CLICK(physaddr), 1);
		}

		/* Entry we will write. */
#if defined(__i386__)
		entry = (physaddr & ARCH_VM_ADDR_MASK) | flags;
#elif defined(__arm__)
		entry = (physaddr & ARM_VM_PTE_MASK) | flags;
#endif

		if(verify) {
			u32_t maskedentry;
			maskedentry = pt->pt_pt[pde][pte];
#if defined(__i386__)
			maskedentry &= ~(I386_VM_ACC|I386_VM_DIRTY);
#endif
			/* Verify pagetable entry. */
#if defined(__i386__)
			if(entry & ARCH_VM_PTE_RW) {
				/* If we expect a writable page, allow a readonly page. */
				maskedentry |= ARCH_VM_PTE_RW;
			}
#elif defined(__arm__)
			if(!(entry & ARCH_VM_PTE_RO)) {
				/* If we expect a writable page, allow a readonly page. */
				maskedentry &= ~ARCH_VM_PTE_RO;
			}
#endif
			if(maskedentry != entry) {
				printf("pt_writemap: mismatch: ");
#if defined(__i386__)
				if((entry & ARCH_VM_ADDR_MASK) !=
					(maskedentry & ARCH_VM_ADDR_MASK)) {
#elif defined(__arm__)
				if((entry & ARM_VM_PTE_MASK) !=
					(maskedentry & ARM_VM_PTE_MASK)) {
#endif
					printf("pt_writemap: physaddr mismatch (0x%lx, 0x%lx); ",
						(long)entry, (long)maskedentry);
				} else printf("phys ok; ");
				printf(" flags: found %s; ",
					ptestr(pt->pt_pt[pde][pte]));
				printf(" masked %s; ",
					ptestr(maskedentry));
				printf(" expected %s\n", ptestr(entry));
				printf("found 0x%x, wanted 0x%x\n", 
					pt->pt_pt[pde][pte], entry);
				ret = EFAULT;
				goto resume_exit;
			}
		} else {
			/* Write pagetable entry. */
			pt->pt_pt[pde][pte] = entry;
		}

		physaddr += VM_PAGE_SIZE;
		v += VM_PAGE_SIZE;
	}

resume_exit:

#ifdef CONFIG_SMP
	if (vminhibit_clear) {
		assert(vmp && vmp->vm_endpoint != NONE && vmp->vm_endpoint != VM_PROC_NR &&
			!(vmp->vm_flags & VMF_EXITING));
		sys_vmctl(vmp->vm_endpoint, VMCTL_VMINHIBIT_CLEAR, 0);
	}
#endif

	return ret;
}

/*===========================================================================*
 *				pt_checkrange		     		     *
 *===========================================================================*/
int pt_checkrange(pt_t *pt, vir_bytes v,  size_t bytes,
	int write)
{
	int p, pages;

	assert(!(bytes % VM_PAGE_SIZE));

	pages = bytes / VM_PAGE_SIZE;

	for(p = 0; p < pages; p++) {
#if defined(__i386__)
		int pde = I386_VM_PDE(v);
		int pte = I386_VM_PTE(v);
#elif defined(__arm__)
		int pde = ARM_VM_PDE(v);
		int pte = ARM_VM_PTE(v);
#endif

		assert(!(v % VM_PAGE_SIZE));
		assert(pte >= 0 && pte < ARCH_VM_PT_ENTRIES);
		assert(pde >= 0 && pde < ARCH_VM_DIR_ENTRIES);

		/* Page table has to be there. */
		if(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT))
			return EFAULT;

		/* Make sure page directory entry for this page table
		 * is marked present and page table entry is available.
		 */
		assert((pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT) && pt->pt_pt[pde]);

		if(!(pt->pt_pt[pde][pte] & ARCH_VM_PTE_PRESENT)) {
			return EFAULT;
		}

#if defined(__i386__)
		if(write && !(pt->pt_pt[pde][pte] & ARCH_VM_PTE_RW)) {
#elif defined(__arm__)
		if(write && (pt->pt_pt[pde][pte] & ARCH_VM_PTE_RO)) {
#endif
			return EFAULT;
		}

		v += VM_PAGE_SIZE;
	}

	return OK;
}

/*===========================================================================*
 *				pt_new			     		     *
 *===========================================================================*/
int pt_new(pt_t *pt)
{
/* Allocate a pagetable root. Allocate a page-aligned page directory
 * and set them to 0 (indicating no page tables are allocated). Lookup
 * its physical address as we'll need that in the future. Verify it's
 * page-aligned.
 */
	int i, r;

	/* Don't ever re-allocate/re-move a certain process slot's
	 * page directory once it's been created. This is a fraction
	 * faster, but also avoids having to invalidate the page
	 * mappings from in-kernel page tables pointing to
	 * the page directories (the page_directories data).
	 */
        if(!pt->pt_dir &&
          !(pt->pt_dir = vm_allocpages((phys_bytes *)&pt->pt_dir_phys,
	  	VMP_PAGEDIR, ARCH_PAGEDIR_SIZE/VM_PAGE_SIZE))) {
		return ENOMEM;
	}

	assert(!((u32_t)pt->pt_dir_phys % ARCH_PAGEDIR_SIZE));

	for(i = 0; i < ARCH_VM_DIR_ENTRIES; i++) {
		pt->pt_dir[i] = 0; /* invalid entry (PRESENT bit = 0) */
		pt->pt_pt[i] = NULL;
	}

	/* Where to start looking for free virtual address space? */
	pt->pt_virtop = 0;

        /* Map in kernel. */
        if((r=pt_mapkernel(pt)) != OK)
		return r;

	return OK;
}

static int freepde(void)
{
	int p = kernel_boot_info.freepde_start++;
	assert(kernel_boot_info.freepde_start < ARCH_VM_DIR_ENTRIES);
	return p;
}

/*===========================================================================*
 *                              pt_init                                      *
 *===========================================================================*/
void pt_init(void)
{
        pt_t *newpt;
        int s, r, p;
	vir_bytes sparepages_mem;
#if defined(__arm__)
	vir_bytes sparepagedirs_mem;
#endif
	static u32_t currentpagedir[ARCH_VM_DIR_ENTRIES];
	int m = kernel_boot_info.kern_mod;
#if defined(__i386__)
	int global_bit_ok = 0;
	u32_t mypdbr; /* Page Directory Base Register (cr3) value */
#elif defined(__arm__)
	u32_t myttbr;
#endif

	/* Find what the physical location of the kernel is. */
	assert(m >= 0);
	assert(m < kernel_boot_info.mods_with_kernel);
	assert(kernel_boot_info.mods_with_kernel < MULTIBOOT_MAX_MODS);
	kern_mb_mod = &kernel_boot_info.module_list[m];
	kern_size = kern_mb_mod->mod_end - kern_mb_mod->mod_start;
	assert(!(kern_mb_mod->mod_start % ARCH_BIG_PAGE_SIZE));
	assert(!(kernel_boot_info.vir_kern_start % ARCH_BIG_PAGE_SIZE));
	kern_start_pde = kernel_boot_info.vir_kern_start / ARCH_BIG_PAGE_SIZE;

        /* Get ourselves spare pages. */
        sparepages_mem = (vir_bytes) static_sparepages;
	assert(!(sparepages_mem % VM_PAGE_SIZE));

#if defined(__arm__)
        /* Get ourselves spare pagedirs. */
	sparepagedirs_mem = (vir_bytes) static_sparepagedirs;
	assert(!(sparepagedirs_mem % ARCH_PAGEDIR_SIZE));
#endif

	/* Spare pages are used to allocate memory before VM has its own page
	 * table that things (i.e. arbitrary physical memory) can be mapped into.
	 * We get it by pre-allocating it in our bss (allocated and mapped in by
	 * the kernel) in static_sparepages. We also need the physical addresses
	 * though; we look them up now so they are ready for use.
	 */
#if defined(__arm__)
        missing_sparedirs = 0;
        assert(STATIC_SPAREPAGEDIRS <= SPAREPAGEDIRS);
        for(s = 0; s < SPAREPAGEDIRS; s++) {
		vir_bytes v = (sparepagedirs_mem + s*ARCH_PAGEDIR_SIZE);;
		phys_bytes ph;
        	if((r=sys_umap(SELF, VM_D, (vir_bytes) v,
	                ARCH_PAGEDIR_SIZE, &ph)) != OK)
				panic("pt_init: sys_umap failed: %d", r);
        	if(s >= STATIC_SPAREPAGEDIRS) {
        		sparepagedirs[s].pagedir = NULL;
        		missing_sparedirs++;
        		continue;
        	}
        	sparepagedirs[s].pagedir = (void *) v;
        	sparepagedirs[s].phys = ph;
        }
#endif

        missing_spares = 0;
        assert(STATIC_SPAREPAGES < SPAREPAGES);
        for(s = 0; s < SPAREPAGES; s++) {
		vir_bytes v = (sparepages_mem + s*VM_PAGE_SIZE);;
		phys_bytes ph;
		if((r=sys_umap(SELF, VM_D, (vir_bytes) v,
	                VM_PAGE_SIZE*SPAREPAGES, &ph)) != OK)
				panic("pt_init: sys_umap failed: %d", r);
        	if(s >= STATIC_SPAREPAGES) {
        		sparepages[s].page = NULL;
        		missing_spares++;
        		continue;
        	}
        	sparepages[s].page = (void *) v;
        	sparepages[s].phys = ph;
        }

#if defined(__i386__)
	/* global bit and 4MB pages available? */
	global_bit_ok = _cpufeature(_CPUF_I386_PGE);
	bigpage_ok = _cpufeature(_CPUF_I386_PSE);

	/* Set bit for PTE's and PDE's if available. */
	if(global_bit_ok)
		global_bit = I386_VM_GLOBAL;
#endif

	/* Now reserve another pde for kernel's own mappings. */
	{
		int kernmap_pde;
		phys_bytes addr, len;
		int flags, index = 0;
		u32_t offset = 0;

		kernmap_pde = freepde();
		offset = kernmap_pde * ARCH_BIG_PAGE_SIZE;

		while(sys_vmctl_get_mapping(index, &addr, &len,
			&flags) == OK)  {
			int usedpde;
			vir_bytes vir;
			if(index >= MAX_KERNMAPPINGS)
                		panic("VM: too many kernel mappings: %d", index);
			kern_mappings[index].phys_addr = addr;
			kern_mappings[index].len = len;
			kern_mappings[index].flags = flags;
			kern_mappings[index].vir_addr = offset;
			kern_mappings[index].flags =
				ARCH_VM_PTE_PRESENT;
			if(flags & VMMF_UNCACHED)
#if defined(__i386__)
				kern_mappings[index].flags |= PTF_NOCACHE;
#elif defined(__arm__)
				kern_mappings[index].flags |= ARM_VM_PTE_DEVICE;
			else
				kern_mappings[index].flags |=
				    ARM_VM_PTE_WB | ARM_VM_PTE_SHAREABLE;
#endif
			if(flags & VMMF_USER)
				kern_mappings[index].flags |= ARCH_VM_PTE_USER;
#if defined(__arm__)
			else
				kern_mappings[index].flags |= ARM_VM_PTE_SUPER;
#endif
			if(flags & VMMF_WRITE)
				kern_mappings[index].flags |= ARCH_VM_PTE_RW;
#if defined(__i386__)
			if(flags & VMMF_GLO)
				kern_mappings[index].flags |= I386_VM_GLOBAL;
#elif defined(__arm__)
			else
				kern_mappings[index].flags |= ARCH_VM_PTE_RO;
#endif
			if(addr % VM_PAGE_SIZE)
                		panic("VM: addr unaligned: %d", addr);
			if(len % VM_PAGE_SIZE)
                		panic("VM: len unaligned: %d", len);
			vir = offset;
			if(sys_vmctl_reply_mapping(index, vir) != OK)
                		panic("VM: reply failed");
			offset += len;
			index++;
			kernmappings++;

#if defined(__i386__)
			usedpde = I386_VM_PDE(offset);
#elif defined(__arm__)
			usedpde = ARM_VM_PDE(offset);
#endif
			while(usedpde > kernmap_pde) {
				int newpde = freepde();
				assert(newpde == kernmap_pde+1);
				kernmap_pde = newpde;
			}
		}
	}

	/* Reserve PDEs available for mapping in the page directories. */
	{
		int pd;
		for(pd = 0; pd < MAX_PAGEDIR_PDES; pd++) {
			struct pdm *pdm = &pagedir_mappings[pd];
			pdm->pdeno = freepde();
			phys_bytes ph;

			/* Allocate us a page table in which to
			 * remember page directory pointers.
			 */
			if(!(pdm->page_directories =
				vm_allocpage(&ph, VMP_PAGETABLE))) {
				panic("no virt addr for vm mappings");
			}
			memset(pdm->page_directories, 0, VM_PAGE_SIZE);
			pdm->phys = ph;

#if defined(__i386__)
			pdm->val = (ph & ARCH_VM_ADDR_MASK) |
				ARCH_VM_PDE_PRESENT | ARCH_VM_PTE_RW;
#elif defined(__arm__)
			pdm->val = (ph & ARCH_VM_PDE_MASK) |
				ARCH_VM_PDE_PRESENT | ARM_VM_PDE_DOMAIN;
#endif
		}
	}

	/* Allright. Now. We have to make our own page directory and page tables,
	 * that the kernel has already set up, accessible to us. It's easier to
	 * understand if we just copy all the required pages (i.e. page directory
	 * and page tables), and set up the pointers as if VM had done it itself.
	 *
	 * This allocation will happen without using any page table, and just
	 * uses spare pages.
	 */
        newpt = &vmprocess->vm_pt;
	if(pt_new(newpt) != OK)
		panic("vm pt_new failed");

	/* Get our current pagedir so we can see it. */
#if defined(__i386__)
	if(sys_vmctl_get_pdbr(SELF, &mypdbr) != OK)
#elif defined(__arm__)
	if(sys_vmctl_get_pdbr(SELF, &myttbr) != OK)
#endif
		panic("VM: sys_vmctl_get_pdbr failed");
#if defined(__i386__)
	if(sys_vircopy(NONE, mypdbr, SELF,
		(vir_bytes) currentpagedir, VM_PAGE_SIZE) != OK)
#elif defined(__arm__)
	if(sys_vircopy(NONE, myttbr, SELF,
		(vir_bytes) currentpagedir, ARCH_PAGEDIR_SIZE) != OK)
#endif
		panic("VM: sys_vircopy failed");

	/* We have mapped in kernel ourselves; now copy mappings for VM
	 * that kernel made, including allocations for BSS. Skip identity
	 * mapping bits; just map in VM.
	 */
	for(p = 0; p < ARCH_VM_DIR_ENTRIES; p++) {
		u32_t entry = currentpagedir[p];
		phys_bytes ptaddr_kern, ptaddr_us;

		/* BIGPAGEs are kernel mapping (do ourselves) or boot
		 * identity mapping (don't want).
		 */
		if(!(entry & ARCH_VM_PDE_PRESENT)) continue;
		if((entry & ARCH_VM_BIGPAGE)) continue;

		if(pt_ptalloc(newpt, p, 0) != OK)
			panic("pt_ptalloc failed");
		assert(newpt->pt_dir[p] & ARCH_VM_PDE_PRESENT);

#if defined(__i386__)
		ptaddr_kern = entry & ARCH_VM_ADDR_MASK;
		ptaddr_us = newpt->pt_dir[p] & ARCH_VM_ADDR_MASK;
#elif defined(__arm__)
		ptaddr_kern = entry & ARCH_VM_PDE_MASK;
		ptaddr_us = newpt->pt_dir[p] & ARCH_VM_PDE_MASK;
#endif

		/* Copy kernel-initialized pagetable contents into our
		 * normally accessible pagetable.
		 */
                if(sys_abscopy(ptaddr_kern, ptaddr_us, VM_PAGE_SIZE) != OK)
			panic("pt_init: abscopy failed");
	}

	/* Inform kernel vm has a newly built page table. */
	assert(vmproc[VM_PROC_NR].vm_endpoint == VM_PROC_NR);
	pt_bind(newpt, &vmproc[VM_PROC_NR]);

	pt_init_done = 1;

	vm_checkspares();

        /* All OK. */
        return;
}

/*===========================================================================*
 *				pt_bind			     		     *
 *===========================================================================*/
int pt_bind(pt_t *pt, struct vmproc *who)
{
	int procslot, pdeslot;
	u32_t phys;
	void *pdes;
	int pagedir_pde;
	int slots_per_pde;
	int pages_per_pagedir = ARCH_PAGEDIR_SIZE/VM_PAGE_SIZE;
	struct pdm *pdm;

	slots_per_pde = ARCH_VM_PT_ENTRIES / pages_per_pagedir;

	/* Basic sanity checks. */
	assert(who);
	assert(who->vm_flags & VMF_INUSE);
	assert(pt);

	procslot = who->vm_slot;
	pdm = &pagedir_mappings[procslot/slots_per_pde];
	pdeslot = procslot%slots_per_pde;
	pagedir_pde = pdm->pdeno;
	assert(pdeslot >= 0);
	assert(procslot < ELEMENTS(vmproc));
	assert(pdeslot < ARCH_VM_PT_ENTRIES / pages_per_pagedir);
	assert(pagedir_pde >= 0);

#if defined(__i386__)
	phys = pt->pt_dir_phys & ARCH_VM_ADDR_MASK;
#elif defined(__arm__)
	phys = pt->pt_dir_phys & ARM_VM_PTE_MASK;
#endif
	assert(pt->pt_dir_phys == phys);
	assert(!(pt->pt_dir_phys % ARCH_PAGEDIR_SIZE));

	/* Update "page directory pagetable." */
#if defined(__i386__)
	pdm->page_directories[pdeslot] =
		phys | ARCH_VM_PDE_PRESENT|ARCH_VM_PTE_RW;
#elif defined(__arm__)
{
	int i;
	for (i = 0; i < pages_per_pagedir; i++) {
		pdm->page_directories[pdeslot*pages_per_pagedir+i] =
			(phys+i*VM_PAGE_SIZE) |
			ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_RW |
			ARCH_VM_PTE_USER;
	}
}
#endif

	/* This is where the PDE's will be visible to the kernel
	 * in its address space.
	 */
	pdes = (void *) (pagedir_pde*ARCH_BIG_PAGE_SIZE + 
#if defined(__i386__)
			pdeslot * VM_PAGE_SIZE);
#elif defined(__arm__)
			pdeslot * ARCH_PAGEDIR_SIZE);
#endif

#if 0
	printf("VM: slot %d endpoint %d has pde val 0x%lx at kernel address 0x%lx\n",
		slot, who->vm_endpoint, page_directories[slot], pdes);
#endif
	/* Tell kernel about new page table root. */
	return sys_vmctl_set_addrspace(who->vm_endpoint, pt->pt_dir_phys, pdes);
}

/*===========================================================================*
 *				pt_free			     		     *
 *===========================================================================*/
void pt_free(pt_t *pt)
{
/* Free memory associated with this pagetable. */
	int i;

	for(i = 0; i < ARCH_VM_DIR_ENTRIES; i++)
		if(pt->pt_pt[i])
			vm_freepages((vir_bytes) pt->pt_pt[i], 1);

	return;
}

/*===========================================================================*
 *				pt_mapkernel		     		     *
 *===========================================================================*/
int pt_mapkernel(pt_t *pt)
{
	int i;
	int kern_pde = kern_start_pde;
	phys_bytes addr, mapped = 0;

        /* Any page table needs to map in the kernel address space. */
	assert(bigpage_ok);
	assert(kern_pde >= 0);

	/* pt_init() has made sure this is ok. */
	addr = kern_mb_mod->mod_start;

	/* Actually mapping in kernel */
	while(mapped < kern_size) {
#if defined(__i386__)
		pt->pt_dir[kern_pde] = addr | ARCH_VM_PDE_PRESENT |
			ARCH_VM_BIGPAGE | ARCH_VM_PTE_RW | global_bit;
#elif defined(__arm__)
		pt->pt_dir[kern_pde] = (addr & ARCH_VM_PDE_MASK) |
			ARM_VM_SECTION |
			ARM_VM_SECTION_DOMAIN | ARM_VM_SECTION_WB |
			ARM_VM_SECTION_SHAREABLE | ARM_VM_SECTION_SUPER;
#endif
		kern_pde++;
		mapped += ARCH_BIG_PAGE_SIZE;
		addr += ARCH_BIG_PAGE_SIZE;
	}

	/* Kernel also wants to know about all page directories. */
	{
		int pd;
		for(pd = 0; pd < MAX_PAGEDIR_PDES; pd++) {
			struct pdm *pdm = &pagedir_mappings[pd];
			
			assert(pdm->pdeno > 0);
			assert(pdm->pdeno > kern_pde);
			pt->pt_dir[pdm->pdeno] = pdm->val;
		}
	}

	/* Kernel also wants various mappings of its own. */
	for(i = 0; i < kernmappings; i++) {
		int r;
		if((r=pt_writemap(NULL, pt,
			kern_mappings[i].vir_addr,
			kern_mappings[i].phys_addr,
			kern_mappings[i].len,
			kern_mappings[i].flags, 0)) != OK) {
			return r;
		}

#if defined(__arm__)
		if(kern_mappings[i].phys_addr == 0x48000000) {
			if((r=pt_writemap(NULL, pt,
				kern_mappings[i].phys_addr,
				kern_mappings[i].phys_addr,
				kern_mappings[i].len,
				kern_mappings[i].flags, 0)) != OK) {
				return r;
			}
		}
#endif
	}

	return OK;
}

/*===========================================================================*
 *				pt_cycle		     		     *
 *===========================================================================*/
void pt_cycle(void)
{
	vm_checkspares();
}

int get_vm_self_pages(void) { return vm_self_pages; }
