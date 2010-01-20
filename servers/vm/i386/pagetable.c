
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

#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include "../proto.h"
#include "../glo.h"
#include "../util.h"
#include "../vm.h"
#include "../sanitycheck.h"

#include "memory.h"

/* PDE used to map in kernel, kernel physical address. */
PRIVATE int id_map_high_pde = -1, pagedir_pde = -1;
PRIVATE u32_t global_bit = 0, pagedir_pde_val;

PRIVATE int proc_pde = 0;

/* 4MB page size available in hardware? */
PRIVATE int bigpage_ok = 0;

/* Our process table entry. */
struct vmproc *vmp = &vmproc[VM_PROC_NR];

/* Spare memory, ready to go after initialization, to avoid a
 * circular dependency on allocating memory and writing it into VM's
 * page table.
 */
#define SPAREPAGES 25
int missing_spares = SPAREPAGES;
PRIVATE struct {
	void *page;
	u32_t phys;
} sparepages[SPAREPAGES];

#define MAX_KERNMAPPINGS 10
PRIVATE struct {
	phys_bytes	phys_addr;	/* Physical addr. */
	phys_bytes	len;		/* Length in bytes. */
	vir_bytes	lin_addr;	/* Offset in page table. */
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
#if CLICK_SIZE != I386_PAGE_SIZE
#error CLICK_SIZE must be page size.
#endif

/* Bytes of virtual address space one pde controls. */
#define BYTESPERPDE (I386_VM_PT_ENTRIES * I386_PAGE_SIZE)

/* Nevertheless, introduce these macros to make the code readable. */
#define CLICK2PAGE(c) ((c) / CLICKSPERPAGE)

/* Page table that contains pointers to all page directories. */
u32_t page_directories_phys, *page_directories = NULL;

#if SANITYCHECKS
/*===========================================================================*
 *				pt_sanitycheck		     		     *
 *===========================================================================*/
PUBLIC void pt_sanitycheck(pt_t *pt, char *file, int line)
{
/* Basic pt sanity check. */
	int i;
	int slot;

	MYASSERT(pt);
	MYASSERT(pt->pt_dir);
	MYASSERT(pt->pt_dir_phys);

	for(slot = 0; slot < ELEMENTS(vmproc); slot++) {
		if(pt == &vmproc[slot].vm_pt)
			break;
	}

	if(slot >= ELEMENTS(vmproc)) {
		vm_panic("pt_sanitycheck: passed pt not in any proc", NO_NUM);
	}

	MYASSERT(usedpages_add(pt->pt_dir_phys, I386_PAGE_SIZE) == OK);

	for(i = proc_pde; i < I386_VM_DIR_ENTRIES; i++) {
		if(pt->pt_pt[i]) {
			if(!(pt->pt_dir[i] & I386_VM_PRESENT)) {
				printf("slot %d: pt->pt_pt[%d] = 0x%lx, but pt_dir entry 0x%lx\n",
					slot, i, pt->pt_pt[i], pt->pt_dir[i]);
			}
			MYASSERT(pt->pt_dir[i] & I386_VM_PRESENT);
			MYASSERT(usedpages_add(I386_VM_PFA(pt->pt_dir[i]),
				I386_PAGE_SIZE) == OK);
		} else {
			MYASSERT(!(pt->pt_dir[i] & I386_VM_PRESENT));
		}
	}
}
#endif

/*===========================================================================*
 *				aalloc			     		     *
 *===========================================================================*/
PRIVATE void *aalloc(size_t bytes)
{
/* Page-aligned malloc(). only used if vm_allocpage can't be used.  */
	u32_t b;

	b = (u32_t) malloc(I386_PAGE_SIZE + bytes);
	if(!b) vm_panic("aalloc: out of memory", bytes);
	b += I386_PAGE_SIZE - (b % I386_PAGE_SIZE);

	return (void *) b;
}

/*===========================================================================*
 *				findhole		     		     *
 *===========================================================================*/
PRIVATE u32_t findhole(pt_t *pt, u32_t vmin, u32_t vmax)
{
/* Find a space in the virtual address space of pageteble 'pt',
 * between page-aligned BYTE offsets vmin and vmax, to fit
 * a page in. Return byte offset.
 */
	u32_t freefound = 0, curv;
	int pde = 0, try_restart;
	static u32_t lastv = 0;

	/* Input sanity check. */
	vm_assert(vmin + I386_PAGE_SIZE >= vmin);
	vm_assert(vmax >= vmin + I386_PAGE_SIZE);
	vm_assert((vmin % I386_PAGE_SIZE) == 0);
	vm_assert((vmax % I386_PAGE_SIZE) == 0);

#if SANITYCHECKS
	curv = ((u32_t) random()) % ((vmax - vmin)/I386_PAGE_SIZE);
	curv *= I386_PAGE_SIZE;
	curv += vmin;
#else
	curv = lastv;
	if(curv < vmin || curv >= vmax)
		curv = vmin;
#endif
	try_restart = 1;

	/* Start looking for a free page starting at vmin. */
	while(curv < vmax) {
		int pte;

		vm_assert(curv >= vmin);
		vm_assert(curv < vmax);

		pde = I386_VM_PDE(curv);
		pte = I386_VM_PTE(curv);

		if(!(pt->pt_dir[pde] & I386_VM_PRESENT) ||
		   !(pt->pt_pt[pde][pte] & I386_VM_PRESENT)) {
			lastv = curv;
			return curv;
		}

		curv+=I386_PAGE_SIZE;

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
PRIVATE void vm_freepages(vir_bytes vir, vir_bytes phys, int pages, int reason)
{
	vm_assert(reason >= 0 && reason < VMP_CATEGORIES);
	if(vir >= vmp->vm_stacktop) {
		vm_assert(!(vir % I386_PAGE_SIZE)); 
		vm_assert(!(phys % I386_PAGE_SIZE)); 
		FREE_MEM(ABS2CLICK(phys), pages);
		if(pt_writemap(&vmp->vm_pt, arch_vir2map(vmp, vir),
			MAP_NONE, pages*I386_PAGE_SIZE, 0, WMF_OVERWRITE) != OK)
				vm_panic("vm_freepages: pt_writemap failed",
					NO_NUM);
	} else {
		printf("VM: vm_freepages not freeing VM heap pages (%d)\n",
			pages);
	}
}

/*===========================================================================*
 *				vm_getsparepage		     		     *
 *===========================================================================*/
PRIVATE void *vm_getsparepage(u32_t *phys)
{
	int s;
	vm_assert(missing_spares >= 0 && missing_spares <= SPAREPAGES);
	for(s = 0; s < SPAREPAGES; s++) {
		if(sparepages[s].page) {
			void *sp;
			sp = sparepages[s].page;
			*phys = sparepages[s].phys;
			sparepages[s].page = NULL;
			missing_spares++;
			vm_assert(missing_spares >= 0 && missing_spares <= SPAREPAGES);
			return sp;
		}
	}
	return NULL;
}

/*===========================================================================*
 *				vm_checkspares		     		     *
 *===========================================================================*/
PRIVATE void *vm_checkspares(void)
{
	int s, n = 0;
	static int total = 0, worst = 0;
	vm_assert(missing_spares >= 0 && missing_spares <= SPAREPAGES);
	for(s = 0; s < SPAREPAGES && missing_spares > 0; s++)
	    if(!sparepages[s].page) {
		n++;
		if((sparepages[s].page = vm_allocpage(&sparepages[s].phys, 
			VMP_SPARE))) {
			missing_spares--;
			vm_assert(missing_spares >= 0);
			vm_assert(missing_spares <= SPAREPAGES);
		} else {
			printf("VM: warning: couldn't get new spare page\n");
		}
	}
	if(worst < n) worst = n;
	total += n;

	return NULL;
}

/*===========================================================================*
 *				vm_allocpage		     		     *
 *===========================================================================*/
PUBLIC void *vm_allocpage(phys_bytes *phys, int reason)
{
/* Allocate a page for use by VM itself. */
	phys_bytes newpage;
	vir_bytes loc;
	pt_t *pt;
	int r;
	static int level = 0;
	void *ret;

	pt = &vmp->vm_pt;
	vm_assert(reason >= 0 && reason < VMP_CATEGORIES);

	level++;

	vm_assert(level >= 1);
	vm_assert(level <= 2);

	if(level > 1 || !(vmp->vm_flags & VMF_HASPT) || !meminit_done) {
		int r;
		void *s;
		s=vm_getsparepage(phys);
		level--;
		if(!s) {
			util_stacktrace();
			printf("VM: warning: out of spare pages\n");
		}
		return s;
	}

	/* VM does have a pagetable, so get a page and map it in there.
	 * Where in our virtual address space can we put it?
	 */
	loc = findhole(pt,  arch_vir2map(vmp, vmp->vm_stacktop),
		vmp->vm_arch.vm_data_top);
	if(loc == NO_MEM) {
		level--;
		printf("VM: vm_allocpage: findhole failed\n");
		return NULL;
	}

	/* Allocate page of memory for use by VM. As VM
	 * is trusted, we don't have to pre-clear it.
	 */
	if((newpage = ALLOC_MEM(CLICKSPERPAGE, 0)) == NO_MEM) {
		level--;
		printf("VM: vm_allocpage: ALLOC_MEM failed\n");
		return NULL;
	}

	*phys = CLICK2ABS(newpage);

	/* Map this page into our address space. */
	if((r=pt_writemap(pt, loc, *phys, I386_PAGE_SIZE,
		I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE, 0)) != OK) {
		FREE_MEM(newpage, CLICKSPERPAGE);
		printf("vm_allocpage writemap failed\n");
		level--;
		return NULL;
	}

	if((r=sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		vm_panic("VMCTL_FLUSHTLB failed", r);
	}

	level--;

	/* Return user-space-ready pointer to it. */
	ret = (void *) arch_map2vir(vmp, loc);

	return ret;
}

/*===========================================================================*
 *				vm_pagelock		     		     *
 *===========================================================================*/
PUBLIC void vm_pagelock(void *vir, int lockflag)
{
/* Mark a page allocated by vm_allocpage() unwritable, i.e. only for VM. */
	vir_bytes m;
	int r;
	u32_t flags = I386_VM_PRESENT | I386_VM_USER;
	pt_t *pt;

	pt = &vmp->vm_pt;
	m = arch_vir2map(vmp, (vir_bytes) vir);

	vm_assert(!(m % I386_PAGE_SIZE));

	if(!lockflag)
		flags |= I386_VM_WRITE;

	/* Update flags. */
	if((r=pt_writemap(pt, m, 0, I386_PAGE_SIZE,
		flags, WMF_OVERWRITE | WMF_WRITEFLAGSONLY)) != OK) {
		vm_panic("vm_lockpage: pt_writemap failed\n", NO_NUM);
	}

	if((r=sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		vm_panic("VMCTL_FLUSHTLB failed", r);
	}

	return;
}

/*===========================================================================*
 *				pt_ptalloc		     		     *
 *===========================================================================*/
PRIVATE int pt_ptalloc(pt_t *pt, int pde, u32_t flags)
{
/* Allocate a page table and write its address into the page directory. */
	int i;
	u32_t pt_phys;

	/* Argument must make sense. */
	vm_assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);
	vm_assert(!(flags & ~(PTF_ALLFLAGS)));

	/* We don't expect to overwrite page directory entry, nor
	 * storage for the page table.
	 */
	vm_assert(!(pt->pt_dir[pde] & I386_VM_PRESENT));
	vm_assert(!pt->pt_pt[pde]);

	/* Get storage for the page table. */
        if(!(pt->pt_pt[pde] = vm_allocpage(&pt_phys, VMP_PAGETABLE)))
		return ENOMEM;

	for(i = 0; i < I386_VM_PT_ENTRIES; i++)
		pt->pt_pt[pde][i] = 0;	/* Empty entry. */

	/* Make page directory entry.
	 * The PDE is always 'present,' 'writable,' and 'user accessible,'
	 * relying on the PTE for protection.
	 */
	pt->pt_dir[pde] = (pt_phys & I386_VM_ADDR_MASK) | flags
		| I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE;

	return OK;
}

/*===========================================================================*
 *				pt_writemap		     		     *
 *===========================================================================*/
PUBLIC int pt_writemap(pt_t *pt, vir_bytes v, phys_bytes physaddr,
	size_t bytes, u32_t flags, u32_t writemapflags)
{
/* Write mapping into page table. Allocate a new page table if necessary. */
/* Page directory and table entries for this virtual address. */
	int p, pages, pdecheck;
	int finalpde;
	int verify = 0;

	if(writemapflags & WMF_VERIFY)
		verify = 1;

	vm_assert(!(bytes % I386_PAGE_SIZE));
	vm_assert(!(flags & ~(PTF_ALLFLAGS)));

	pages = bytes / I386_PAGE_SIZE;

	/* MAP_NONE means to clear the mapping. It doesn't matter
	 * what's actually written into the PTE if I386_VM_PRESENT
	 * isn't on, so we can just write MAP_NONE into it.
	 */
#if SANITYCHECKS
	if(physaddr != MAP_NONE && !(flags & I386_VM_PRESENT)) {
		vm_panic("pt_writemap: writing dir with !P\n", NO_NUM);
	}
	if(physaddr == MAP_NONE && flags) {
		vm_panic("pt_writemap: writing 0 with flags\n", NO_NUM);
	}
#endif

	finalpde = I386_VM_PDE(v + I386_PAGE_SIZE * pages);

	/* First make sure all the necessary page tables are allocated,
	 * before we start writing in any of them, because it's a pain
	 * to undo our work properly. Walk the range in page-directory-entry
	 * sized leaps.
	 */
	for(pdecheck = I386_VM_PDE(v); pdecheck <= finalpde; pdecheck++) {
		vm_assert(pdecheck >= 0 && pdecheck < I386_VM_DIR_ENTRIES);
		if(pt->pt_dir[pdecheck] & I386_VM_BIGPAGE) {
			printf("pt_writemap: trying to write 0x%lx into 0x%lx\n",
				physaddr, v);
                        vm_panic("pt_writemap: BIGPAGE found", NO_NUM);
		}
		if(!(pt->pt_dir[pdecheck] & I386_VM_PRESENT)) {
			int r;
			if(verify) {
				printf("pt_writemap verify: no pde %d\n", pdecheck);
				return EFAULT;
			}
			vm_assert(!pt->pt_dir[pdecheck]);
			if((r=pt_ptalloc(pt, pdecheck, flags)) != OK) {
				/* Couldn't do (complete) mapping.
				 * Don't bother freeing any previously
				 * allocated page tables, they're
				 * still writable, don't point to nonsense,
				 * and pt_ptalloc leaves the directory
				 * and other data in a consistent state.
				 */
				printf("pt_writemap: pt_ptalloc failed\n", pdecheck);
				return r;
			}
		}
		vm_assert(pt->pt_dir[pdecheck] & I386_VM_PRESENT);
	}

	/* Now write in them. */
	for(p = 0; p < pages; p++) {
		u32_t entry;
		int pde = I386_VM_PDE(v);
		int pte = I386_VM_PTE(v);

		vm_assert(!(v % I386_PAGE_SIZE));
		vm_assert(pte >= 0 && pte < I386_VM_PT_ENTRIES);
		vm_assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);

		/* Page table has to be there. */
		vm_assert(pt->pt_dir[pde] & I386_VM_PRESENT);

		/* Make sure page directory entry for this page table
		 * is marked present and page table entry is available.
		 */
		vm_assert((pt->pt_dir[pde] & I386_VM_PRESENT) && pt->pt_pt[pde]);

#if SANITYCHECKS
		/* We don't expect to overwrite a page. */
		if(!(writemapflags & (WMF_OVERWRITE|WMF_VERIFY)))
			vm_assert(!(pt->pt_pt[pde][pte] & I386_VM_PRESENT));
#endif
		if(writemapflags & (WMF_WRITEFLAGSONLY|WMF_FREE)) {
			physaddr = pt->pt_pt[pde][pte] & I386_VM_ADDR_MASK;
		}

		if(writemapflags & WMF_FREE) {
			FREE_MEM(ABS2CLICK(physaddr), 1);
		}

		/* Entry we will write. */
		entry = (physaddr & I386_VM_ADDR_MASK) | flags;

		if(verify) {
			u32_t maskedentry;
			maskedentry = pt->pt_pt[pde][pte];
			maskedentry &= ~(I386_VM_ACC|I386_VM_DIRTY);
			/* Verify pagetable entry. */
			if(maskedentry != entry) {
				printf("pt_writemap: 0x%lx found, masked 0x%lx, 0x%lx expected\n",
					pt->pt_pt[pde][pte], maskedentry, entry);
				return EFAULT;
			}
		} else {
			/* Write pagetable entry. */
			pt->pt_pt[pde][pte] = entry;
		}

		physaddr += I386_PAGE_SIZE;
		v += I386_PAGE_SIZE;
	}

	return OK;
}

/*===========================================================================*
 *				pt_checkrange		     		     *
 *===========================================================================*/
PUBLIC int pt_checkrange(pt_t *pt, vir_bytes v,  size_t bytes,
	int write)
{
	int p, pages, pde;

	vm_assert(!(bytes % I386_PAGE_SIZE));

	pages = bytes / I386_PAGE_SIZE;

	for(p = 0; p < pages; p++) {
		u32_t entry;
		int pde = I386_VM_PDE(v);
		int pte = I386_VM_PTE(v);

		vm_assert(!(v % I386_PAGE_SIZE));
		vm_assert(pte >= 0 && pte < I386_VM_PT_ENTRIES);
		vm_assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);

		/* Page table has to be there. */
		if(!(pt->pt_dir[pde] & I386_VM_PRESENT))
			return EFAULT;

		/* Make sure page directory entry for this page table
		 * is marked present and page table entry is available.
		 */
		vm_assert((pt->pt_dir[pde] & I386_VM_PRESENT) && pt->pt_pt[pde]);

		if(!(pt->pt_pt[pde][pte] & I386_VM_PRESENT)) {
			return EFAULT;
		}

		if(write && !(pt->pt_pt[pde][pte] & I386_VM_WRITE)) {
			return EFAULT;
		}

		v += I386_PAGE_SIZE;
	}

	return OK;
}

/*===========================================================================*
 *				pt_new			     		     *
 *===========================================================================*/
PUBLIC int pt_new(pt_t *pt)
{
/* Allocate a pagetable root. On i386, allocate a page-aligned page directory
 * and set them to 0 (indicating no page tables are allocated). Lookup
 * its physical address as we'll need that in the future. Verify it's
 * page-aligned.
 */
	int i;

	/* Don't ever re-allocate/re-move a certain process slot's
	 * page directory once it's been created. This is a fraction
	 * faster, but also avoids having to invalidate the page
	 * mappings from in-kernel page tables pointing to
	 * the page directories (the page_directories data).
	 */
        if(!pt->pt_dir &&
          !(pt->pt_dir = vm_allocpage(&pt->pt_dir_phys, VMP_PAGEDIR))) {
		return ENOMEM;
	}

	for(i = 0; i < I386_VM_DIR_ENTRIES; i++) {
		pt->pt_dir[i] = 0; /* invalid entry (I386_VM_PRESENT bit = 0) */
		pt->pt_pt[i] = NULL;
	}

	/* Where to start looking for free virtual address space? */
	pt->pt_virtop = 0;

        /* Map in kernel. */
        if(pt_mapkernel(pt) != OK)
                vm_panic("pt_new: pt_mapkernel failed", NO_NUM);

	return OK;
}

/*===========================================================================*
 *				pt_identity		     		     *
 *===========================================================================*/
PUBLIC int pt_identity(pt_t *pt)
{
/* Allocate a pagetable that does a 1:1 mapping. */
	int i;

	/* Allocate page directory. */
        if(!pt->pt_dir &&
          !(pt->pt_dir = vm_allocpage(&pt->pt_dir_phys, VMP_PAGEDIR))) {
		return ENOMEM;
	}

	for(i = 0; i < I386_VM_DIR_ENTRIES; i++) {
		phys_bytes addr;
		addr = I386_BIG_PAGE_SIZE*i;
		pt->pt_dir[i] = (addr & I386_VM_ADDR_MASK_4MB) |
				I386_VM_BIGPAGE|
				I386_VM_USER|
				I386_VM_PRESENT|I386_VM_WRITE;
		pt->pt_pt[i] = NULL;
	}

	/* Where to start looking for free virtual address space? */
	pt->pt_virtop = 0;

	return OK;
}

/*===========================================================================*
 *                              pt_init                                      *
 *===========================================================================*/
PUBLIC void pt_init(phys_bytes usedlimit)
{
/* By default, the kernel gives us a data segment with pre-allocated
 * memory that then can't grow. We want to be able to allocate memory
 * dynamically, however. So here we copy the part of the page table
 * that's ours, so we get a private page table. Then we increase the
 * hardware segment size so we can allocate memory above our stack.
 */
        pt_t *newpt;
        int s, r;
        vir_bytes v, kpagedir;
        phys_bytes lo, hi; 
        vir_bytes extra_clicks;
        u32_t moveup = 0;
	int global_bit_ok = 0;
	int free_pde;
	int p;
	vir_bytes kernlimit;
	vir_bytes sparepages_mem;
	phys_bytes sparepages_ph;

        /* Shorthand. */
        newpt = &vmp->vm_pt;


        /* Get ourselves spare pages. */
        if(!(sparepages_mem = (vir_bytes) aalloc(I386_PAGE_SIZE*SPAREPAGES)))
		vm_panic("pt_init: aalloc for spare failed", NO_NUM);
        if((r=sys_umap(SELF, VM_D, (vir_bytes) sparepages_mem,
                I386_PAGE_SIZE*SPAREPAGES, &sparepages_ph)) != OK)
                vm_panic("pt_init: sys_umap failed", r);

        for(s = 0; s < SPAREPAGES; s++) {
        	sparepages[s].page = (void *) (sparepages_mem + s*I386_PAGE_SIZE);
        	sparepages[s].phys = sparepages_ph + s*I386_PAGE_SIZE;
        }

	missing_spares = 0;

	/* global bit and 4MB pages available? */
	global_bit_ok = _cpufeature(_CPUF_I386_PGE);
	bigpage_ok = _cpufeature(_CPUF_I386_PSE);

	/* Set bit for PTE's and PDE's if available. */
	if(global_bit_ok)
		global_bit = I386_VM_GLOBAL;

	/* The kernel and boot time processes need an identity mapping.
	 * We use full PDE's for this without separate page tables.
	 * Figure out which pde we can start using for other purposes.
	 */
	id_map_high_pde = usedlimit / I386_BIG_PAGE_SIZE;

	/* We have to make mappings up till here. */
	free_pde = id_map_high_pde+1;

        /* Initial (current) range of our virtual address space. */
        lo = CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys);
        hi = CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_phys +
                vmp->vm_arch.vm_seg[S].mem_len);
                  
        vm_assert(!(lo % I386_PAGE_SIZE)); 
        vm_assert(!(hi % I386_PAGE_SIZE));
 
        if(lo < VM_PROCSTART) {
                moveup = VM_PROCSTART - lo;
                vm_assert(!(VM_PROCSTART % I386_PAGE_SIZE));
                vm_assert(!(lo % I386_PAGE_SIZE));
                vm_assert(!(moveup % I386_PAGE_SIZE));
        }
        
        /* Make new page table for ourselves, partly copied
         * from the current one.
         */     
        if(pt_new(newpt) != OK)
                vm_panic("pt_init: pt_new failed", NO_NUM); 

        /* Set up mappings for VM process. */
        for(v = lo; v < hi; v += I386_PAGE_SIZE)  {
                phys_bytes addr;
                u32_t flags; 
        
                /* We have to write the new position in the PT,
                 * so we can move our segments.
                 */ 
                if(pt_writemap(newpt, v+moveup, v, I386_PAGE_SIZE,
                        I386_VM_PRESENT|I386_VM_WRITE|I386_VM_USER, 0) != OK)
                        vm_panic("pt_init: pt_writemap failed", NO_NUM);
        }
       
        /* Move segments up too. */
        vmp->vm_arch.vm_seg[T].mem_phys += ABS2CLICK(moveup);
        vmp->vm_arch.vm_seg[D].mem_phys += ABS2CLICK(moveup);
        vmp->vm_arch.vm_seg[S].mem_phys += ABS2CLICK(moveup);
       
	/* Allocate us a page table in which to remember page directory
	 * pointers.
	 */
	if(!(page_directories = vm_allocpage(&page_directories_phys,
		VMP_PAGETABLE)))
                vm_panic("no virt addr for vm mappings", NO_NUM);

	memset(page_directories, 0, I386_PAGE_SIZE);
       
        /* Increase our hardware data segment to create virtual address
         * space above our stack. We want to increase it to VM_DATATOP,
         * like regular processes have.
         */
        extra_clicks = ABS2CLICK(VM_DATATOP - hi);
        vmp->vm_arch.vm_seg[S].mem_len += extra_clicks;
       
        /* We pretend to the kernel we have a huge stack segment to
         * increase our data segment.
         */
        vmp->vm_arch.vm_data_top =
                (vmp->vm_arch.vm_seg[S].mem_vir +
                vmp->vm_arch.vm_seg[S].mem_len) << CLICK_SHIFT;
       
        /* Where our free virtual address space starts.
         * This is only a hint to the VM system.
         */
        newpt->pt_virtop = 0;

        /* Let other functions know VM now has a private page table. */
        vmp->vm_flags |= VMF_HASPT;

	/* Now reserve another pde for kernel's own mappings. */
	{
		int kernmap_pde;
		phys_bytes addr, len;
		int flags, index = 0;
		u32_t offset = 0;

		kernmap_pde = free_pde++;
		offset = kernmap_pde * I386_BIG_PAGE_SIZE;

		while(sys_vmctl_get_mapping(index, &addr, &len,
			&flags) == OK)  {
			vir_bytes vir;
			if(index >= MAX_KERNMAPPINGS)
                		vm_panic("VM: too many kernel mappings", index);
			kern_mappings[index].phys_addr = addr;
			kern_mappings[index].len = len;
			kern_mappings[index].flags = flags;
			kern_mappings[index].lin_addr = offset;
			kern_mappings[index].flags =
				I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE |
				global_bit;
			if(flags & VMMF_UNCACHED)
				kern_mappings[index].flags |=
					I386_VM_PWT | I386_VM_PCD;
			if(addr % I386_PAGE_SIZE)
                		vm_panic("VM: addr unaligned", addr);
			if(len % I386_PAGE_SIZE)
                		vm_panic("VM: len unaligned", len);
			vir = arch_map2vir(&vmproc[VMP_SYSTEM], offset);
			if(sys_vmctl_reply_mapping(index, vir) != OK)
                		vm_panic("VM: reply failed", NO_NUM);
			offset += len;
			index++;
			kernmappings++;
		}
	}

	/* Find a PDE below processes available for mapping in the
	 * page directories (readonly).
	 */
	pagedir_pde = free_pde++;
	pagedir_pde_val = (page_directories_phys & I386_VM_ADDR_MASK) |
			I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE;

	/* Tell kernel about free pde's. */
	while(free_pde*I386_BIG_PAGE_SIZE < VM_PROCSTART) {
		if((r=sys_vmctl(SELF, VMCTL_I386_FREEPDE, free_pde++)) != OK) {
			vm_panic("VMCTL_I386_FREEPDE failed", r);
		}
	}

	/* first pde in use by process. */
	proc_pde = free_pde;

	kernlimit = free_pde*I386_BIG_PAGE_SIZE;

	/* Increase kernel segment to address this memory. */
	if((r=sys_vmctl(SELF, VMCTL_I386_KERNELLIMIT, kernlimit)) != OK) {
                vm_panic("VMCTL_I386_KERNELLIMIT failed", r);
	}

	kpagedir = arch_map2vir(&vmproc[VMP_SYSTEM],
		pagedir_pde*I386_BIG_PAGE_SIZE);

	/* Tell kernel how to get at the page directories. */
	if((r=sys_vmctl(SELF, VMCTL_I386_PAGEDIRS, kpagedir)) != OK) {
                vm_panic("VMCTL_I386_KERNELLIMIT failed", r);
	}
       
        /* Give our process the new, copied, private page table. */
	pt_mapkernel(newpt);	/* didn't know about vm_dir pages earlier */
        pt_bind(newpt, vmp);
       
	/* Now actually enable paging. */
	if(sys_vmctl_enable_paging(vmp->vm_arch.vm_seg) != OK)
        	vm_panic("pt_init: enable paging failed", NO_NUM);

        /* Back to reality - this is where the stack actually is. */
        vmp->vm_arch.vm_seg[S].mem_len -= extra_clicks;
       
        /* All OK. */
        return;
}


/*===========================================================================*
 *				pt_bind			     		     *
 *===========================================================================*/
PUBLIC int pt_bind(pt_t *pt, struct vmproc *who)
{
	int slot, ispt;
	u32_t phys;

	/* Basic sanity checks. */
	vm_assert(who);
	vm_assert(who->vm_flags & VMF_INUSE);
	vm_assert(pt);

	slot = who->vm_slot;
	vm_assert(slot >= 0);
	vm_assert(slot < ELEMENTS(vmproc));
	vm_assert(slot < I386_VM_PT_ENTRIES);

	phys = pt->pt_dir_phys & I386_VM_ADDR_MASK;
	vm_assert(pt->pt_dir_phys == phys);

	/* Update "page directory pagetable." */
	page_directories[slot] = phys | I386_VM_PRESENT|I386_VM_WRITE;

#if 0
	printf("VM: slot %d has pde val 0x%lx\n", slot, page_directories[slot]);
#endif
	/* Tell kernel about new page table root. */
	return sys_vmctl(who->vm_endpoint, VMCTL_I386_SETCR3,
		pt ? pt->pt_dir_phys : 0);
}

/*===========================================================================*
 *				pt_free			     		     *
 *===========================================================================*/
PUBLIC void pt_free(pt_t *pt)
{
/* Free memory associated with this pagetable. */
	int i;

	for(i = 0; i < I386_VM_DIR_ENTRIES; i++)
		if(pt->pt_pt[i])
			vm_freepages((vir_bytes) pt->pt_pt[i],
				I386_VM_PFA(pt->pt_dir[i]), 1, VMP_PAGETABLE);

	return;
}

/*===========================================================================*
 *				pt_mapkernel		     		     *
 *===========================================================================*/
PUBLIC int pt_mapkernel(pt_t *pt)
{
	int r, i;

        /* Any i386 page table needs to map in the kernel address space. */
        vm_assert(vmproc[VMP_SYSTEM].vm_flags & VMF_INUSE);

	if(bigpage_ok) {
		int pde;
		for(pde = 0; pde <= id_map_high_pde; pde++) {
			phys_bytes addr;
			addr = pde * I386_BIG_PAGE_SIZE;
			vm_assert((addr & I386_VM_ADDR_MASK) == addr);
			pt->pt_dir[pde] = addr | I386_VM_PRESENT |
				I386_VM_BIGPAGE | I386_VM_USER |
				I386_VM_WRITE | global_bit;
		}
	} else {
		vm_panic("VM: pt_mapkernel: no bigpage", NO_NUM);
	}

	if(pagedir_pde >= 0) {
		/* Kernel also wants to know about all page directories. */
		pt->pt_dir[pagedir_pde] = pagedir_pde_val;
	}

	for(i = 0; i < kernmappings; i++) {
		if(pt_writemap(pt,
			kern_mappings[i].lin_addr,
			kern_mappings[i].phys_addr,
			kern_mappings[i].len,
			kern_mappings[i].flags, 0) != OK) {
			vm_panic("pt_mapkernel: pt_writemap failed", NO_NUM);
		}
	}

	return OK;
}

/*===========================================================================*
 *				pt_cycle		     		     *
 *===========================================================================*/
PUBLIC void pt_cycle(void)
{
	vm_checkspares();
}

