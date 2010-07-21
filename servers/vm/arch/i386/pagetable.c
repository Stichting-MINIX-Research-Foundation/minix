
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

#include "memory.h"

/* PDE used to map in kernel, kernel physical address. */
PRIVATE int id_map_high_pde = -1, pagedir_pde = -1;
PRIVATE u32_t global_bit = 0, pagedir_pde_val;

PRIVATE int proc_pde = 0;

/* 4MB page size available in hardware? */
PRIVATE int bigpage_ok = 0;

/* Our process table entry. */
struct vmproc *vmprocess = &vmproc[VM_PROC_NR];

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

#define STATIC_SPAREPAGES 10

PRIVATE char static_sparepages[I386_PAGE_SIZE*STATIC_SPAREPAGES + I386_PAGE_SIZE];

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
		panic("pt_sanitycheck: passed pt not in any proc");
	}

	MYASSERT(usedpages_add(pt->pt_dir_phys, I386_PAGE_SIZE) == OK);

	for(i = proc_pde; i < I386_VM_DIR_ENTRIES; i++) {
		if(pt->pt_pt[i]) {
			int pte;
			MYASSERT(vm_addrok(pt->pt_pt[i], 1));
			if(!(pt->pt_dir[i] & I386_VM_PRESENT)) {
				printf("slot %d: pt->pt_pt[%d] = %p, but pt_dir entry 0x%lx\n",
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
	assert(vmin + I386_PAGE_SIZE >= vmin);
	assert(vmax >= vmin + I386_PAGE_SIZE);
	assert((vmin % I386_PAGE_SIZE) == 0);
	assert((vmax % I386_PAGE_SIZE) == 0);

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

		assert(curv >= vmin);
		assert(curv < vmax);

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
	assert(reason >= 0 && reason < VMP_CATEGORIES);
	if(vir >= vmprocess->vm_stacktop) {
		assert(!(vir % I386_PAGE_SIZE)); 
		assert(!(phys % I386_PAGE_SIZE)); 
		free_mem(ABS2CLICK(phys), pages);
		if(pt_writemap(&vmprocess->vm_pt, arch_vir2map(vmprocess, vir),
			MAP_NONE, pages*I386_PAGE_SIZE, 0, WMF_OVERWRITE) != OK)
			panic("vm_freepages: pt_writemap failed");
	} else {
		printf("VM: vm_freepages not freeing VM heap pages (%d)\n",
			pages);
	}

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
PRIVATE void *vm_getsparepage(u32_t *phys)
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
	return NULL;
}

/*===========================================================================*
 *				vm_checkspares		     		     *
 *===========================================================================*/
PRIVATE void *vm_checkspares(void)
{
	int s, n = 0;
	static int total = 0, worst = 0;
	assert(missing_spares >= 0 && missing_spares <= SPAREPAGES);
	for(s = 0; s < SPAREPAGES && missing_spares > 0; s++)
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

	pt = &vmprocess->vm_pt;
	assert(reason >= 0 && reason < VMP_CATEGORIES);

	level++;

	assert(level >= 1);
	assert(level <= 2);

	if(level > 1 || !(vmprocess->vm_flags & VMF_HASPT) || !meminit_done) {
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
	loc = findhole(pt,  arch_vir2map(vmprocess, vmprocess->vm_stacktop),
		vmprocess->vm_arch.vm_data_top);
	if(loc == NO_MEM) {
		level--;
		printf("VM: vm_allocpage: findhole failed\n");
		return NULL;
	}

	/* Allocate page of memory for use by VM. As VM
	 * is trusted, we don't have to pre-clear it.
	 */
	if((newpage = alloc_mem(CLICKSPERPAGE, 0)) == NO_MEM) {
		level--;
		printf("VM: vm_allocpage: alloc_mem failed\n");
		return NULL;
	}

	*phys = CLICK2ABS(newpage);

	/* Map this page into our address space. */
	if((r=pt_writemap(pt, loc, *phys, I386_PAGE_SIZE,
		I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE, 0)) != OK) {
		free_mem(newpage, CLICKSPERPAGE);
		printf("vm_allocpage writemap failed\n");
		level--;
		return NULL;
	}

	if((r=sys_vmctl(SELF, VMCTL_FLUSHTLB, 0)) != OK) {
		panic("VMCTL_FLUSHTLB failed: %d", r);
	}

	level--;

	/* Return user-space-ready pointer to it. */
	ret = (void *) arch_map2vir(vmprocess, loc);

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

	pt = &vmprocess->vm_pt;
	m = arch_vir2map(vmprocess, (vir_bytes) vir);

	assert(!(m % I386_PAGE_SIZE));

	if(!lockflag)
		flags |= I386_VM_WRITE;

	/* Update flags. */
	if((r=pt_writemap(pt, m, 0, I386_PAGE_SIZE,
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
PUBLIC int vm_addrok(void *vir, int writeflag)
{
/* Mark a page allocated by vm_allocpage() unwritable, i.e. only for VM. */
	pt_t *pt = &vmprocess->vm_pt;
	int pde, pte;
	vir_bytes v = arch_vir2map(vmprocess, (vir_bytes) vir);

	/* No PT yet? Don't bother looking. */
	if(!(vmprocess->vm_flags & VMF_HASPT)) {
		return 1;
	}

	pde = I386_VM_PDE(v);
	pte = I386_VM_PTE(v);

	if(!(pt->pt_dir[pde] & I386_VM_PRESENT)) {
		printf("addr not ok: missing pde %d\n", pde);
		return 0;
	}

	if(writeflag &&
		!(pt->pt_dir[pde] & I386_VM_WRITE)) {
		printf("addr not ok: pde %d present but pde unwritable\n", pde);
		return 0;
	}

	if(!(pt->pt_pt[pde][pte] & I386_VM_PRESENT)) {
		printf("addr not ok: missing pde %d / pte %d\n",
			pde, pte);
		return 0;
	}

	if(writeflag &&
		!(pt->pt_pt[pde][pte] & I386_VM_WRITE)) {
		printf("addr not ok: pde %d / pte %d present but unwritable\n",
			pde, pte);
		return 0;
	}

	return 1;
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
	assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);
	assert(!(flags & ~(PTF_ALLFLAGS)));

	/* We don't expect to overwrite page directory entry, nor
	 * storage for the page table.
	 */
	assert(!(pt->pt_dir[pde] & I386_VM_PRESENT));
	assert(!pt->pt_pt[pde]);

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
 *			    pt_ptalloc_in_range		     		     *
 *===========================================================================*/
PUBLIC int pt_ptalloc_in_range(pt_t *pt, vir_bytes start, vir_bytes end,
	u32_t flags, int verify)
{
/* Allocate all the page tables in the range specified. */
	int pde, first_pde, last_pde;

	first_pde = start ? I386_VM_PDE(start) : proc_pde;
	last_pde = end ? I386_VM_PDE(end) : I386_VM_DIR_ENTRIES - 1;
	assert(first_pde >= 0);
	assert(last_pde < I386_VM_DIR_ENTRIES);

	/* Scan all page-directory entries in the range. */
	for(pde = first_pde; pde <= last_pde; pde++) {
		assert(!(pt->pt_dir[pde] & I386_VM_BIGPAGE));
		if(!(pt->pt_dir[pde] & I386_VM_PRESENT)) {
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
				printf("pt_ptalloc_in_range: pt_ptalloc failed\n");
				return r;
			}
		}
		assert(pt->pt_dir[pde] & I386_VM_PRESENT);
	}

	return OK;
}

PRIVATE char *ptestr(u32_t pte)
{
#define FLAG(constant, name) {						\
	if(pte & (constant)) { strcat(str, name); strcat(str, " "); }	\
}

	static char str[30];
	if(!(pte & I386_VM_PRESENT)) {
		return "not present";
	}
	str[0] = '\0';
	FLAG(I386_VM_WRITE, "W");
	FLAG(I386_VM_USER, "U");
	FLAG(I386_VM_PWT, "PWT");
	FLAG(I386_VM_PCD, "PCD");
	FLAG(I386_VM_ACC, "ACC");
	FLAG(I386_VM_DIRTY, "DIRTY");
	FLAG(I386_VM_PS, "PS");
	FLAG(I386_VM_GLOBAL, "G");
	FLAG(I386_VM_PTAVAIL1, "AV1");
	FLAG(I386_VM_PTAVAIL2, "AV2");
	FLAG(I386_VM_PTAVAIL3, "AV3");

	return str;
}

/*===========================================================================*
 *			     pt_map_in_range		     		     *
 *===========================================================================*/
PUBLIC int pt_map_in_range(struct vmproc *src_vmp, struct vmproc *dst_vmp,
	vir_bytes start, vir_bytes end)
{
/* Transfer all the mappings from the pt of the source process to the pt of
 * the destination process in the range specified.
 */
	int pde, pte;
	int r;
	vir_bytes viraddr, mapaddr;
	pt_t *pt, *dst_pt;

	pt = &src_vmp->vm_pt;
	dst_pt = &dst_vmp->vm_pt;

	end = end ? end : VM_DATATOP;
	assert(start % I386_PAGE_SIZE == 0);
	assert(end % I386_PAGE_SIZE == 0);
	assert(I386_VM_PDE(start) >= proc_pde && start <= end);
	assert(I386_VM_PDE(end) < I386_VM_DIR_ENTRIES);

#if LU_DEBUG
	printf("VM: pt_map_in_range: src = %d, dst = %d\n",
		src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
	printf("VM: pt_map_in_range: transferring from 0x%08x (pde %d pte %d) to 0x%08x (pde %d pte %d)\n",
		start, I386_VM_PDE(start), I386_VM_PTE(start),
		end, I386_VM_PDE(end), I386_VM_PTE(end));
#endif

	/* Scan all page-table entries in the range. */
	for(viraddr = start; viraddr <= end; viraddr += I386_PAGE_SIZE) {
		pde = I386_VM_PDE(viraddr);
		if(!(pt->pt_dir[pde] & I386_VM_PRESENT)) {
			if(viraddr == VM_DATATOP) break;
			continue;
		}
		pte = I386_VM_PTE(viraddr);
		if(!(pt->pt_pt[pde][pte] & I386_VM_PRESENT)) {
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
PUBLIC int pt_ptmap(struct vmproc *src_vmp, struct vmproc *dst_vmp)
{
/* Transfer mappings to page dir and page tables from source process and
 * destination process. Make sure all the mappings are above the stack, not
 * to corrupt valid mappings in the data segment of the destination process.
 */
	int pde, r;
	phys_bytes physaddr;
	vir_bytes viraddr;
	pt_t *pt;

	assert(src_vmp->vm_stacktop == dst_vmp->vm_stacktop);
	pt = &src_vmp->vm_pt;

#if LU_DEBUG
	printf("VM: pt_ptmap: src = %d, dst = %d\n",
		src_vmp->vm_endpoint, dst_vmp->vm_endpoint);
#endif

	/* Transfer mapping to the page directory. */
	assert((vir_bytes) pt->pt_dir >= src_vmp->vm_stacktop);
	viraddr = arch_vir2map(src_vmp, (vir_bytes) pt->pt_dir);
	physaddr = pt->pt_dir_phys & I386_VM_ADDR_MASK;
	if((r=pt_writemap(&dst_vmp->vm_pt, viraddr, physaddr, I386_PAGE_SIZE,
		I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE,
		WMF_OVERWRITE)) != OK) {
		return r;
	}
#if LU_DEBUG
	printf("VM: pt_ptmap: transferred mapping to page dir: 0x%08x (0x%08x)\n",
		viraddr, physaddr);
#endif

	/* Scan all non-reserved page-directory entries. */
	for(pde=proc_pde; pde < I386_VM_DIR_ENTRIES; pde++) {
		if(!(pt->pt_dir[pde] & I386_VM_PRESENT)) {
			continue;
		}

		/* Transfer mapping to the page table. */
		assert((vir_bytes) pt->pt_pt[pde] >= src_vmp->vm_stacktop);
		viraddr = arch_vir2map(src_vmp, (vir_bytes) pt->pt_pt[pde]);
		physaddr = pt->pt_dir[pde] & I386_VM_ADDR_MASK;
		if((r=pt_writemap(&dst_vmp->vm_pt, viraddr, physaddr, I386_PAGE_SIZE,
			I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE,
			WMF_OVERWRITE)) != OK) {
			return r;
		}
	}
#if LU_DEBUG
	printf("VM: pt_ptmap: transferred mappings to page tables, pde range %d - %d\n",
		proc_pde, I386_VM_DIR_ENTRIES - 1);
#endif

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
	int p, r, pages;
	int verify = 0;

	if(writemapflags & WMF_VERIFY)
		verify = 1;

	assert(!(bytes % I386_PAGE_SIZE));
	assert(!(flags & ~(PTF_ALLFLAGS)));

	pages = bytes / I386_PAGE_SIZE;

	/* MAP_NONE means to clear the mapping. It doesn't matter
	 * what's actually written into the PTE if I386_VM_PRESENT
	 * isn't on, so we can just write MAP_NONE into it.
	 */
	assert(physaddr == MAP_NONE || (flags & I386_VM_PRESENT));
	assert(physaddr != MAP_NONE || !flags);

	/* First make sure all the necessary page tables are allocated,
	 * before we start writing in any of them, because it's a pain
	 * to undo our work properly.
	 */
	r = pt_ptalloc_in_range(pt, v, v + I386_PAGE_SIZE*pages, flags, verify);
	if(r != OK) {
		return r;
	}

	/* Now write in them. */
	for(p = 0; p < pages; p++) {
		u32_t entry;
		int pde = I386_VM_PDE(v);
		int pte = I386_VM_PTE(v);

		assert(!(v % I386_PAGE_SIZE));
		assert(pte >= 0 && pte < I386_VM_PT_ENTRIES);
		assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);

		/* Page table has to be there. */
		assert(pt->pt_dir[pde] & I386_VM_PRESENT);

		/* Make sure page directory entry for this page table
		 * is marked present and page table entry is available.
		 */
		assert((pt->pt_dir[pde] & I386_VM_PRESENT));
		assert(pt->pt_pt[pde]);

#if SANITYCHECKS
		/* We don't expect to overwrite a page. */
		if(!(writemapflags & (WMF_OVERWRITE|WMF_VERIFY)))
			assert(!(pt->pt_pt[pde][pte] & I386_VM_PRESENT));
#endif
		if(writemapflags & (WMF_WRITEFLAGSONLY|WMF_FREE)) {
			physaddr = pt->pt_pt[pde][pte] & I386_VM_ADDR_MASK;
		}

		if(writemapflags & WMF_FREE) {
			free_mem(ABS2CLICK(physaddr), 1);
		}

		/* Entry we will write. */
		entry = (physaddr & I386_VM_ADDR_MASK) | flags;

		if(verify) {
			u32_t maskedentry;
			maskedentry = pt->pt_pt[pde][pte];
			maskedentry &= ~(I386_VM_ACC|I386_VM_DIRTY);
			/* Verify pagetable entry. */
			if(entry & I386_VM_WRITE) {
				/* If we expect a writable page, allow a readonly page. */
				maskedentry |= I386_VM_WRITE;
			}
			if(maskedentry != entry) {
				printf("pt_writemap: mismatch: ");
				if((entry & I386_VM_ADDR_MASK) !=
					(maskedentry & I386_VM_ADDR_MASK)) {
					printf("pt_writemap: physaddr mismatch (0x%lx, 0x%lx); ", entry, maskedentry);
				} else printf("phys ok; ");
				printf(" flags: found %s; ",
					ptestr(pt->pt_pt[pde][pte]));
				printf(" masked %s; ",
					ptestr(maskedentry));
				printf(" expected %s\n", ptestr(entry));
				return EFAULT;
			}
		} else {
			/* Write pagetable entry. */
#if SANITYCHECKS
			assert(vm_addrok(pt->pt_pt[pde], 1));
#endif
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

	assert(!(bytes % I386_PAGE_SIZE));

	pages = bytes / I386_PAGE_SIZE;

	for(p = 0; p < pages; p++) {
		u32_t entry;
		int pde = I386_VM_PDE(v);
		int pte = I386_VM_PTE(v);

		assert(!(v % I386_PAGE_SIZE));
		assert(pte >= 0 && pte < I386_VM_PT_ENTRIES);
		assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);

		/* Page table has to be there. */
		if(!(pt->pt_dir[pde] & I386_VM_PRESENT))
			return EFAULT;

		/* Make sure page directory entry for this page table
		 * is marked present and page table entry is available.
		 */
		assert((pt->pt_dir[pde] & I386_VM_PRESENT) && pt->pt_pt[pde]);

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
                panic("pt_new: pt_mapkernel failed");

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
        vir_bytes v;
        phys_bytes lo, hi; 
        vir_bytes extra_clicks;
        u32_t moveup = 0;
	int global_bit_ok = 0;
	int free_pde;
	int p;
	struct vm_ep_data ep_data;
	vir_bytes sparepages_mem;
	phys_bytes sparepages_ph;
	vir_bytes ptr;

        /* Shorthand. */
        newpt = &vmprocess->vm_pt;

        /* Get ourselves spare pages. */
        ptr = (vir_bytes) static_sparepages;
        ptr += I386_PAGE_SIZE - (ptr % I386_PAGE_SIZE);
        if(!(sparepages_mem = ptr))
		panic("pt_init: aalloc for spare failed");
        if((r=sys_umap(SELF, VM_D, (vir_bytes) sparepages_mem,
                I386_PAGE_SIZE*SPAREPAGES, &sparepages_ph)) != OK)
                panic("pt_init: sys_umap failed: %d", r);

        missing_spares = 0;
        assert(STATIC_SPAREPAGES < SPAREPAGES);
        for(s = 0; s < SPAREPAGES; s++) {
        	if(s >= STATIC_SPAREPAGES) {
        		sparepages[s].page = NULL;
        		missing_spares++;
        		continue;
        	}
        	sparepages[s].page = (void *) (sparepages_mem + s*I386_PAGE_SIZE);
        	sparepages[s].phys = sparepages_ph + s*I386_PAGE_SIZE;
        }

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
        lo = CLICK2ABS(vmprocess->vm_arch.vm_seg[T].mem_phys);
        hi = CLICK2ABS(vmprocess->vm_arch.vm_seg[S].mem_phys +
                vmprocess->vm_arch.vm_seg[S].mem_len);
                  
        assert(!(lo % I386_PAGE_SIZE)); 
        assert(!(hi % I386_PAGE_SIZE));
 
        if(lo < VM_PROCSTART) {
                moveup = VM_PROCSTART - lo;
                assert(!(VM_PROCSTART % I386_PAGE_SIZE));
                assert(!(lo % I386_PAGE_SIZE));
                assert(!(moveup % I386_PAGE_SIZE));
        }
        
        /* Make new page table for ourselves, partly copied
         * from the current one.
         */     
        if(pt_new(newpt) != OK)
                panic("pt_init: pt_new failed"); 

        /* Set up mappings for VM process. */
        for(v = lo; v < hi; v += I386_PAGE_SIZE)  {
                phys_bytes addr;
                u32_t flags; 
        
                /* We have to write the new position in the PT,
                 * so we can move our segments.
                 */ 
                if(pt_writemap(newpt, v+moveup, v, I386_PAGE_SIZE,
                        I386_VM_PRESENT|I386_VM_WRITE|I386_VM_USER, 0) != OK)
                        panic("pt_init: pt_writemap failed");
        }
       
        /* Move segments up too. */
        vmprocess->vm_arch.vm_seg[T].mem_phys += ABS2CLICK(moveup);
        vmprocess->vm_arch.vm_seg[D].mem_phys += ABS2CLICK(moveup);
        vmprocess->vm_arch.vm_seg[S].mem_phys += ABS2CLICK(moveup);
       
	/* Allocate us a page table in which to remember page directory
	 * pointers.
	 */
	if(!(page_directories = vm_allocpage(&page_directories_phys,
		VMP_PAGETABLE)))
                panic("no virt addr for vm mappings");

	memset(page_directories, 0, I386_PAGE_SIZE);
       
        /* Increase our hardware data segment to create virtual address
         * space above our stack. We want to increase it to VM_DATATOP,
         * like regular processes have.
         */
        extra_clicks = ABS2CLICK(VM_DATATOP - hi);
        vmprocess->vm_arch.vm_seg[S].mem_len += extra_clicks;
       
        /* We pretend to the kernel we have a huge stack segment to
         * increase our data segment.
         */
        vmprocess->vm_arch.vm_data_top =
                (vmprocess->vm_arch.vm_seg[S].mem_vir +
                vmprocess->vm_arch.vm_seg[S].mem_len) << CLICK_SHIFT;
       
        /* Where our free virtual address space starts.
         * This is only a hint to the VM system.
         */
        newpt->pt_virtop = 0;

        /* Let other functions know VM now has a private page table. */
        vmprocess->vm_flags |= VMF_HASPT;

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
                		panic("VM: too many kernel mappings: %d", index);
			kern_mappings[index].phys_addr = addr;
			kern_mappings[index].len = len;
			kern_mappings[index].flags = flags;
			kern_mappings[index].lin_addr = offset;
			kern_mappings[index].flags =
				I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE |
				global_bit;
			if(flags & VMMF_UNCACHED)
				kern_mappings[index].flags |= PTF_NOCACHE;
			if(addr % I386_PAGE_SIZE)
                		panic("VM: addr unaligned: %d", addr);
			if(len % I386_PAGE_SIZE)
                		panic("VM: len unaligned: %d", len);
			vir = arch_map2vir(&vmproc[VMP_SYSTEM], offset);
			if(sys_vmctl_reply_mapping(index, vir) != OK)
                		panic("VM: reply failed");
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
			panic("VMCTL_I386_FREEPDE failed: %d", r);
		}
	}

	/* first pde in use by process. */
	proc_pde = free_pde;

        /* Give our process the new, copied, private page table. */
	pt_mapkernel(newpt);	/* didn't know about vm_dir pages earlier */
        pt_bind(newpt, vmprocess);
       
	/* new segment limit for the kernel after paging is enabled */
	ep_data.data_seg_limit = free_pde*I386_BIG_PAGE_SIZE;
	/* the memory map which must be installed after paging is enabled */
	ep_data.mem_map = vmprocess->vm_arch.vm_seg;

	/* Now actually enable paging. */
	if(sys_vmctl_enable_paging(&ep_data) != OK)
        	panic("pt_init: enable paging failed");

        /* Back to reality - this is where the stack actually is. */
        vmprocess->vm_arch.vm_seg[S].mem_len -= extra_clicks;

        /* Pretend VM stack top is the same as any regular process, not to
         * have discrepancies with new VM instances later on.
         */
        vmprocess->vm_stacktop = VM_STACKTOP;

        /* All OK. */
        return;
}

/*===========================================================================*
 *                             pt_init_mem                                   *
 *===========================================================================*/
PUBLIC void pt_init_mem()
{
/* Architecture-specific memory initialization. Make sure all the pages
 * shared with the kernel and VM's page tables are mapped above the stack,
 * so that we can easily transfer existing mappings for new VM instances.
 */
        u32_t new_page_directories_phys, *new_page_directories;
        u32_t new_pt_dir_phys, *new_pt_dir;
        u32_t new_pt_phys, *new_pt; 
        pt_t *vmpt;
        int i;

        vmpt = &vmprocess->vm_pt;

        /* We should be running this when VM has been assigned a page
         * table and memory initialization has already been performed.
         */
        assert(vmprocess->vm_flags & VMF_HASPT);
        assert(meminit_done);

        /* Throw away static spare pages. */
	vm_checkspares();
	for(i = 0; i < SPAREPAGES; i++) {
		if(sparepages[i].page && (vir_bytes) sparepages[i].page
			< vmprocess->vm_stacktop) {
			sparepages[i].page = NULL;
			missing_spares++;
		}
	}
	vm_checkspares();

        /* Rellocate page for page directories pointers. */
	if(!(new_page_directories = vm_allocpage(&new_page_directories_phys,
		VMP_PAGETABLE)))
                panic("unable to reallocated page for page dir ptrs");
	assert((vir_bytes) new_page_directories >= vmprocess->vm_stacktop);
	memcpy(new_page_directories, page_directories, I386_PAGE_SIZE);
	page_directories = new_page_directories;
	pagedir_pde_val = (new_page_directories_phys & I386_VM_ADDR_MASK) |
			(pagedir_pde_val & ~I386_VM_ADDR_MASK);

	/* Remap in kernel. */
	pt_mapkernel(vmpt);

	/* Reallocate VM's page directory. */
	if((vir_bytes) vmpt->pt_dir < vmprocess->vm_stacktop) {
		if(!(new_pt_dir= vm_allocpage(&new_pt_dir_phys, VMP_PAGEDIR))) {
			panic("unable to reallocate VM's page directory");
		}
		assert((vir_bytes) new_pt_dir >= vmprocess->vm_stacktop);
		memcpy(new_pt_dir, vmpt->pt_dir, I386_PAGE_SIZE);
		vmpt->pt_dir = new_pt_dir;
		vmpt->pt_dir_phys = new_pt_dir_phys;
		pt_bind(vmpt, vmprocess);
	}

	/* Reallocate VM's page tables. */
	for(i = proc_pde; i < I386_VM_DIR_ENTRIES; i++) {
		if(!(vmpt->pt_dir[i] & I386_VM_PRESENT)) {
			continue;
		}
		assert(vmpt->pt_pt[i]);
		if((vir_bytes) vmpt->pt_pt[i] >= vmprocess->vm_stacktop) {
			continue;
		}
		vm_checkspares();
		if(!(new_pt = vm_allocpage(&new_pt_phys, VMP_PAGETABLE)))
			panic("unable to reallocate VM's page table");
		assert((vir_bytes) new_pt >= vmprocess->vm_stacktop);
		memcpy(new_pt, vmpt->pt_pt[i], I386_PAGE_SIZE);
		vmpt->pt_pt[i] = new_pt;
		vmpt->pt_dir[i] = (new_pt_phys & I386_VM_ADDR_MASK) |
			(vmpt->pt_dir[i] & ~I386_VM_ADDR_MASK);
	}
}

/*===========================================================================*
 *				pt_bind			     		     *
 *===========================================================================*/
PUBLIC int pt_bind(pt_t *pt, struct vmproc *who)
{
	int slot, ispt;
	u32_t phys;
	void *pdes;

	/* Basic sanity checks. */
	assert(who);
	assert(who->vm_flags & VMF_INUSE);
	assert(pt);

	assert(pagedir_pde >= 0);

	slot = who->vm_slot;
	assert(slot >= 0);
	assert(slot < ELEMENTS(vmproc));
	assert(slot < I386_VM_PT_ENTRIES);

	phys = pt->pt_dir_phys & I386_VM_ADDR_MASK;
	assert(pt->pt_dir_phys == phys);

	/* Update "page directory pagetable." */
	page_directories[slot] = phys | I386_VM_PRESENT|I386_VM_WRITE;

	/* This is where the PDE's will be visible to the kernel
	 * in its address space.
	 */
	pdes = (void *) arch_map2vir(&vmproc[VMP_SYSTEM],
		pagedir_pde*I386_BIG_PAGE_SIZE + 
			slot * I386_PAGE_SIZE);

#if 0
	printf("VM: slot %d endpoint %d has pde val 0x%lx at kernel address 0x%lx\n",
		slot, who->vm_endpoint, page_directories[slot], pdes);
#endif
	/* Tell kernel about new page table root. */
	return sys_vmctl_set_addrspace(who->vm_endpoint,
		pt ? pt->pt_dir_phys : 0,
		pt ? pdes : 0);
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
        assert(vmproc[VMP_SYSTEM].vm_flags & VMF_INUSE);

	if(bigpage_ok) {
		int pde;
		for(pde = 0; pde <= id_map_high_pde; pde++) {
			phys_bytes addr;
			addr = pde * I386_BIG_PAGE_SIZE;
			assert((addr & I386_VM_ADDR_MASK) == addr);
			pt->pt_dir[pde] = addr | I386_VM_PRESENT |
				I386_VM_BIGPAGE | I386_VM_USER |
				I386_VM_WRITE | global_bit;
		}
	} else {
		panic("VM: pt_mapkernel: no bigpage");
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
			panic("pt_mapkernel: pt_writemap failed");
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

