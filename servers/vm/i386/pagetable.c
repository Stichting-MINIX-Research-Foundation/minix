
#define _SYSTEM 1

#define VERBOSE 0

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

#include <errno.h>
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

/* Location in our virtual address space where we can map in 
 * any physical page we want.
*/
static unsigned char *varmap = NULL;	/* Our address space. */
static u32_t varmap_loc;		/* Our page table. */

/* Our process table entry. */
struct vmproc *vmp = &vmproc[VM_PROC_NR];

/* Spare memory, ready to go after initialization, to avoid a
 * circular dependency on allocating memory and writing it into VM's
 * page table.
 */
#define SPAREPAGES 3
static struct {
	void *page;
	u32_t phys;
} sparepages[SPAREPAGES];

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

#if SANITYCHECKS
#define PT_SANE(p) { pt_sanitycheck((p), __FILE__, __LINE__); SANITYCHECK(SCL_DETAIL); }
/*===========================================================================*
 *				pt_sanitycheck		     		     *
 *===========================================================================*/
PUBLIC void pt_sanitycheck(pt_t *pt, char *file, int line)
{
/* Basic pt sanity check. */
	int i;

	MYASSERT(pt);
	MYASSERT(pt->pt_dir);
	MYASSERT(pt->pt_dir_phys);

	for(i = 0; i < I386_VM_DIR_ENTRIES; i++) {
		if(pt->pt_pt[i]) {
			MYASSERT(pt->pt_dir[i] & I386_VM_PRESENT);
		} else {
			MYASSERT(!(pt->pt_dir[i] & I386_VM_PRESENT));
		}
	}
}
#else
#define PT_SANE(p)
#endif

/*===========================================================================*
 *				aalloc			     		     *
 *===========================================================================*/
PRIVATE void *aalloc(size_t bytes)
{
/* Page-aligned malloc(). only used if vm_allocpages can't be used.  */
	u32_t b;

	b = (u32_t) malloc(I386_PAGE_SIZE + bytes);
	if(!b) vm_panic("aalloc: out of memory", bytes);
	b += I386_PAGE_SIZE - (b % I386_PAGE_SIZE);

	return (void *) b;
}

/*===========================================================================*
 *				findhole		     		     *
 *===========================================================================*/
PRIVATE u32_t findhole(pt_t *pt, u32_t virbytes, u32_t vmin, u32_t vmax)
{
/* Find a space in the virtual address space of pageteble 'pt',
 * between page-aligned BYTE offsets vmin and vmax, to fit
 * 'virbytes' in. Return byte offset.
 *
 * As a simple way to speed up the search a bit, we start searching
 * after the location we found the previous hole, if that's in range.
 * If that's not in range (or if that doesn't work), search the entire
 * range (as well). try_restart controls whether we have to restart
 * the search if it fails. (Just once of course.)
 */
	u32_t freeneeded, freefound = 0, freestart = 0, curv;
	int pde = 0, try_restart;

	/* Input sanity check. */
	vm_assert(vmin + virbytes >= vmin);
	vm_assert(vmax >= vmin + virbytes);
	vm_assert((virbytes % I386_PAGE_SIZE) == 0);
	vm_assert((vmin % I386_PAGE_SIZE) == 0);
	vm_assert((vmax % I386_PAGE_SIZE) == 0);

	/* How many pages do we need? */
	freeneeded = virbytes / I386_PAGE_SIZE;

	if(pt->pt_virtop >= vmin && pt->pt_virtop <= vmax - virbytes) {
		curv = pt->pt_virtop;
		try_restart = 1;
	} else {
		curv = vmin;
		try_restart = 0;
	}


	/* Start looking for a consecutive block of free pages
	 * starting at vmin.
	 */
	for(freestart = curv; curv < vmax; ) {
		int pte;
		pde = I386_VM_PDE(curv);
		pte = I386_VM_PTE(curv);

		if(!(pt->pt_dir[pde] & I386_VM_PRESENT)) {
			int rempte;
			rempte = I386_VM_PT_ENTRIES - pte;
			freefound += rempte;
			curv += rempte * I386_PAGE_SIZE;
		} else {
			if(pt->pt_pt[pde][pte] & I386_VM_PRESENT) {
				freefound = 0;
				freestart = curv + I386_PAGE_SIZE;
			} else {
				freefound++;
			}
			curv+=I386_PAGE_SIZE;
		}

		if(freefound >= freeneeded) {
			u32_t v;
			v = freestart;
			vm_assert(v != NO_MEM);
			vm_assert(v >= vmin);
			vm_assert(v < vmax);

			/* Next time, start looking here. */
			pt->pt_virtop = v + virbytes;

			return v;
		}

		if(curv >= vmax && try_restart) {
			curv = vmin;
			try_restart = 0;
		}
	}

	printf("VM: out of virtual address space in a process\n");

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
		if(pt_writemap(&vmp->vm_pt,
			vir + CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys),
			0, pages*I386_PAGE_SIZE, 0, WMF_OVERWRITE) != OK)
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
	for(s = 0; s < SPAREPAGES; s++) {
		if(sparepages[s].page) {
			void *sp;
			sp = sparepages[s].page;
			*phys = sparepages[s].phys;
			sparepages[s].page = NULL;
			return sp;
		}
	}
	vm_panic("VM: out of spare pages", NO_NUM);
	return NULL;
}

/*===========================================================================*
 *				vm_checkspares		     		     *
 *===========================================================================*/
PRIVATE void *vm_checkspares(void)
{
	int s, n = 0;
	static int total = 0, worst = 0;
	for(s = 0; s < SPAREPAGES; s++)
	    if(!sparepages[s].page) {
		n++;
		sparepages[s].page = vm_allocpages(&sparepages[s].phys, 1,
			VMP_SPARE);
	}
	if(worst < n) worst = n;
	total += n;
#if 0
	if(n > 0)
		printf("VM: made %d spares, total %d, worst %d\n", n, total, worst);
#endif
	return NULL;
}

/*===========================================================================*
 *				vm_allocpages		     		     *
 *===========================================================================*/
PUBLIC void *vm_allocpages(phys_bytes *phys, int pages, int reason)
{
/* Allocate a number of pages for use by VM itself. */
	phys_bytes newpage;
	vir_bytes loc;
	pt_t *pt;
	int r;
	vir_bytes bytes = pages * I386_PAGE_SIZE;
	static int level = 0;
#define MAXDEPTH 10
	static int reasons[MAXDEPTH];

	pt = &vmp->vm_pt;
	vm_assert(reason >= 0 && reason < VMP_CATEGORIES);
	vm_assert(pages > 0);

	reasons[level++] = reason;

	vm_assert(level >= 1);
	vm_assert(level <= 2);

	if(level > 1 || !(vmp->vm_flags & VMF_HASPT)) {
		int r;
		void *s;
		vm_assert(pages == 1);
		s=vm_getsparepage(phys);
		level--;
		return s;
	}

	/* VM does have a pagetable, so get a page and map it in there.
	 * Where in our virtual address space can we put it?
	 */
	loc = findhole(pt, I386_PAGE_SIZE * pages,
		CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys) + vmp->vm_stacktop,
		vmp->vm_arch.vm_data_top);
	if(loc == NO_MEM) {
		level--;
		return NULL;
	}

	/* Allocate 'pages' pages of memory for use by VM. As VM
	 * is trusted, we don't have to pre-clear it.
	 */
	if((newpage = ALLOC_MEM(CLICKSPERPAGE * pages, 0)) == NO_MEM) {
		level--;
		return NULL;
	}

	*phys = CLICK2ABS(newpage);

	/* Map this page into our address space. */
	if((r=pt_writemap(pt, loc, *phys, bytes,
		I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE, 0)) != OK) {
		FREE_MEM(newpage, CLICKSPERPAGE * pages / I386_PAGE_SIZE);
		return NULL;
	}

	level--;

	/* Return user-space-ready pointer to it. */
	return (void *) (loc - CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys));
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
	vm_assert(!(flags & ~(PTF_ALLFLAGS | PTF_MAPALLOC)));

	/* We don't expect to overwrite page directory entry, nor
	 * storage for the page table.
	 */
	vm_assert(!(pt->pt_dir[pde] & I386_VM_PRESENT));
	vm_assert(!pt->pt_pt[pde]);
	PT_SANE(pt);

	/* Get storage for the page table. */
        if(!(pt->pt_pt[pde] = vm_allocpages(&pt_phys, 1, VMP_PAGETABLE)))
		return ENOMEM;

	for(i = 0; i < I386_VM_PT_ENTRIES; i++)
		pt->pt_pt[pde][i] = 0;	/* Empty entry. */

	/* Make page directory entry.
	 * The PDE is always 'present,' 'writable,' and 'user accessible,'
	 * relying on the PTE for protection.
	 */
	pt->pt_dir[pde] = (pt_phys & I386_VM_ADDR_MASK) | flags
		| I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE;
	vm_assert(flags & I386_VM_PRESENT);
	PT_SANE(pt);

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
	int p, pages, pde;
	SANITYCHECK(SCL_FUNCTIONS);

	vm_assert(!(bytes % I386_PAGE_SIZE));
	vm_assert(!(flags & ~(PTF_ALLFLAGS | PTF_MAPALLOC)));

	pages = bytes / I386_PAGE_SIZE;

#if SANITYCHECKS
	if(physaddr && !(flags & I386_VM_PRESENT)) {
		vm_panic("pt_writemap: writing dir with !P\n", NO_NUM);
	}
	if(!physaddr && flags) {
		vm_panic("pt_writemap: writing 0 with flags\n", NO_NUM);
	}
#endif

	PT_SANE(pt);

	/* First make sure all the necessary page tables are allocated,
	 * before we start writing in any of them, because it's a pain
	 * to undo our work properly. Walk the range in page-directory-entry
	 * sized leaps.
	 */
	for(pde = I386_VM_PDE(v); pde <= I386_VM_PDE(v + I386_PAGE_SIZE * pages); pde++) {
		vm_assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);
		if(!(pt->pt_dir[pde] & I386_VM_PRESENT)) {
			int r;
			vm_assert(!pt->pt_dir[pde]);
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
		}
		vm_assert(pt->pt_dir[pde] & I386_VM_PRESENT);
	}

	PT_SANE(pt);

	/* Now write in them. */
	for(p = 0; p < pages; p++) {
		int pde = I386_VM_PDE(v);
		int pte = I386_VM_PTE(v);
	PT_SANE(pt);

		vm_assert(!(v % I386_PAGE_SIZE));
		vm_assert(pte >= 0 && pte < I386_VM_PT_ENTRIES);
		vm_assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);

		/* Page table has to be there. */
		vm_assert(pt->pt_dir[pde] & I386_VM_PRESENT);

		/* Make sure page directory entry for this page table
		 * is marked present and page table entry is available.
		 */
		vm_assert((pt->pt_dir[pde] & I386_VM_PRESENT) && pt->pt_pt[pde]);

	PT_SANE(pt);
#if SANITYCHECKS
		/* We don't expect to overwrite a page. */
		if(!(writemapflags & WMF_OVERWRITE))
			vm_assert(!(pt->pt_pt[pde][pte] & I386_VM_PRESENT));
#endif

		/* Write pagetable entry. */
		pt->pt_pt[pde][pte] = (physaddr & I386_VM_ADDR_MASK) | flags;

		physaddr += I386_PAGE_SIZE;
		v += I386_PAGE_SIZE;
	PT_SANE(pt);
	}
	SANITYCHECK(SCL_FUNCTIONS);
	PT_SANE(pt);

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

        if(!(pt->pt_dir = vm_allocpages(&pt->pt_dir_phys, 1, VMP_PAGEDIR))) {
		return ENOMEM;
	}

	for(i = 0; i < I386_VM_DIR_ENTRIES; i++) {
		pt->pt_dir[i] = 0; /* invalid entry (I386_VM_PRESENT bit = 0) */
		pt->pt_pt[i] = NULL;
	}

	/* Where to start looking for free virtual address space? */
	pt->pt_virtop = VM_STACKTOP +
		CLICK2ABS(vmproc[VMP_SYSTEM].vm_arch.vm_seg[D].mem_phys);

	return OK;
}



/*===========================================================================*
 *				pt_allocmap		     		     *
 *===========================================================================*/
PUBLIC int pt_allocmap(pt_t *pt, vir_bytes v_min, vir_bytes v_max,
	size_t bytes, u32_t pageflags, u32_t memflags, vir_bytes *v_final)
{
/* Allocate new memory, and map it into the page table. */
	u32_t newpage;
	u32_t v;
	int r;

	/* Input sanity check. */
	PT_SANE(pt);
	vm_assert(!(pageflags & ~PTF_ALLFLAGS));

	/* Valid no-op. */
	if(bytes == 0) return OK;

	/* Round no. of bytes up to a page. */
	if(bytes % I386_PAGE_SIZE) {
		bytes += I386_PAGE_SIZE - (bytes % I386_PAGE_SIZE);
	}

	/* Special case; if v_max is 0, the request is to map the memory
	 * into v_min at exactly that location. We raise v_max as necessary,
	 * so the check to see if the virtual space is free does happen.
	 */
	if(v_max == 0) {
		v_max = v_min + bytes;

		/* Sanity check. */
		if(v_max < v_min) {
			printf("pt_allocmap: v_min 0x%lx and bytes 0x%lx\n",
				v_min, bytes);
			return ENOMEM;
		}
	}

	/* Basic sanity check. */
	if(v_max < v_min) {
		printf("pt_allocmap: v_min 0x%lx, v_max 0x%lx\n", v_min, v_max);
		return ENOMEM;
	}

	/* v_max itself may not be used. Bytes may be 0. */
	if(v_max < v_min + bytes) {
		printf("pt_allocmap: v_min 0x%lx, bytes 0x%lx, v_max 0x%lx\n",
			v_min, bytes, v_max);
		return ENOMEM;
	}

	/* Find where to fit this into the virtual address space. */
	v = findhole(pt, bytes, v_min, v_max);
	if(v == NO_MEM) {
		printf("pt_allocmap: no hole found to map 0x%lx bytes into\n",
			bytes);
		return ENOSPC;
	}

	vm_assert(!(v % I386_PAGE_SIZE));

	if(v_final) *v_final = v;

	/* Memory is currently always allocated contiguously physically,
	 * but if that were to change, note the setting of
	 * PAF_CONTIG in memflags.
	 */

	newpage = ALLOC_MEM(CLICKSPERPAGE * bytes / I386_PAGE_SIZE, memflags);
	if(newpage == NO_MEM) {
		printf("pt_allocmap: out of memory\n");
		return ENOMEM;
	}

	/* Write into the page table. */
	if((r=pt_writemap(pt, v, CLICK2ABS(newpage), bytes,
		pageflags | PTF_MAPALLOC, 0)) != OK) {
		FREE_MEM(newpage, CLICKSPERPAGE * bytes / I386_PAGE_SIZE);
		return r;
	}

	/* Sanity check result. */
	PT_SANE(pt);

	return OK;
}

/*===========================================================================*
 *				raw_readmap		     		     *
 *===========================================================================*/
PRIVATE int raw_readmap(phys_bytes root, u32_t v, u32_t *phys, u32_t *flags)
{
	u32_t dir[I386_VM_DIR_ENTRIES];
	u32_t tab[I386_VM_PT_ENTRIES];
	int pde, pte, r;

	/* Sanity check. */
	vm_assert((root % I386_PAGE_SIZE) == 0);
	vm_assert((v % I386_PAGE_SIZE) == 0);

	/* Get entry in page directory. */
	pde = I386_VM_PDE(v);
	if((r=sys_physcopy(SYSTEM, PHYS_SEG, root,
		SELF, VM_D, (phys_bytes) dir, sizeof(dir))) != OK) {
		printf("VM: raw_readmap: sys_physcopy failed (dir) (%d)\n", r);
		return EFAULT;
	}

	if(!(dir[pde] & I386_VM_PRESENT)) {
		printf("raw_readmap: 0x%lx: pde %d not present: 0x%lx\n",
			v, pde, dir[pde]);
		return EFAULT;
	}

	/* Get entry in page table. */
	if((r=sys_physcopy(SYSTEM, PHYS_SEG, I386_VM_PFA(dir[pde]),
		SELF, VM_D, (vir_bytes) tab, sizeof(tab))) != OK) {
		printf("VM: raw_readmap: sys_physcopy failed (tab) (r)\n");
		return EFAULT;
	}
	pte = I386_VM_PTE(v);
	if(!(tab[pte] & I386_VM_PRESENT)) {
		printf("raw_readmap: 0x%lx: pde %d not present: 0x%lx\n",
			v, pte, tab[pte]);
		return EFAULT;
	}

	/* Get address and flags. */
	*phys = I386_VM_PFA(tab[pte]);
	*flags = tab[pte] & PTF_ALLFLAGS;
	
	return OK;
}

/*===========================================================================*
 *				pt_init			     		     *
 *===========================================================================*/
PUBLIC void pt_init(void)
{
/* By default, the kernel gives us a data segment with pre-allocated
 * memory that then can't grow. We want to be able to allocate memory
 * dynamically, however. So here we copy the part of the page table
 * that's ours, so we get a private page table. Then we increase the
 * hardware segment size so we can allocate memory above our stack.
 */
	u32_t my_cr3;
	pt_t *newpt;
	int s, r;
	vir_bytes v;
	phys_bytes lo, hi;
	vir_bytes extra_clicks;

	/* Retrieve current CR3 - shared page table. */
	if((r=sys_vmctl_get_cr3_i386(SELF, &my_cr3)) != OK)
		vm_panic("pt_init: sys_vmctl_get_cr3_i386 failed", r);

	/* Shorthand. */
	newpt = &vmp->vm_pt;

	/* Get ourselves a spare page. */
	for(s = 0; s < SPAREPAGES; s++) {
		if(!(sparepages[s].page = aalloc(I386_PAGE_SIZE)))
			vm_panic("pt_init: aalloc for spare failed", NO_NUM);
		if((r=sys_umap(SELF, VM_D, (vir_bytes) sparepages[s].page,
			I386_PAGE_SIZE, &sparepages[s].phys)) != OK)
			vm_panic("pt_init: sys_umap failed", r);
	}

	/* Make new page table for ourselves, partly copied
	 * from the current one.
	 */
	if(pt_new(newpt) != OK)
		vm_panic("pt_init: pt_new failed", NO_NUM);

	/* Initial (current) range of our virtual address space. */
	lo = CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys);
	hi = CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_phys +
		vmp->vm_arch.vm_seg[S].mem_len);

	/* Copy the mappings from the shared page table to our private one. */
	for(v = lo; v < hi; v += I386_PAGE_SIZE)  {
		phys_bytes addr;
		u32_t flags;
		if(raw_readmap(my_cr3, v, &addr, &flags) != OK)
			vm_panic("pt_init: raw_readmap failed", NO_NUM);
		if(pt_writemap(newpt, v, addr, I386_PAGE_SIZE, flags, 0) != OK)
			vm_panic("pt_init: pt_writemap failed", NO_NUM);
	}

	/* Map in kernel. */
	if(pt_mapkernel(newpt) != OK)
		vm_panic("pt_init: pt_mapkernel failed", NO_NUM);

	/* Give our process the new, copied, private page table. */
	pt_bind(newpt, vmp);

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

	if((s=sys_newmap(VM_PROC_NR, vmp->vm_arch.vm_seg)) != OK)
		vm_panic("VM: pt_init: sys_newmap failed", s);

	/* Back to reality - this is where the stack actually is. */
	vmp->vm_arch.vm_seg[S].mem_len -= extra_clicks;

	/* Where our free virtual address space starts.
	 * This is only a hint to the VM system.
	 */
	newpt->pt_virtop = (vmp->vm_arch.vm_seg[S].mem_vir +
		vmp->vm_arch.vm_seg[S].mem_len) << CLICK_SHIFT;

	/* Let other functions know VM now has a private page table. */
	vmp->vm_flags |= VMF_HASPT;

	/* Reserve a page in our virtual address space that we
	 * can use to map in arbitrary physical pages.
	 */
	varmap_loc = findhole(newpt, I386_PAGE_SIZE,
		CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys) + vmp->vm_stacktop,
		vmp->vm_arch.vm_data_top);
	if(varmap_loc == NO_MEM) {
		vm_panic("no virt addr for vm mappings", NO_NUM);
	}
	varmap = (unsigned char *) (varmap_loc -
		CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys));

	/* All OK. */
	return;
}

/*===========================================================================*
 *				pt_bind			     		     *
 *===========================================================================*/
PUBLIC int pt_bind(pt_t *pt, struct vmproc *who)
{
	/* Basic sanity checks. */
	vm_assert(who);
	vm_assert(who->vm_flags & VMF_INUSE);
	if(pt) PT_SANE(pt);

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

	PT_SANE(pt);

	for(i = 0; i < I386_VM_DIR_ENTRIES; i++) {
		int p;
		if(pt->pt_pt[i]) {
		   for(p = 0; p < I386_VM_PT_ENTRIES; p++) {
			if((pt->pt_pt[i][p] & (PTF_MAPALLOC | I386_VM_PRESENT)) 
			 == (PTF_MAPALLOC | I386_VM_PRESENT)) {
					u32_t pa = I386_VM_PFA(pt->pt_pt[i][p]);
					FREE_MEM(ABS2CLICK(pa), CLICKSPERPAGE);
				}
		  }
		  vm_freepages((vir_bytes) pt->pt_pt[i],
			I386_VM_PFA(pt->pt_dir[i]), 1, VMP_PAGETABLE);
		}
	}

	vm_freepages((vir_bytes) pt->pt_dir, pt->pt_dir_phys, 1, VMP_PAGEDIR);

	return;
}

/*===========================================================================*
 *				pt_mapkernel		     		     *
 *===========================================================================*/
PUBLIC int pt_mapkernel(pt_t *pt)
{
	int r;

        /* Any i386 page table needs to map in the kernel address space. */
        vm_assert(vmproc[VMP_SYSTEM].vm_flags & VMF_INUSE);

        /* Map in text. flags: don't write, supervisor only */
        if((r=pt_writemap(pt, KERNEL_TEXT, KERNEL_TEXT, KERNEL_TEXT_LEN,
                 I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE, 0)) != OK)
		return r;
 
        /* Map in data. flags: read-write, supervisor only */
        if((r=pt_writemap(pt, KERNEL_DATA, KERNEL_DATA, KERNEL_DATA_LEN,
                I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE, 0)) != OK)
		return r;

	return OK;
}

/*===========================================================================*
 *				pt_freerange		     		     *
 *===========================================================================*/
PUBLIC void pt_freerange(pt_t *pt, vir_bytes low, vir_bytes high)
{
/* Free memory allocated by pagetable functions in this range. */
	int pde;
	u32_t v;

	PT_SANE(pt);

	for(v = low; v < high; v += I386_PAGE_SIZE) {
		int pte;
		pde = I386_VM_PDE(v);
		pte = I386_VM_PTE(v);
		if(!(pt->pt_dir[pde] & I386_VM_PRESENT))
			continue;
		if((pt->pt_pt[pde][pte] & (PTF_MAPALLOC | I386_VM_PRESENT)) 
		 == (PTF_MAPALLOC | I386_VM_PRESENT)) {
			u32_t pa = I386_VM_PFA(pt->pt_pt[pde][pte]);
			FREE_MEM(ABS2CLICK(pa), CLICKSPERPAGE);
			pt->pt_pt[pde][pte] = 0;
		}
	}

	PT_SANE(pt);

	return;
}

/*===========================================================================*
 *				pt_cycle		     		     *
 *===========================================================================*/
PUBLIC void pt_cycle(void)
{
	vm_checkspares();
}

/*===========================================================================*
 *				pt_copy			     		     *
 *===========================================================================*/
PUBLIC int pt_copy(pt_t *src, pt_t *dst)
{
	int i, r;

	SANITYCHECK(SCL_FUNCTIONS);
	PT_SANE(src);

	if((r=pt_new(dst)) != OK)
		return r;

	for(i = 0; i < I386_VM_DIR_ENTRIES; i++) {
		int p; 
		if(!(src->pt_dir[i] & I386_VM_PRESENT))
			continue;
		for(p = 0; p < I386_VM_PT_ENTRIES; p++) {
			u32_t v = i * I386_VM_PT_ENTRIES * I386_PAGE_SIZE +
				p * I386_PAGE_SIZE;
			u32_t pa1, pa2, flags;
			if(!(src->pt_pt[i][p] & I386_VM_PRESENT))
				continue;
#if 0
			if((dst->pt_pt[i] &&
			   (dst->pt_pt[i][p] & I386_VM_PRESENT)))
				continue;
#endif
			flags = src->pt_pt[i][p] & (PTF_WRITE | PTF_USER);
			flags |= I386_VM_PRESENT;
			pa1 = I386_VM_PFA(src->pt_pt[i][p]);
			if(PTF_MAPALLOC & src->pt_pt[i][p]) {
				PT_SANE(dst);
				if(pt_allocmap(dst, v, 0,
					I386_PAGE_SIZE, flags, 0, NULL) != OK) {
					pt_free(dst);
					return ENOMEM;
				}
				pa2 = I386_VM_PFA(dst->pt_pt[i][p]);
				sys_abscopy(pa1, pa2, I386_PAGE_SIZE);
			} else {
				PT_SANE(dst);
				if(pt_writemap(dst, v, pa1, I386_PAGE_SIZE, flags, 0) != OK) {
					pt_free(dst);
					return ENOMEM;
				}
			}
		}
	}

	PT_SANE(src);
	PT_SANE(dst);
	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}

#define PHYS_MAP(a, o)							\
{	int r;								\
	vm_assert(varmap);						\
	(o) = (a) % I386_PAGE_SIZE;					\
	r = pt_writemap(&vmp->vm_pt, varmap_loc, (a) - (o), I386_PAGE_SIZE, \
		I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE, 0); 	\
	if(r != OK)							\
		vm_panic("PHYS_MAP: pt_writemap failed", NO_NUM);	\
	/* pt_bind() flushes TLB. */					\
	pt_bind(&vmp->vm_pt, vmp);					\
}

#define PHYSMAGIC 0x7b9a0590

#define PHYS_UNMAP if(OK != pt_writemap(&vmp->vm_pt, varmap_loc, 0, 	\
	I386_PAGE_SIZE, 0, WMF_OVERWRITE)) {				\
		vm_panic("PHYS_UNMAP: pt_writemap failed", NO_NUM); }

#define PHYS_VAL(o) (* (phys_bytes *) (varmap + (o)))

/*===========================================================================*
 *                              phys_writeaddr                               *
 *===========================================================================*/
PUBLIC void phys_writeaddr(phys_bytes addr, phys_bytes v1, phys_bytes v2)
{
	phys_bytes offset;

	SANITYCHECK(SCL_DETAIL);
	PHYS_MAP(addr, offset);
	PHYS_VAL(offset) = v1;
	PHYS_VAL(offset + sizeof(phys_bytes)) = v2;
#if SANITYCHECKS
	PHYS_VAL(offset + 2*sizeof(phys_bytes)) = PHYSMAGIC;
#endif
	PHYS_UNMAP;
	SANITYCHECK(SCL_DETAIL);
}

/*===========================================================================*
 *                              phys_readaddr                                *
 *===========================================================================*/
PUBLIC void phys_readaddr(phys_bytes addr, phys_bytes *v1, phys_bytes *v2)
{
	phys_bytes offset;

	SANITYCHECK(SCL_DETAIL);
	PHYS_MAP(addr, offset);
	*v1 = PHYS_VAL(offset);
	*v2 = PHYS_VAL(offset + sizeof(phys_bytes));
#if SANITYCHECKS
	vm_assert(PHYS_VAL(offset + 2*sizeof(phys_bytes)) == PHYSMAGIC);
#endif
	PHYS_UNMAP;
	SANITYCHECK(SCL_DETAIL);
}
