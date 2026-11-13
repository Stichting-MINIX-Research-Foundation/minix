
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
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

static kinfo_module_t *kern_mb_mod = NULL;
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
#define SPAREPAGES 200
#define STATIC_SPAREPAGES 190
#else
#ifdef __arm__
# define SPAREPAGES 150
# define STATIC_SPAREPAGES 140
#elif defined(__x86_64__)
/* x86_64 has 4-level page tables: mapping a region can allocate a PDPT, PD,
 * PT-pointer page and PT (up to 4 pages) versus i386's single PT, so the
 * bootstrap spare pool drains much faster while VM brings up its dynamic
 * allocator.  Use a larger static pool. */
# define SPAREPAGES 200
# define STATIC_SPAREPAGES 190
#else
# define SPAREPAGES 20
# define STATIC_SPAREPAGES 15
#endif /* __arm__ */
#endif

#if defined(__i386__) || defined(__x86_64__)
static u64_t global_bit = 0;
#endif

#ifdef __x86_64__
/* Kernel's PML4[511] entry — wired into every new process's PML4 by
 * pt_mapkernel().  Set once during pt_init() from VM's own PML4. */
static u64_t kern_pml4_hi = 0;
#endif

#define SPAREPAGEDIRS 1
#define STATIC_SPAREPAGEDIRS 1

int missing_sparedirs = SPAREPAGEDIRS;
static struct {
	void *pagedir;
	phys_bytes phys;
} sparepagedirs[SPAREPAGEDIRS];

#define is_staticaddr(v) ((vir_bytes) (v) < VM_OWN_HEAPSTART)

#define MAX_KERNMAPPINGS 10
static struct {
	phys_bytes	phys_addr;	/* Physical addr. */
	phys_bytes	len;		/* Length in bytes. */
	vir_bytes	vir_addr;	/* Offset in page table. */
	u64_t		flags;
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

static void *spare_pagequeue;
static char static_sparepages[VM_PAGE_SIZE*STATIC_SPAREPAGES] 
	__aligned(VM_PAGE_SIZE);

#if defined(__arm__)
static char static_sparepagedirs[ARCH_PAGEDIR_SIZE*STATIC_SPAREPAGEDIRS + ARCH_PAGEDIR_SIZE] __aligned(ARCH_PAGEDIR_SIZE);
#endif

void pt_assert(pt_t *pt)
{
#ifdef __x86_64__
	char pml4[4096];
	pt_clearmapcache();
	if((sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("VMCTL_FLUSHTLB failed");
	}
	sys_physcopy(NONE, pt->pt_pml4_phys, SELF, (vir_bytes) pml4, sizeof(pml4), 0);
	assert(!memcmp(pml4, pt->pt_pml4, sizeof(pml4)));
#else
	char dir[4096];
	pt_clearmapcache();
	if((sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("VMCTL_FLUSHTLB failed");
	}
	sys_physcopy(NONE, pt->pt_dir_phys, SELF, (vir_bytes) dir, sizeof(dir), 0);
	assert(!memcmp(dir, pt->pt_dir, sizeof(dir)));
#endif
}

#if SANITYCHECKS
/*===========================================================================*
 *				pt_sanitycheck		     		     *
 *===========================================================================*/
void pt_sanitycheck(pt_t *pt, const char *file, int line)
{
/* Basic pt sanity check. */
	int slot;

	MYASSERT(pt);
#ifdef __x86_64__
	MYASSERT(pt->pt_pml4);
	MYASSERT(pt->pt_pml4_phys);
#else
	MYASSERT(pt->pt_dir);
	MYASSERT(pt->pt_dir_phys);
#endif

	for(slot = 0; slot < ELEMENTS(vmproc); slot++) {
		if(pt == &vmproc[slot].vm_pt)
			break;
	}

	if(slot >= ELEMENTS(vmproc)) {
		panic("pt_sanitycheck: passed pt not in any proc");
	}

#ifdef __x86_64__
	MYASSERT(usedpages_add(pt->pt_pml4_phys, VM_PAGE_SIZE) == OK);
#else
	MYASSERT(usedpages_add(pt->pt_dir_phys, VM_PAGE_SIZE) == OK);
#endif
}
#endif

/*===========================================================================*
 *				findhole		     		     *
 *===========================================================================*/
static vir_bytes findhole(int pages)
{
/* Find a space in the virtual address space of VM. */
	vir_bytes curv;
	int try_restart;
	static void *lastv = 0;
	pt_t *pt = &vmprocess->vm_pt;
	vir_bytes vmin, vmax;
	vir_bytes holev = NO_MEM;
	int holesize = -1;

	vmin = VM_OWN_MMAPBASE;
	vmax = VM_OWN_MMAPTOP;

	/* Input sanity check. */
	assert(vmin + VM_PAGE_SIZE >= vmin);
	assert(vmax >= vmin + VM_PAGE_SIZE);
	assert((vmin % VM_PAGE_SIZE) == 0);
	assert((vmax % VM_PAGE_SIZE) == 0);
	assert(pages > 0);

	curv = (vir_bytes) lastv;
	if(curv < vmin || curv >= vmax)
		curv = vmin;

	try_restart = 1;

	/* Start looking for a free page starting at vmin. */
	while(curv < vmax) {
		int occupied;

		assert(curv >= vmin);
		assert(curv < vmax);

#ifdef __x86_64__
		{
			int pdpte = ARCH_VM_PDPTE(curv);
			int pde   = ARCH_VM_PDE(curv);
			int pte   = ARCH_VM_PTE(curv);
			occupied =
			    pt->pt_pd[pdpte] &&
			    (pt->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT) &&
			    pt->pt_pt[pdpte] &&
			    pt->pt_pt[pdpte][pde] &&
			    (pt->pt_pt[pdpte][pde][pte] & ARCH_VM_PTE_PRESENT);
		}
#else
		{
			int pde = ARCH_VM_PDE(curv);
			int pte = ARCH_VM_PTE(curv);
			occupied =
			    (pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT) &&
			    (pt->pt_pt[pde][pte] & ARCH_VM_PTE_PRESENT);
		}
#endif

		if(occupied) {
			/* there is a page here - so keep looking for holes */
			holev = NO_MEM;
			holesize = 0;
		} else {
			/* there is no page here - so we have a hole, a bigger
			 * one if we already had one
			 */
			if(holev == NO_MEM) {
				holev = curv;
				holesize = 1;
			} else holesize++;

			assert(holesize > 0);
			assert(holesize <= pages);

			/* if it's big enough, return it */
			if(holesize == pages) {
				lastv = (void*) (curv + VM_PAGE_SIZE);
				return holev;
			}
		}

		curv+=VM_PAGE_SIZE;

		/* if we reached the limit, start scanning from the beginning if
		 * we haven't looked there yet
		 */
		if(curv >= vmax && try_restart) {
			try_restart = 0;
			curv = vmin;
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
	void *ptr;
	if(reservedqueue_alloc(spare_pagequeue, phys, &ptr) != OK) {
		return NULL;
	}
	assert(ptr);
	return ptr;
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

void *vm_mappages(phys_bytes p, int pages)
{
	vir_bytes loc;
	int r;
	pt_t *pt = &vmprocess->vm_pt;

	/* Where in our virtual address space can we put it? */
	loc = findhole(pages);
	if(loc == NO_MEM) {
		printf("vm_mappages: findhole failed\n");
		return NULL;
	}

	/* Map this page into our address space. */
	if((r=pt_writemap(vmprocess, pt, loc, p, VM_PAGE_SIZE*pages,
		ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW
#if defined(__arm__)
		| ARM_VM_PTE_CACHED
#endif
		, 0)) != OK) {
		printf("vm_mappages writemap failed\n");
		return NULL;
	}

	if((r=sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("VMCTL_FLUSHTLB failed: %d", r);
	}

	assert(loc);

	return (void *) loc;
}

static int pt_init_done;

/*===========================================================================*
 *				vm_allocpage		     		     *
 *===========================================================================*/
void *vm_allocpages(phys_bytes *phys, int reason, int pages)
{
/* Allocate a page for use by VM itself. */
	phys_bytes newpage;
	static int level = 0;
	void *ret;
	u32_t mem_flags = 0;

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

	/* Allocate page of memory for use by VM. As VM
	 * is trusted, we don't have to pre-clear it.
	 */
	if((newpage = alloc_mem(pages, mem_flags)) == NO_MEM) {
		level--;
		printf("VM: vm_allocpage: alloc_mem failed\n");
		return NULL;
	}

	*phys = CLICK2ABS(newpage);

	if(!(ret = vm_mappages(*phys, pages))) {
		level--;
		printf("VM: vm_allocpage: vm_mappages failed\n");
		return NULL;
	}

	level--;
	vm_self_pages++;

	return ret;
}

void *vm_allocpage(phys_bytes *phys, int reason)
{
	return vm_allocpages(phys, reason, 1);
}

void *vm_allocpagedir(phys_bytes *phys)
{
	return vm_allocpages(phys, VMP_PAGEDIR, ARCH_PAGEDIR_SIZE/VM_PAGE_SIZE);
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

	flags |= ARM_VM_PTE_CACHED ;
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

	pde = ARCH_VM_PDE(v);
	pte = ARCH_VM_PTE(v);

#ifdef __x86_64__
	{
		int pdpte = ARCH_VM_PDPTE(v);
		if(!pt->pt_pd[pdpte] ||
		   !(pt->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT)) {
			printf("addr not ok: missing pdpte %d pde %d\n", pdpte, pde);
			return 0;
		}
		if(writeflag && !(pt->pt_pd[pdpte][pde] & ARCH_VM_PTE_RW)) {
			printf("addr not ok: pdpte %d pde %d present but unwritable\n",
				pdpte, pde);
			return 0;
		}
		if(!pt->pt_pt[pdpte] || !pt->pt_pt[pdpte][pde] ||
		   !(pt->pt_pt[pdpte][pde][pte] & ARCH_VM_PTE_PRESENT)) {
			printf("addr not ok: missing pdpte %d pde %d pte %d\n",
				pdpte, pde, pte);
			return 0;
		}
		if(writeflag && !(pt->pt_pt[pdpte][pde][pte] & ARCH_VM_PTE_RW)) {
			printf("addr not ok: pdpte %d pde %d pte %d present but unwritable\n",
				pdpte, pde, pte);
			return 0;
		}
	}
#else
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
#elif defined(__arm__)
	if(writeflag &&
		 (pt->pt_dir[pde] & ARCH_VM_PTE_RO)) {
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
			pde, pte);
#elif defined(__arm__)
	if(writeflag &&
		 (pt->pt_pt[pde][pte] & ARCH_VM_PTE_RO)) {
		printf("addr not ok: pde %d / pte %d present but unwritable\n",
			pde, pte);
#endif
		return 0;
	}
#endif /* __x86_64__ */

	return 1;
}

/*===========================================================================*
 *				pt_ptalloc		     		     *
 *===========================================================================*/
#ifdef __x86_64__
/* Allocate a PT page (and PD page if needed) for the given (pdpte, pde) slot. */
static int pt_ptalloc(pt_t *pt, int pdpte, int pde, u64_t flags)
{
	int i;
	phys_bytes pt_phys, pd_phys;
	u64_t *p;

	assert(pdpte >= 0 && pdpte < ARCH_PDPT_ENTRIES);
	assert(pde   >= 0 && pde   < ARCH_VM_DIR_ENTRIES);
	assert(!(flags & ~(PTF_ALLFLAGS)));

	/* Allocate PD page for this PDPT entry if not yet present.
	 * vm_allocpagedir()/vm_allocpage() below map a page, which can re-enter
	 * pt_ptalloc() for this same pdpte (recursive allocation).  If the
	 * recursion already populated pt_pd[pdpte]/pt_pt[pdpte], we must keep its
	 * version and free ours — otherwise overwriting it with a freshly-zeroed
	 * page loses the recursion's PDE/PT-pointer pairing and corrupts the
	 * pt_pd vs pt_pt consistency the asserts below rely on. */
	if(!pt->pt_pd[pdpte]) {
		u64_t *pd;
		if(!(pd = vm_allocpagedir(&pd_phys)))
			return ENOMEM;
		if(pt->pt_pd[pdpte]) {
			vm_freepages((vir_bytes)pd, 1);
		} else {
			memset(pd, 0, VM_PAGE_SIZE);
			pt->pt_pdpt[pdpte] = (pd_phys & ARCH_VM_ADDR_MASK) |
				ARCH_VM_PDE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW;
			pt->pt_pd[pdpte] = pd;
		}
	}

	/* Allocate the PT-pointer array for this PDPT entry if not yet present.
	 * One page holds 512 u64_t* pointers = exactly ARCH_VM_DIR_ENTRIES. */
	if(!pt->pt_pt[pdpte]) {
		phys_bytes dummy;
		u64_t **arr;
		if(!(arr = (u64_t **)vm_allocpage(&dummy, VMP_PAGETABLE)))
			return ENOMEM;
		if(pt->pt_pt[pdpte]) {
			vm_freepages((vir_bytes)arr, 1);
		} else {
			memset(arr, 0, VM_PAGE_SIZE);
			pt->pt_pt[pdpte] = arr;
		}
	}

	/* Allocating the PD/pointer-array pages above (or the PT page below) maps
	 * memory, which can recursively re-enter pt_ptalloc() for this exact
	 * (pdpte,pde) slot and fully create it.  If so, we're done — don't
	 * re-allocate (that would trip the asserts / leak). */
	if((pt->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT) || pt->pt_pt[pdpte][pde]) {
		assert((pt->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT) &&
			pt->pt_pt[pdpte][pde]);
		return OK;
	}

	/* Allocate the PT page itself. */
	if(!(p = (u64_t *)vm_allocpage(&pt_phys, VMP_PAGETABLE)))
		return ENOMEM;
	if(pt->pt_pt[pdpte][pde]) {
		/* Recursive allocation created it as a side-effect. */
		vm_freepages((vir_bytes)p, 1);
		return OK;
	}
	pt->pt_pt[pdpte][pde] = p;

	for(i = 0; i < ARCH_VM_PT_ENTRIES; i++)
		p[i] = 0;

	/* Write PDE: present, writable, user, pointing to PT page.
	 * PTF_NOEXEC must not propagate to the PDE — a PDE with NX=1 blocks
	 * execution for every page in the 2 MB region, even those with NX=0
	 * in their PTE.  NX is enforced per-page at the PTE level. */
	pt->pt_pd[pdpte][pde] = (pt_phys & ARCH_VM_ADDR_MASK) |
		((u64_t)flags & ~PTF_NOEXEC) |
		ARCH_VM_PDE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW;

	return OK;
}
#else  /* i386 / earm */
static int pt_ptalloc(pt_t *pt, int pde, u32_t flags)
{
/* Allocate a page table and write its address into the page directory. */
	int i;
	phys_bytes pt_phys;
	u32_t *p;

	/* Argument must make sense. */
	assert(pde >= 0 && pde < ARCH_VM_DIR_ENTRIES);
	assert(!(flags & ~(PTF_ALLFLAGS)));

	/* We don't expect to overwrite page directory entry, nor
	 * storage for the page table.
	 */
	assert(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT));
	assert(!pt->pt_pt[pde]);

	/* Get storage for the page table. The allocation call may in fact
	 * recursively create the directory entry as a side effect. In that
	 * case, we free the newly allocated page and do nothing else.
	 */
	if (!(p = vm_allocpage(&pt_phys, VMP_PAGETABLE)))
		return ENOMEM;
	if (pt->pt_pt[pde]) {
		vm_freepages((vir_bytes) p, 1);
		assert(pt->pt_pt[pde]);
		return OK;
	}
	pt->pt_pt[pde] = p;

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
		| ARCH_VM_PDE_PRESENT | ARM_VM_PDE_DOMAIN; //LSC FIXME
#endif

	return OK;
}
#endif /* __x86_64__ */

/*===========================================================================*
 *			    pt_ptalloc_in_range		     		     *
 *===========================================================================*/
int pt_ptalloc_in_range(pt_t *pt, vir_bytes start, vir_bytes end,
	u64_t flags, int verify)
{
/* Allocate all the page tables in the range specified. */
#ifdef __x86_64__
	int pdpte, pde;
	int first_pdpte = ARCH_VM_PDPTE(start);
	int last_pdpte  = ARCH_VM_PDPTE(end - 1);

	for(pdpte = first_pdpte; pdpte <= last_pdpte; pdpte++) {
		int first_pde_in_pdpte = (pdpte == first_pdpte) ? ARCH_VM_PDE(start) : 0;
		int last_pde_in_pdpte  = (pdpte == last_pdpte)  ? ARCH_VM_PDE(end - 1)
		                                                  : ARCH_VM_DIR_ENTRIES - 1;
		for(pde = first_pde_in_pdpte; pde <= last_pde_in_pdpte; pde++) {
			int r;
			if(pt->pt_pd[pdpte] &&
			   (pt->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT)) {
				assert(pt->pt_pt[pdpte] && pt->pt_pt[pdpte][pde]);
				continue;
			}
			if(verify) {
				printf("pt_ptalloc_in_range: no pde %d/%d\n", pdpte, pde);
				return EFAULT;
			}
			if((r = pt_ptalloc(pt, pdpte, pde, flags)) != OK)
				return r;
			assert(pt->pt_pt[pdpte] && pt->pt_pt[pdpte][pde]);
		}
	}
	return OK;
#else
	int pde, first_pde, last_pde;

	first_pde = ARCH_VM_PDE(start);
	last_pde = ARCH_VM_PDE(end-1);

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
			if((r=pt_ptalloc(pt, pde, (u32_t)flags)) != OK) {
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
#endif
}

#ifdef __x86_64__
static const char *ptestr(u64_t pte)
{
#define FLAG(constant, name) {						\
	if(pte & (u64_t)(constant)) { strcat(str, name); strcat(str, " "); }	\
}
	static char str[40];
	if(!(pte & AMD64_VM_PRESENT)) {
		return "not present";
	}
	str[0] = '\0';
	FLAG(AMD64_VM_WRITE,   "W");
	FLAG(AMD64_VM_USER,    "U");
	FLAG(AMD64_VM_PWT,     "PWT");
	FLAG(AMD64_VM_PCD,     "PCD");
	FLAG(AMD64_VM_PS,      "PS");
	FLAG(AMD64_VM_GLOBAL,  "G");
	FLAG(AMD64_VM_NX,      "NX");
	return str;
}
#else
static const char *ptestr(u32_t pte)
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
	FLAG(ARM_VM_PTE_S, "SH");
	FLAG(ARM_VM_PTE_WB, "WB");
	FLAG(ARM_VM_PTE_WT, "WT");
#endif

	return str;
}
#endif /* __x86_64__ */

/*===========================================================================*
 *			     pt_map_in_range		     		     *
 *===========================================================================*/
int pt_map_in_range(struct vmproc *src_vmp, struct vmproc *dst_vmp,
	vir_bytes start, vir_bytes end)
{
/* Transfer all the mappings from the pt of the source process to the pt of
 * the destination process in the range specified.
 */
	vir_bytes viraddr;
	pt_t *pt, *dst_pt;

	pt = &src_vmp->vm_pt;
	dst_pt = &dst_vmp->vm_pt;

	end = end ? end : VM_DATATOP;
	assert(start % VM_PAGE_SIZE == 0);
	assert(end % VM_PAGE_SIZE == 0);
	assert(start <= end);

#if LU_DEBUG
	printf("VM: pt_map_in_range: src = %d, dst = %d\n",
		src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif

	/* Scan all page-table entries in the range. */
	for(viraddr = start; viraddr <= end; viraddr += VM_PAGE_SIZE) {
#ifdef __x86_64__
		int pdpte = ARCH_VM_PDPTE(viraddr);
		int pde   = ARCH_VM_PDE(viraddr);
		int pte   = ARCH_VM_PTE(viraddr);
		if(!pt->pt_pd[pdpte] ||
		   !(pt->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT)) {
			if(viraddr == VM_DATATOP) break;
			continue;
		}
		if(!pt->pt_pt[pdpte] || !pt->pt_pt[pdpte][pde] ||
		   !(pt->pt_pt[pdpte][pde][pte] & ARCH_VM_PTE_PRESENT)) {
			if(viraddr == VM_DATATOP) break;
			continue;
		}
		dst_pt->pt_pt[pdpte][pde][pte] = pt->pt_pt[pdpte][pde][pte];
		assert(dst_pt->pt_pt[pdpte] && dst_pt->pt_pt[pdpte][pde]);
#else
		int pde = ARCH_VM_PDE(viraddr);
		int pte = ARCH_VM_PTE(viraddr);
		if(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT)) {
			if(viraddr == VM_DATATOP) break;
			continue;
		}
		if(!(pt->pt_pt[pde][pte] & ARCH_VM_PTE_PRESENT)) {
			if(viraddr == VM_DATATOP) break;
			continue;
		}
		dst_pt->pt_pt[pde][pte] = pt->pt_pt[pde][pte];
		assert(dst_pt->pt_pt[pde]);
#endif
		if(viraddr == VM_DATATOP) break;
	}

	return OK;
}

/*===========================================================================*
 *				pt_ptmap		     		     *
 *===========================================================================*/
int pt_ptmap(struct vmproc *src_vmp, struct vmproc *dst_vmp)
{
/* Transfer mappings to page tables from source to destination process. */
	int r;
	phys_bytes physaddr;
	vir_bytes viraddr;
	pt_t *pt;

	pt = &src_vmp->vm_pt;

#if LU_DEBUG
	printf("VM: pt_ptmap: src = %d, dst = %d\n",
		src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif

#ifdef __x86_64__
	/* Map PML4 page. */
	viraddr  = (vir_bytes) pt->pt_pml4;
	physaddr = pt->pt_pml4_phys & ARCH_VM_ADDR_MASK;
	if((r = pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr, physaddr,
		VM_PAGE_SIZE,
		ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW,
		WMF_OVERWRITE)) != OK)
		return r;

	/* Map PDPT page. */
	viraddr  = (vir_bytes) pt->pt_pdpt;
	physaddr = pt->pt_pml4[0] & ARCH_VM_ADDR_MASK;
	if((r = pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr, physaddr,
		VM_PAGE_SIZE,
		ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW,
		WMF_OVERWRITE)) != OK)
		return r;

	/* Map each PD page and its PT pages. */
	{
		int pdpte, pde;
		for(pdpte = 0; pdpte < ARCH_PDPT_ENTRIES; pdpte++) {
			if(!pt->pt_pd[pdpte]) continue;
			viraddr  = (vir_bytes) pt->pt_pd[pdpte];
			physaddr = pt->pt_pdpt[pdpte] & ARCH_VM_ADDR_MASK;
			if((r = pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr,
				physaddr, VM_PAGE_SIZE,
				ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW,
				WMF_OVERWRITE)) != OK)
				return r;
			if(!pt->pt_pt[pdpte]) continue;
			for(pde = 0; pde < ARCH_VM_DIR_ENTRIES; pde++) {
				if(!pt->pt_pt[pdpte][pde]) continue;
				viraddr  = (vir_bytes) pt->pt_pt[pdpte][pde];
				physaddr = pt->pt_pd[pdpte][pde] & ARCH_VM_ADDR_MASK;
				if((r = pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr,
					physaddr, VM_PAGE_SIZE,
					ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW,
					WMF_OVERWRITE)) != OK)
					return r;
			}
		}
	}
#else /* i386 / earm */
	/* Transfer mapping to the page directory. */
	viraddr = (vir_bytes) pt->pt_dir;
	physaddr = pt->pt_dir_phys & ARCH_VM_ADDR_MASK;
#if defined(__i386__)
	if((r=pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr, physaddr, VM_PAGE_SIZE,
		ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW,
#elif defined(__arm__)
	if((r=pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr, physaddr, ARCH_PAGEDIR_SIZE,
		ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER |
		ARM_VM_PTE_CACHED ,
#endif
		WMF_OVERWRITE)) != OK) {
		return r;
	}
#if LU_DEBUG
	printf("VM: pt_ptmap: transferred mapping to page dir: 0x%08x (0x%08x)\n",
		viraddr, physaddr);
#endif

	/* Scan all non-reserved page-directory entries. */
	for(int pde=0; pde < kern_start_pde; pde++) {
		if(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT)) {
			continue;
		}

		if(!pt->pt_pt[pde]) { panic("pde %d empty\n", pde); }

		/* Transfer mapping to the page table. */
		viraddr = (vir_bytes) pt->pt_pt[pde];
#if defined(__i386__)
		physaddr = pt->pt_dir[pde] & ARCH_VM_ADDR_MASK;
#elif defined(__arm__)
		physaddr = pt->pt_dir[pde] & ARCH_VM_PDE_MASK;
#endif
		assert(viraddr);
		if((r=pt_writemap(dst_vmp, &dst_vmp->vm_pt, viraddr, physaddr, VM_PAGE_SIZE,
			ARCH_VM_PTE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW
#ifdef __arm__
			| ARM_VM_PTE_CACHED
#endif
			,
			WMF_OVERWRITE)) != OK) {
			return r;
		}
	}
#endif /* __x86_64__ */

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

int pt_writable(struct vmproc *vmp, vir_bytes v)
{
	pt_t *pt = &vmp->vm_pt;
	assert(!(v % VM_PAGE_SIZE));
#ifdef __x86_64__
	int pdpte = ARCH_VM_PDPTE(v);
	int pde   = ARCH_VM_PDE(v);
	int pte   = ARCH_VM_PTE(v);
	u64_t entry;

	assert(pt->pt_pd[pdpte] && (pt->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT));
	assert(pt->pt_pt[pdpte] && pt->pt_pt[pdpte][pde]);
	entry = pt->pt_pt[pdpte][pde][pte];
	return (entry & AMD64_VM_WRITE) ? 1 : 0;
#else
	int pde = ARCH_VM_PDE(v);
	int pte = ARCH_VM_PTE(v);
	u32_t entry;

	assert(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT);
	assert(pt->pt_pt[pde]);
	entry = pt->pt_pt[pde][pte];
#if defined(__i386__)
	return((entry & PTF_WRITE) ? 1 : 0);
#elif defined(__arm__)
	return((entry & ARCH_VM_PTE_RO) ? 0 : 1);
#endif
#endif /* __x86_64__ */
}

/*===========================================================================*
 *				pt_writemap		     		     *
 *===========================================================================*/
int pt_writemap(struct vmproc * vmp,
			pt_t *pt,
			vir_bytes v,
			phys_bytes physaddr,
			size_t bytes,
			u64_t flags,
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
		assert(!(v % VM_PAGE_SIZE));

#ifdef __x86_64__
		{
		int pdpte = ARCH_VM_PDPTE(v);
		int pde   = ARCH_VM_PDE(v);
		int pte   = ARCH_VM_PTE(v);
		u64_t entry;

		assert(pte >= 0 && pte < ARCH_VM_PT_ENTRIES);
		assert(pde >= 0 && pde < ARCH_VM_DIR_ENTRIES);
		assert(pdpte >= 0 && pdpte < ARCH_PDPT_ENTRIES);
		assert(pt->pt_pd[pdpte] && (pt->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT));
		assert(pt->pt_pt[pdpte] && pt->pt_pt[pdpte][pde]);

		if(writemapflags & (WMF_WRITEFLAGSONLY|WMF_FREE))
			physaddr = pt->pt_pt[pdpte][pde][pte] & ARCH_VM_ADDR_MASK;
		if(writemapflags & WMF_FREE)
			free_mem(ABS2CLICK(physaddr), 1);

		entry = (u64_t)(physaddr & ARCH_VM_ADDR_MASK) | (u64_t)flags;

		if(verify) {
			u64_t maskedentry = pt->pt_pt[pdpte][pde][pte];
			maskedentry &= ~((u64_t)(AMD64_VM_ACC | AMD64_VM_DIRTY));
			if(entry & ARCH_VM_PTE_RW)
				maskedentry |= ARCH_VM_PTE_RW;
			if(maskedentry != entry) {
				printf("pt_writemap: mismatch: ");
				if((entry & ARCH_VM_ADDR_MASK) !=
				   (maskedentry & ARCH_VM_ADDR_MASK))
					printf("physaddr mismatch; ");
				else printf("phys ok; ");
				printf("flags: found %s; ", ptestr(pt->pt_pt[pdpte][pde][pte]));
				printf("masked %s; ", ptestr(maskedentry));
				printf("expected %s\n", ptestr(entry));
				ret = EFAULT;
				goto resume_exit;
			}
		} else {
			pt->pt_pt[pdpte][pde][pte] = entry;
		}
		}
#else /* i386 / earm */
		{
		int pde = ARCH_VM_PDE(v);
		int pte = ARCH_VM_PTE(v);
		u32_t entry;

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
		entry = (physaddr & ARCH_VM_ADDR_MASK) | (u32_t)flags;
#elif defined(__arm__)
		entry = (physaddr & ARM_VM_PTE_MASK) | (u32_t)flags;
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
			maskedentry &= ~(ARM_VM_PTE_WB|ARM_VM_PTE_WT);
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
		}
#endif /* __x86_64__ */

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
		assert(!(v % VM_PAGE_SIZE));

#ifdef __x86_64__
		{
		int pdpte = ARCH_VM_PDPTE(v);
		int pde   = ARCH_VM_PDE(v);
		int pte   = ARCH_VM_PTE(v);
		u64_t entry;

		if(!pt->pt_pd[pdpte] ||
		   !(pt->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT))
			return EFAULT;
		assert(pt->pt_pt[pdpte] && pt->pt_pt[pdpte][pde]);
		entry = pt->pt_pt[pdpte][pde][pte];
		if(!(entry & ARCH_VM_PTE_PRESENT))
			return EFAULT;
		if(write && !(entry & AMD64_VM_WRITE))
			return EFAULT;
		}
#else /* i386 / earm */
		{
		int pde = ARCH_VM_PDE(v);
		int pte = ARCH_VM_PTE(v);

		assert(pte >= 0 && pte < ARCH_VM_PT_ENTRIES);
		assert(pde >= 0 && pde < ARCH_VM_DIR_ENTRIES);

		if(!(pt->pt_dir[pde] & ARCH_VM_PDE_PRESENT))
			return EFAULT;

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
		}
#endif /* __x86_64__ */

		v += VM_PAGE_SIZE;
	}

	return OK;
}

/*===========================================================================*
 *				pt_new			     		     *
 *===========================================================================*/
int pt_new(pt_t *pt)
{
/* Allocate a pagetable root. On x86_64 this is a PML4 + initial PDPT.
 * On i386/arm it is the page directory.
 */
	int r;

#ifdef __x86_64__
	{
	phys_bytes pdpt_phys;
	int i;

	/* Allocate PML4 page (once; never moved — same reason as i386 pt_dir). */
	if(!pt->pt_pml4) {
		phys_bytes pml4_phys;
		if(!(pt->pt_pml4 = (u64_t *)vm_allocpagedir(&pml4_phys)))
			return ENOMEM;
		pt->pt_pml4_phys = pml4_phys;
	}
	assert(!(pt->pt_pml4_phys % VM_PAGE_SIZE));

	/* Allocate PDPT page (once; re-derive physical from stored PML4[0]). */
	if(!pt->pt_pdpt) {
		if(!(pt->pt_pdpt = (u64_t *)vm_allocpagedir(&pdpt_phys)))
			return ENOMEM;
	} else {
		/* Already allocated; recover physical from saved PML4[0] entry.
		 * We must read it before clearing pt_pml4 below.
		 */
		pdpt_phys = pt->pt_pml4[0] & ARCH_VM_ADDR_MASK;
	}

	/* Clear PML4, wire PML4[0] → PDPT. */
	memset(pt->pt_pml4, 0, VM_PAGE_SIZE);
	pt->pt_pml4[0] = (pdpt_phys & ARCH_VM_ADDR_MASK) |
		ARCH_VM_PDE_PRESENT | ARCH_VM_PTE_USER | ARCH_VM_PTE_RW;

	/* Clear PDPT and PD/PT pointer arrays. */
	memset(pt->pt_pdpt, 0, VM_PAGE_SIZE);
	for(i = 0; i < ARCH_PDPT_ENTRIES; i++) {
		pt->pt_pd[i]  = NULL;
		pt->pt_pt[i]  = NULL;
	}

	pt->pt_virtop = 0;

	if((r = pt_mapkernel(pt)) != OK)
		return r;
	}
#else /* i386 / earm */
	int i;

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
#endif /* __x86_64__ */

	return OK;
}

static int freepde(void)
{
	int p = kernel_boot_info.freepde_start++;
	assert(kernel_boot_info.freepde_start < ARCH_VM_DIR_ENTRIES);
	return p;
}

void pt_allocate_kernel_mapped_pagetables(void)
{
#ifdef __x86_64__
	/* x86_64: kernel uses identity-mapped physical RAM; no PDM windows needed. */
	return;
#else
	/* Reserve PDEs available for mapping in the page directories. */
	int pd;
	for(pd = 0; pd < MAX_PAGEDIR_PDES; pd++) {
		struct pdm *pdm = &pagedir_mappings[pd];
		if(!pdm->pdeno)  {
			pdm->pdeno = freepde();
			assert(pdm->pdeno);
		}
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
		pdm->val = (ph & ARCH_VM_PDE_MASK)
			| ARCH_VM_PDE_PRESENT
			| ARM_VM_PTE_CACHED
			| ARM_VM_PDE_DOMAIN; //LSC FIXME
#endif
	}
#endif /* __x86_64__ */
}

static void pt_copy(pt_t *dst, pt_t *src)
{
#ifdef __x86_64__
	int pdpte, pde;
	for(pdpte = 0; pdpte < ARCH_PDPT_ENTRIES; pdpte++) {
		if(!src->pt_pd[pdpte]) continue;
		for(pde = 0; pde < ARCH_VM_DIR_ENTRIES; pde++) {
			if(!src->pt_pd[pdpte] ||
			   !(src->pt_pd[pdpte][pde] & ARCH_VM_PDE_PRESENT))
				continue;
			if(!src->pt_pt[pdpte] || !src->pt_pt[pdpte][pde])
				panic("pt_copy: pde %d/%d empty", pdpte, pde);
			if(pt_ptalloc(dst, pdpte, pde, 0) != OK)
				panic("pt_copy: pt_ptalloc failed");
			memcpy(dst->pt_pt[pdpte][pde], src->pt_pt[pdpte][pde],
				ARCH_VM_PT_ENTRIES * sizeof(*dst->pt_pt[pdpte][pde]));
		}
	}
#else
	int pde;
	for(pde=0; pde < kern_start_pde; pde++) {
		if(!(src->pt_dir[pde] & ARCH_VM_PDE_PRESENT)) {
			continue;
		}
		assert(!(src->pt_dir[pde] & ARCH_VM_BIGPAGE));
		if(!src->pt_pt[pde]) { panic("pde %d empty\n", pde); }
		if(pt_ptalloc(dst, pde, 0) != OK)
			panic("pt_ptalloc failed");
		memcpy(dst->pt_pt[pde], src->pt_pt[pde],
			ARCH_VM_PT_ENTRIES * sizeof(*dst->pt_pt[pde]));
	}
#endif
}

/*===========================================================================*
 *                              pt_init                                      *
 *===========================================================================*/
void pt_init(void)
{
        pt_t *newpt, newpt_dyn;
        int s, r, p;
	phys_bytes phys;
	vir_bytes sparepages_mem;
#if defined(__arm__)
	vir_bytes sparepagedirs_mem;
#endif
	static u32_t currentpagedir[ARCH_VM_DIR_ENTRIES];
	int m = kernel_boot_info.kern_mod;
#if defined(__x86_64__)
	phys_bytes mypdbr; /* PML4 physical address */
	/* Bounce buffers: VM cannot dereference the bootstrap page tables
	 * directly (they live in the kernel image, whose low identity alias is
	 * occupied by VM's own program), so copy each level from physical via
	 * the kernel (sys_vircopy from NONE). */
	static u64_t pml4buf[ARCH_VM_DIR_ENTRIES];
	static u64_t pdptbuf[ARCH_PDPT_ENTRIES];
	static u64_t pdbuf[ARCH_VM_DIR_ENTRIES];
#elif defined(__i386__)
	int global_bit_ok = 0;
	phys_bytes mypdbr; /* Page Directory Base Register (cr3) value */
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
#if !defined(__x86_64__)
	kern_start_pde = kernel_boot_info.vir_kern_start / ARCH_BIG_PAGE_SIZE;
#endif

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

	if(!(spare_pagequeue = reservedqueue_new(SPAREPAGES, 1, 1, 0)))
		panic("reservedqueue_new for single pages failed");

        assert(STATIC_SPAREPAGES < SPAREPAGES);
        for(s = 0; s < STATIC_SPAREPAGES; s++) {
		void *v = (void *) (sparepages_mem + s*VM_PAGE_SIZE);
		phys_bytes ph;
		if((r=sys_umap(SELF, VM_D, (vir_bytes) v,
	                VM_PAGE_SIZE*SPAREPAGES, &ph)) != OK)
				panic("pt_init: sys_umap failed: %d", r);
		reservedqueue_add(spare_pagequeue, v, ph);
        }

#if defined(__x86_64__)
	/* x86_64 always has PGE and 2MB pages (PSE). */
	global_bit = AMD64_VM_GLOBAL;
	bigpage_ok = 1;
#elif defined(__i386__)
	/* global bit and 4MB pages available? */
	global_bit_ok = _cpufeature(_CPUF_I386_PGE);
	bigpage_ok = _cpufeature(_CPUF_I386_PSE);

	/* Set bit for PTE's and PDE's if available. */
	if(global_bit_ok)
		global_bit = I386_VM_GLOBAL;
#endif

	/* Now reserve another pde for kernel's own mappings. */
	{
#ifndef __x86_64__
		int kernmap_pde;
#endif
		phys_bytes addr, len;
		int flags, pindex = 0;
		vir_bytes offset = 0;

#ifdef __x86_64__
		/* Map the kernel-provided regions (the user-shared usermapped area
		 * that holds minix_kerninfo, plus device windows) just above the
		 * user address space.  They MUST be reachable from user mode: on
		 * x86_64 the kernel high half lives under the supervisor PML4[511]
		 * (kern_pml4_hi), so anything mapped there (as i386 does) is
		 * inaccessible to user processes because the U/S bit is ANDed across
		 * all four paging levels.  VM_DATATOP is in the user half (PML4[0],
		 * which pt_new() marks USER) and above all normal user allocations.
		 * Page tables are allocated on demand by pt_ptalloc(), so no PDE
		 * pre-reservation (freepde) is needed here. */
		offset = VM_DATATOP;
#else
		kernmap_pde = freepde();
		offset = kernmap_pde * ARCH_BIG_PAGE_SIZE;
#endif

		while(sys_vmctl_get_mapping(pindex, &addr, &len,
			&flags) == OK)  {
#ifndef __x86_64__
			int usedpde;
#endif
			vir_bytes vir;
			if(pindex >= MAX_KERNMAPPINGS)
                		panic("VM: too many kernel mappings: %d", pindex);
			kern_mappings[pindex].phys_addr = addr;
			kern_mappings[pindex].len = len;
			kern_mappings[pindex].flags = flags;
			kern_mappings[pindex].vir_addr = offset;
			kern_mappings[pindex].flags =
				ARCH_VM_PTE_PRESENT;
			if(flags & VMMF_UNCACHED)
#if defined(__i386__) || defined(__x86_64__)
				kern_mappings[pindex].flags |= PTF_NOCACHE;
#elif defined(__arm__)
				kern_mappings[pindex].flags |= ARM_VM_PTE_DEVICE;
			else {
				kern_mappings[pindex].flags |= ARM_VM_PTE_CACHED;
			}
#endif
			if(flags & VMMF_USER)
				kern_mappings[pindex].flags |= ARCH_VM_PTE_USER;
#if defined(__arm__)
			else
				kern_mappings[pindex].flags |= ARM_VM_PTE_SUPER;
#endif
			if(flags & VMMF_WRITE)
				kern_mappings[pindex].flags |= ARCH_VM_PTE_RW;
#if defined(__arm__)
			else 
				kern_mappings[pindex].flags |= ARCH_VM_PTE_RO;
#endif

#if defined(__i386__) || defined(__x86_64__)
			if(flags & VMMF_GLO)
				kern_mappings[pindex].flags |= PTF_GLOBAL;
#endif

			if(addr % VM_PAGE_SIZE)
                		panic("VM: addr unaligned: %lu", addr);
			if(len % VM_PAGE_SIZE)
                		panic("VM: len unaligned: %lu", len);
			vir = offset;
			if(sys_vmctl_reply_mapping(pindex, vir) != OK)
                		panic("VM: reply failed");
			offset += len;
			pindex++;
			kernmappings++;

#ifndef __x86_64__
			usedpde = ARCH_VM_PDE(offset);
			while(usedpde > kernmap_pde) {
				int newpde = freepde();
				assert(newpde == kernmap_pde+1);
				kernmap_pde = newpde;
			}
#endif
		}
	}

	pt_allocate_kernel_mapped_pagetables();

	/* Allright. Now. We have to make our own page directory and page tables,
	 * that the kernel has already set up, accessible to us. It's easier to
	 * understand if we just copy all the required pages (i.e. page directory
	 * and page tables), and set up the pointers as if VM had done it itself.
	 *
	 * This allocation will happen without using any page table, and just
	 * uses spare pages.
	 */
	/* Get our current pagedir so we can see it. */
#if defined(__x86_64__)
	if(sys_vmctl_get_pdbr(SELF, &mypdbr) != OK)
		panic("VM: sys_vmctl_get_pdbr failed");

	/* Capture the kernel's high PML4 entry BEFORE pt_new(): pt_new() calls
	 * pt_mapkernel(), which needs kern_pml4_hi.  Copy our PML4 from physical
	 * via the kernel (sys_vircopy from NONE); we cannot dereference it
	 * directly (see pml4buf comment above). */
	if(sys_vircopy(NONE, mypdbr, SELF, (vir_bytes) pml4buf,
		sizeof(pml4buf), 0) != OK)
		panic("VM: pml4 copy failed");
	kern_pml4_hi = pml4buf[511];
#elif defined(__i386__)
	if(sys_vmctl_get_pdbr(SELF, &mypdbr) != OK)
		panic("VM: sys_vmctl_get_pdbr failed");
#elif defined(__arm__)
	if(sys_vmctl_get_pdbr(SELF, &myttbr) != OK)
		panic("VM: sys_vmctl_get_pdbr failed");
#endif

        newpt = &vmprocess->vm_pt;
	if(pt_new(newpt) != OK)
		panic("vm pt_new failed");

#if defined(__x86_64__)
	/* Walk PML4→PDPT→PD (copied from physical via the kernel) to find VM's
	 * user PT pages and replicate them into newpt.  PML4[511] (kernel high)
	 * was already saved into kern_pml4_hi above.
	 */
	{
		int pml4i, pdpte_i, pde_i;

		/* Walk only user-space PML4 entries (0..510; 511 = kernel high). */
		for(pml4i = 0; pml4i < 511; pml4i++) {
			if(!(pml4buf[pml4i] & AMD64_VM_PRESENT)) continue;
			if(sys_vircopy(NONE, pml4buf[pml4i] & ARCH_VM_ADDR_MASK,
				SELF, (vir_bytes) pdptbuf, sizeof(pdptbuf), 0) != OK)
				panic("VM: pdpt copy failed");

			for(pdpte_i = 0; pdpte_i < ARCH_PDPT_ENTRIES; pdpte_i++) {
				if(!(pdptbuf[pdpte_i] & AMD64_VM_PRESENT)) continue;
				if(pdptbuf[pdpte_i] & AMD64_VM_PS) continue; /* 1 GB page */
				if(sys_vircopy(NONE, pdptbuf[pdpte_i] & ARCH_VM_ADDR_MASK,
					SELF, (vir_bytes) pdbuf, sizeof(pdbuf), 0) != OK)
					panic("VM: pd copy failed");

				for(pde_i = 0; pde_i < ARCH_VM_DIR_ENTRIES; pde_i++) {
					phys_bytes ptaddr_kern, ptaddr_us;
					if(!(pdbuf[pde_i] & AMD64_VM_PRESENT)) continue;
					if(pdbuf[pde_i] & AMD64_VM_PS) continue; /* 2 MB page */

					if(pt_ptalloc(newpt, pdpte_i, pde_i, 0) != OK)
						panic("pt_init: pt_ptalloc failed");
					ptaddr_kern = pdbuf[pde_i] & ARCH_VM_ADDR_MASK;
					ptaddr_us   = newpt->pt_pd[pdpte_i][pde_i] & ARCH_VM_ADDR_MASK;
					if(sys_abscopy(ptaddr_kern, ptaddr_us, VM_PAGE_SIZE) != OK)
						panic("pt_init: abscopy failed");
				}
			}
		}
	}
#elif defined(__i386__)
	if(sys_vircopy(NONE, mypdbr, SELF,
		(vir_bytes) currentpagedir, VM_PAGE_SIZE, 0) != OK)
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

		ptaddr_kern = entry & ARCH_VM_ADDR_MASK;
		ptaddr_us = newpt->pt_dir[p] & ARCH_VM_ADDR_MASK;

		/* Copy kernel-initialized pagetable contents into our
		 * normally accessible pagetable.
		 */
                if(sys_abscopy(ptaddr_kern, ptaddr_us, VM_PAGE_SIZE) != OK)
			panic("pt_init: abscopy failed");
	}
#elif defined(__arm__)
	if(sys_vircopy(NONE, myttbr, SELF,
		(vir_bytes) currentpagedir, ARCH_PAGEDIR_SIZE, 0) != OK)
		panic("VM: sys_vircopy failed");

	for(p = 0; p < ARCH_VM_DIR_ENTRIES; p++) {
		u32_t entry = currentpagedir[p];
		phys_bytes ptaddr_kern, ptaddr_us;

		if(!(entry & ARCH_VM_PDE_PRESENT)) continue;
		if((entry & ARCH_VM_BIGPAGE)) continue;

		if(pt_ptalloc(newpt, p, 0) != OK)
			panic("pt_ptalloc failed");
		assert(newpt->pt_dir[p] & ARCH_VM_PDE_PRESENT);

		ptaddr_kern = entry & ARCH_VM_PDE_MASK;
		ptaddr_us = newpt->pt_dir[p] & ARCH_VM_PDE_MASK;

                if(sys_abscopy(ptaddr_kern, ptaddr_us, VM_PAGE_SIZE) != OK)
			panic("pt_init: abscopy failed");
	}
#endif

	/* Inform kernel vm has a newly built page table. */
	assert(vmproc[VM_PROC_NR].vm_endpoint == VM_PROC_NR);
	pt_bind(newpt, &vmproc[VM_PROC_NR]);

	pt_init_done = 1;

	/* VM is now fully functional in that it can dynamically allocate memory
	 * for itself.
	 *
	 * We don't want to keep using the bootstrap statically allocated spare
	 * pages though, as the physical addresses will change on liveupdate. So we
	 * re-do part of the initialization now with purely dynamically allocated
	 * memory. First throw out the static pool.
	 *
	 * Then allocate the kernel-shared-pagetables and VM pagetables with dynamic
	 * memory.
	 */

	alloc_cycle();                          /* Make sure allocating works */
	while(vm_getsparepage(&phys)) ;		/* Use up all static pages */
	alloc_cycle();                          /* Refill spares with dynamic */
	pt_allocate_kernel_mapped_pagetables(); /* Reallocate in-kernel pages */
	pt_bind(newpt, &vmproc[VM_PROC_NR]);    /* Recalculate */
	pt_mapkernel(newpt);                    /* Rewrite pagetable info */

	/* Flush TLB just in case any of those mappings have been touched */
	if((sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("VMCTL_FLUSHTLB failed");
	}

	/* Recreate VM page table with dynamic-only allocations */
	memset(&newpt_dyn, 0, sizeof(newpt_dyn));
	pt_new(&newpt_dyn);
	pt_copy(&newpt_dyn, newpt);
	memcpy(newpt, &newpt_dyn, sizeof(*newpt));

	pt_bind(newpt, &vmproc[VM_PROC_NR]);    /* Recalculate */
	pt_mapkernel(newpt);                    /* Rewrite pagetable info */

	/* Flush TLB just in case any of those mappings have been touched */
	if((sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("VMCTL_FLUSHTLB failed");
	}

        /* All OK. */
        return;
}

/*===========================================================================*
 *				pt_bind			     		     *
 *===========================================================================*/
int pt_bind(pt_t *pt, struct vmproc *who)
{
	assert(who);
	assert(who->vm_flags & VMF_INUSE);
	assert(pt);

#ifdef __x86_64__
	/* On x86_64 the kernel uses identity-mapped physical RAM; no PDM windows.
	 * Pass PML4 physical address as CR3 and virtual address of PML4 as ptroot_v.
	 */
	assert(pt->pt_pml4);
	assert(pt->pt_pml4_phys);
	assert(!(pt->pt_pml4_phys % VM_PAGE_SIZE));
	return sys_vmctl_set_addrspace(who->vm_endpoint,
		pt->pt_pml4_phys, (void *)pt->pt_pml4);
#else
	int procslot, pdeslot;
	u32_t phys;
	void *pdes;
	int pagedir_pde;
	int slots_per_pde;
	int pages_per_pagedir = ARCH_PAGEDIR_SIZE/VM_PAGE_SIZE;
	struct pdm *pdm;

	slots_per_pde = ARCH_VM_PT_ENTRIES / pages_per_pagedir;

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
			(phys+i*VM_PAGE_SIZE)
			| ARCH_VM_PTE_PRESENT
			| ARCH_VM_PTE_RW
			| ARM_VM_PTE_CACHED
			| ARCH_VM_PTE_USER; //LSC FIXME
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

	/* Tell kernel about new page table root. */
	return sys_vmctl_set_addrspace(who->vm_endpoint, pt->pt_dir_phys , pdes);
#endif /* __x86_64__ */
}

/*===========================================================================*
 *				pt_free			     		     *
 *===========================================================================*/
void pt_free(pt_t *pt)
{
/* Free memory associated with this pagetable. */
#ifdef __x86_64__
	int pdpte, pde;

	for(pdpte = 0; pdpte < ARCH_PDPT_ENTRIES; pdpte++) {
		if(!pt->pt_pt[pdpte] && !pt->pt_pd[pdpte]) continue;
		if(pt->pt_pt[pdpte]) {
			for(pde = 0; pde < ARCH_VM_DIR_ENTRIES; pde++) {
				if(pt->pt_pt[pdpte][pde])
					vm_freepages((vir_bytes)pt->pt_pt[pdpte][pde], 1);
			}
			/* Free the pointer array page itself. */
			vm_freepages((vir_bytes)pt->pt_pt[pdpte], 1);
		}
		if(pt->pt_pd[pdpte])
			vm_freepages((vir_bytes)pt->pt_pd[pdpte], 1);
		pt->pt_pd[pdpte] = NULL;
		pt->pt_pt[pdpte] = NULL;
	}
	/* pt_pml4 AND pt_pdpt are never freed: pt_new() allocates them once and
	 * reuses them (it recovers pt_pdpt's physical address from the kept
	 * pt_pml4[0] entry).  Freeing pt_pdpt here while leaving the pointer
	 * non-NULL made the next pt_new() (e.g. VMPPARAM_CLEAR reusing the slot)
	 * memset() a freed, unmapped page -> "pagefault in VM".  Same persistent
	 * per-slot policy as i386 pt_dir. */
#else
	int i;

	for(i = 0; i < ARCH_VM_DIR_ENTRIES; i++)
		if(pt->pt_pt[i])
			vm_freepages((vir_bytes) pt->pt_pt[i], 1);
#endif
}

/*===========================================================================*
 *				pt_mapkernel		     		     *
 *===========================================================================*/
int pt_mapkernel(pt_t *pt)
{
#ifdef __x86_64__
	int i;

	/* Share the kernel's upper-half page tables via PML4[511].
	 * kern_pml4_hi is captured from VM's own PML4 during pt_init().
	 */
	assert(kern_pml4_hi & AMD64_VM_PRESENT);
	pt->pt_pml4[511] = kern_pml4_hi;

	/* Map any additional kernel device regions. */
	for(i = 0; i < kernmappings; i++) {
		int r;
		if((r = pt_writemap(NULL, pt,
			kern_mappings[i].vir_addr,
			kern_mappings[i].phys_addr,
			kern_mappings[i].len,
			kern_mappings[i].flags, 0)) != OK)
			return r;
	}
#else
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
		pt->pt_dir[kern_pde] = (addr & ARM_VM_SECTION_MASK)
			| ARM_VM_SECTION
			| ARM_VM_SECTION_DOMAIN
			| ARM_VM_SECTION_CACHED
			| ARM_VM_SECTION_SUPER;
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

	}
#endif /* __x86_64__ */

	return OK;
}

int get_vm_self_pages(void) { return vm_self_pages; }
