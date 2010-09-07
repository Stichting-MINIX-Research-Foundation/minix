
#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "kernel/vm.h"

#include <machine/vm.h>

#include <minix/type.h>
#include <minix/syslib.h>
#include <minix/cpufeature.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>

#include <machine/vm.h>

#include "oxpcie.h"
#include "proto.h"
#include "kernel/proto.h"
#include "kernel/debug.h"

#ifdef CONFIG_APIC
#include "apic.h"
#ifdef CONFIG_WATCHDOG
#include "kernel/watchdog.h"
#endif
#endif

PRIVATE int psok = 0;

#define MAX_FREEPDES (3 * CONFIG_MAX_CPUS)
PRIVATE int nfreepdes = 0, freepdes[MAX_FREEPDES];

#define HASPT(procptr) ((procptr)->p_seg.p_cr3 != 0)

FORWARD _PROTOTYPE( u32_t phys_get32, (phys_bytes v)			);
FORWARD _PROTOTYPE( void vm_enable_paging, (void)			);

	
/* *** Internal VM Functions *** */

PUBLIC void vm_init(struct proc *newptproc)
{
	if(vm_running)
		panic("vm_init: vm_running");

	/* switch_address_space() checks what is in cr3, and doesn't do
	 * anything if it's the same as the cr3 of its argument, newptproc.
	 * If MINIX was previously booted, this could very well be the case.
	 *
	 * The first time switch_address_space() is called, we want to
	 * force it to do something (load cr3 and set newptproc), so we
	 * zero cr3, and force paging off to make that a safe thing to do.
	 *
	 * After that, vm_enable_paging() enables paging with the page table
	 * of newptproc loaded.
	 */

	vm_stop();
	write_cr3(0);
	switch_address_space(newptproc);
	assert(ptproc == newptproc);
	catch_pagefaults = 0;
	vm_enable_paging();
	vm_running = 1;
}

/* This function sets up a mapping from within the kernel's address
 * space to any other area of memory, either straight physical
 * memory (pr == NULL) or a process view of memory, in 4MB windows.
 * I.e., it maps in 4MB chunks of virtual (or physical) address space
 * to 4MB chunks of kernel virtual address space.
 *
 * It recognizes pr already being in memory as a special case (no
 * mapping required).
 *
 * The target (i.e. in-kernel) mapping area is one of the freepdes[]
 * VM has earlier already told the kernel about that is available. It is
 * identified as the 'pde' parameter. This value can be chosen freely
 * by the caller, as long as it is in range (i.e. 0 or higher and corresonds
 * to a known freepde slot). It is up to the caller to keep track of which
 * freepde's are in use, and to determine which ones are free to use.
 *
 * The logical number supplied by the caller is translated into an actual
 * pde number to be used, and a pointer to it (linear address) is returned
 * for actual use by phys_copy or phys_memset.
 */
PRIVATE phys_bytes createpde(
	const struct proc *pr,	/* Requested process, NULL for physical. */
	const phys_bytes linaddr,/* Address after segment translation. */
	phys_bytes *bytes,	/* Size of chunk, function may truncate it. */
	int free_pde_idx,	/* index of the free slot to use */
	int *changed		/* If mapping is made, this is set to 1. */
	)
{
	u32_t pdeval;
	phys_bytes offset;
	int pde;

	assert(free_pde_idx >= 0 && free_pde_idx < nfreepdes);
	pde = freepdes[free_pde_idx];
	assert(pde >= 0 && pde < 1024);

	if(pr && ((pr == ptproc) || !HASPT(pr))) {
		/* Process memory is requested, and
		 * it's a process that is already in current page table, or
		 * a process that is in every page table.
		 * Therefore linaddr is valid directly, with the requested
		 * size.
		 */
		return linaddr;
	}

	if(pr) {
		/* Requested address is in a process that is not currently
		 * accessible directly. Grab the PDE entry of that process'
		 * page table that corresponds to the requested address.
		 */
		assert(pr->p_seg.p_cr3_v);
		pdeval = pr->p_seg.p_cr3_v[I386_VM_PDE(linaddr)];
	} else {
		/* Requested address is physical. Make up the PDE entry. */
		pdeval = (linaddr & I386_VM_ADDR_MASK_4MB) | 
			I386_VM_BIGPAGE | I386_VM_PRESENT | 
			I386_VM_WRITE | I386_VM_USER;
	}

	/* Write the pde value that we need into a pde that the kernel
	 * can access, into the currently loaded page table so it becomes
	 * visible.
	 */
	assert(ptproc->p_seg.p_cr3_v);
	if(ptproc->p_seg.p_cr3_v[pde] != pdeval) {
		ptproc->p_seg.p_cr3_v[pde] = pdeval;
		*changed = 1;
	}

	/* Memory is now available, but only the 4MB window of virtual
	 * address space that we have mapped; calculate how much of
	 * the requested range is visible and return that in *bytes,
	 * if that is less than the requested range.
	 */
	offset = linaddr & I386_VM_OFFSET_MASK_4MB; /* Offset in 4MB window. */
	*bytes = MIN(*bytes, I386_BIG_PAGE_SIZE - offset); 

	/* Return the linear address of the start of the new mapping. */
	return I386_BIG_PAGE_SIZE*pde + offset;
}
  
/*===========================================================================*
 *				lin_lin_copy				     *
 *===========================================================================*/
PRIVATE int lin_lin_copy(const struct proc *srcproc, vir_bytes srclinaddr, 
	const struct proc *dstproc, vir_bytes dstlinaddr, vir_bytes bytes)
{
	u32_t addr;
	proc_nr_t procslot;

	assert(vm_running);
	assert(nfreepdes >= 3);

	assert(ptproc);
	assert(proc_ptr);
	assert(read_cr3() == ptproc->p_seg.p_cr3);

	procslot = ptproc->p_nr;

	assert(procslot >= 0 && procslot < I386_VM_DIR_ENTRIES);

	if(srcproc) assert(!RTS_ISSET(srcproc, RTS_SLOT_FREE));
	if(dstproc) assert(!RTS_ISSET(dstproc, RTS_SLOT_FREE));
	assert(!RTS_ISSET(ptproc, RTS_SLOT_FREE));
	assert(ptproc->p_seg.p_cr3_v);

	while(bytes > 0) {
		phys_bytes srcptr, dstptr;
		vir_bytes chunk = bytes;
		int changed = 0;

		/* Set up 4MB ranges. */
		srcptr = createpde(srcproc, srclinaddr, &chunk, 0, &changed);
		dstptr = createpde(dstproc, dstlinaddr, &chunk, 1, &changed);
		if(changed)
			reload_cr3(); 

		/* Copy pages. */
		PHYS_COPY_CATCH(srcptr, dstptr, chunk, addr);

		if(addr) {
			/* If addr is nonzero, a page fault was caught. */

			if(addr >= srcptr && addr < (srcptr + chunk)) {
				return EFAULT_SRC;
			}
			if(addr >= dstptr && addr < (dstptr + chunk)) {
				return EFAULT_DST;
			}

			panic("lin_lin_copy fault out of range");

			/* Not reached. */
			return EFAULT;
		}

		/* Update counter and addresses for next iteration, if any. */
		bytes -= chunk;
		srclinaddr += chunk;
		dstlinaddr += chunk;
	}

	if(srcproc) assert(!RTS_ISSET(srcproc, RTS_SLOT_FREE));
	if(dstproc) assert(!RTS_ISSET(dstproc, RTS_SLOT_FREE));
	assert(!RTS_ISSET(ptproc, RTS_SLOT_FREE));
	assert(ptproc->p_seg.p_cr3_v);

	return OK;
}


PRIVATE u32_t phys_get32(phys_bytes addr)
{
	const u32_t v;
	int r;

	if(!vm_running) {
		phys_copy(addr, vir2phys(&v), sizeof(v));
		return v;
	}

	if((r=lin_lin_copy(NULL, addr, 
		proc_addr(SYSTEM), vir2phys(&v), sizeof(v))) != OK) {
		panic("lin_lin_copy for phys_get32 failed: %d",  r);
	}

	return v;
}

PRIVATE char *cr0_str(u32_t e)
{
	static char str[80];
	strcpy(str, "");
#define FLAG(v) do { if(e & (v)) { strcat(str, #v " "); e &= ~v; } } while(0)
	FLAG(I386_CR0_PE);
	FLAG(I386_CR0_MP);
	FLAG(I386_CR0_EM);
	FLAG(I386_CR0_TS);
	FLAG(I386_CR0_ET);
	FLAG(I386_CR0_PG);
	FLAG(I386_CR0_WP);
	if(e) { strcat(str, " (++)"); }
	return str;
}

PRIVATE char *cr4_str(u32_t e)
{
	static char str[80];
	strcpy(str, "");
	FLAG(I386_CR4_VME);
	FLAG(I386_CR4_PVI);
	FLAG(I386_CR4_TSD);
	FLAG(I386_CR4_DE);
	FLAG(I386_CR4_PSE);
	FLAG(I386_CR4_PAE);
	FLAG(I386_CR4_MCE);
	FLAG(I386_CR4_PGE);
	if(e) { strcat(str, " (++)"); }
	return str;
}

PUBLIC void vm_stop(void)
{
	write_cr0(read_cr0() & ~I386_CR0_PG);
}

PRIVATE void vm_enable_paging(void)
{
	u32_t cr0, cr4;
	int pgeok;

	psok = _cpufeature(_CPUF_I386_PSE);
	pgeok = _cpufeature(_CPUF_I386_PGE);

	cr0= read_cr0();
	cr4= read_cr4();

	/* First clear PG and PGE flag, as PGE must be enabled after PG. */
	write_cr0(cr0 & ~I386_CR0_PG);
	write_cr4(cr4 & ~(I386_CR4_PGE | I386_CR4_PSE));

	cr0= read_cr0();
	cr4= read_cr4();

	/* Our first page table contains 4MB entries. */
	if(psok)
		cr4 |= I386_CR4_PSE;

	write_cr4(cr4);

	/* First enable paging, then enable global page flag. */
	cr0 |= I386_CR0_PG;
	write_cr0(cr0 );
	cr0 |= I386_CR0_WP;
	write_cr0(cr0);

	/* May we enable these features? */
	if(pgeok)
		cr4 |= I386_CR4_PGE;

	write_cr4(cr4);
}

PUBLIC vir_bytes alloc_remote_segment(u32_t *selector,
	segframe_t *segments, const int index, phys_bytes phys,
	vir_bytes size, int priv)
{
	phys_bytes offset = 0;
	/* Check if the segment size can be recorded in bytes, that is, check
	 * if descriptor's limit field can delimited the allowed memory region
	 * precisely. This works up to 1MB. If the size is larger, 4K pages
	 * instead of bytes are used.
	*/
	if (size < BYTE_GRAN_MAX) {
		init_dataseg(&segments->p_ldt[EXTRA_LDT_INDEX+index],
			phys, size, priv);
		*selector = ((EXTRA_LDT_INDEX+index)*0x08) | (1*0x04) | priv;
		offset = 0;
	} else {
		init_dataseg(&segments->p_ldt[EXTRA_LDT_INDEX+index],
			phys & ~0xFFFF, 0, priv);
		*selector = ((EXTRA_LDT_INDEX+index)*0x08) | (1*0x04) | priv;
		offset = phys & 0xFFFF;
	}

	return offset;
}

PUBLIC phys_bytes umap_remote(const struct proc* rp, const int seg,
	const vir_bytes vir_addr, const vir_bytes bytes)
{
/* Calculate the physical memory address for a given virtual address. */
  struct far_mem *fm;

#if 0
  if(rp->p_misc_flags & MF_FULLVM) return 0;
#endif

  if (bytes <= 0) return( (phys_bytes) 0);
  if (seg < 0 || seg >= NR_REMOTE_SEGS) return( (phys_bytes) 0);

  fm = &rp->p_priv->s_farmem[seg];
  if (! fm->in_use) return( (phys_bytes) 0);
  if (vir_addr + bytes > fm->mem_len) return( (phys_bytes) 0);

  return(fm->mem_phys + (phys_bytes) vir_addr);
}

/*===========================================================================*
 *                              umap_local                                   *
 *===========================================================================*/
PUBLIC phys_bytes umap_local(rp, seg, vir_addr, bytes)
register struct proc *rp;       /* pointer to proc table entry for process */
int seg;                        /* T, D, or S segment */
vir_bytes vir_addr;             /* virtual address in bytes within the seg */
vir_bytes bytes;                /* # of bytes to be copied */
{
/* Calculate the physical memory address for a given virtual address. */
  vir_clicks vc;                /* the virtual address in clicks */
  phys_bytes pa;                /* intermediate variables as phys_bytes */
  phys_bytes seg_base;

  if(seg != T && seg != D && seg != S)
	panic("umap_local: wrong seg: %d",  seg);

  if (bytes <= 0) return( (phys_bytes) 0);
  if (vir_addr + bytes <= vir_addr) return 0;   /* overflow */
  vc = (vir_addr + bytes - 1) >> CLICK_SHIFT;   /* last click of data */
 
  if (seg != T)
        seg = (vc < rp->p_memmap[D].mem_vir + rp->p_memmap[D].mem_len ? D : S);
  else if (rp->p_memmap[T].mem_len == 0)	/* common I&D? */
        seg = D;				/* ptrace needs this */
 
  if ((vir_addr>>CLICK_SHIFT) >= rp->p_memmap[seg].mem_vir +
        rp->p_memmap[seg].mem_len) return( (phys_bytes) 0 );
 
  if (vc >= rp->p_memmap[seg].mem_vir +
        rp->p_memmap[seg].mem_len) return( (phys_bytes) 0 );
  
  seg_base = (phys_bytes) rp->p_memmap[seg].mem_phys;
  seg_base = seg_base << CLICK_SHIFT;   /* segment origin in bytes */
  pa = (phys_bytes) vir_addr;
  pa -= rp->p_memmap[seg].mem_vir << CLICK_SHIFT;
  return(seg_base + pa);
}

/*===========================================================================*
 *                              umap_virtual                                 *
 *===========================================================================*/
PUBLIC phys_bytes umap_virtual(rp, seg, vir_addr, bytes)
register struct proc *rp;       /* pointer to proc table entry for process */
int seg;                        /* T, D, or S segment */
vir_bytes vir_addr;             /* virtual address in bytes within the seg */
vir_bytes bytes;                /* # of bytes to be copied */
{
	vir_bytes linear;
	u32_t phys = 0;

	if(seg == MEM_GRANT) {
		return umap_grant(rp, (cp_grant_id_t) vir_addr, bytes);
	}
	
	if(!(linear = umap_local(rp, seg, vir_addr, bytes))) {
			printf("SYSTEM:umap_virtual: umap_local failed\n");
			phys = 0;
		} else {
			if(vm_lookup(rp, linear, &phys, NULL) != OK) {
				printf("SYSTEM:umap_virtual: vm_lookup of %s: seg 0x%lx: 0x%lx failed\n", rp->p_name, seg, vir_addr);
				phys = 0;
			}
			if(phys == 0)
				panic("vm_lookup returned phys: %d",  phys);
		}
	

	if(phys == 0) {
		printf("SYSTEM:umap_virtual: lookup failed\n");
		return 0;
	}

	/* Now make sure addresses are contiguous in physical memory
	 * so that the umap makes sense.
	 */
	if(bytes > 0 && !vm_contiguous(rp, linear, bytes)) {
		printf("umap_virtual: %s: %d at 0x%lx (vir 0x%lx) not contiguous\n",
			rp->p_name, bytes, linear, vir_addr);
		return 0;
	}

	/* phys must be larger than 0 (or the caller will think the call
	 * failed), and address must not cross a page boundary.
	 */
	assert(phys);

	return phys;
}


/*===========================================================================*
 *                              vm_lookup                                    *
 *===========================================================================*/
PUBLIC int vm_lookup(const struct proc *proc, const vir_bytes virtual,
 vir_bytes *physical, u32_t *ptent)
{
	u32_t *root, *pt;
	int pde, pte;
	u32_t pde_v, pte_v;

	assert(proc);
	assert(physical);
	assert(!isemptyp(proc));

	if(!HASPT(proc)) {
		*physical = virtual;
		return OK;
	}

	/* Retrieve page directory entry. */
	root = (u32_t *) proc->p_seg.p_cr3;
	assert(!((u32_t) root % I386_PAGE_SIZE));
	pde = I386_VM_PDE(virtual);
	assert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);
	pde_v = phys_get32((u32_t) (root + pde));

	if(!(pde_v & I386_VM_PRESENT)) {
		return EFAULT;
	}

	/* We don't expect to ever see this. */
	if(pde_v & I386_VM_BIGPAGE) {
		*physical = pde_v & I386_VM_ADDR_MASK_4MB;
		if(ptent) *ptent = pde_v;
		*physical += virtual & I386_VM_OFFSET_MASK_4MB;
	} else {
		/* Retrieve page table entry. */
		pt = (u32_t *) I386_VM_PFA(pde_v);
		assert(!((u32_t) pt % I386_PAGE_SIZE));
		pte = I386_VM_PTE(virtual);
		assert(pte >= 0 && pte < I386_VM_PT_ENTRIES);
		pte_v = phys_get32((u32_t) (pt + pte));
		if(!(pte_v & I386_VM_PRESENT)) {
			return EFAULT;
		}

		if(ptent) *ptent = pte_v;

		/* Actual address now known; retrieve it and add page offset. */
		*physical = I386_VM_PFA(pte_v);
		*physical += virtual % I386_PAGE_SIZE;
	}

	return OK;
}

/*===========================================================================*
 *                              vm_contiguous                                *
 *===========================================================================*/
PUBLIC int vm_contiguous(const struct proc *targetproc, vir_bytes vir_buf, size_t bytes)
{
	int first = 1, r;
	u32_t prev_phys = 0;    /* Keep lints happy. */
	u32_t po;

	assert(targetproc);
	assert(bytes > 0);

	if(!HASPT(targetproc))
		return 1;

	/* Start and end at page boundary to make logic simpler. */
	po = vir_buf % I386_PAGE_SIZE;
	if(po > 0) {
		bytes += po;
		vir_buf -= po;
	}
	po = (vir_buf + bytes) % I386_PAGE_SIZE;
	if(po > 0)
		bytes += I386_PAGE_SIZE - po;

	/* Keep going as long as we cross a page boundary. */
	while(bytes > 0) {
		u32_t phys;

		if((r=vm_lookup(targetproc, vir_buf, &phys, NULL)) != OK) {
			printf("vm_contiguous: vm_lookup failed, %d\n", r);
			printf("kernel stack: ");
			util_stacktrace();
			return 0;
		}

		if(!first) {
			if(prev_phys+I386_PAGE_SIZE != phys) {
				printf("vm_contiguous: no (0x%lx, 0x%lx)\n",
					prev_phys, phys);
				printf("kernel stack: ");
				util_stacktrace();
				return 0;
			}
		}

		first = 0;

		prev_phys = phys;
		vir_buf += I386_PAGE_SIZE;
		bytes -= I386_PAGE_SIZE;
	}

	return 1;
}

/*===========================================================================*
 *                              vm_suspend                                *
 *===========================================================================*/
PRIVATE void vm_suspend(struct proc *caller, const struct proc *target,
	const vir_bytes linaddr, const vir_bytes len, const int type)
{
	/* This range is not OK for this process. Set parameters  
	 * of the request and notify VM about the pending request. 
	 */								
	assert(!RTS_ISSET(caller, RTS_VMREQUEST));
	assert(!RTS_ISSET(target, RTS_VMREQUEST));

	RTS_SET(caller, RTS_VMREQUEST);

	caller->p_vmrequest.req_type = VMPTYPE_CHECK;
	caller->p_vmrequest.target = target->p_endpoint;
	caller->p_vmrequest.params.check.start = linaddr;
	caller->p_vmrequest.params.check.length = len;
	caller->p_vmrequest.params.check.writeflag = 1;
	caller->p_vmrequest.type = type;
							
	/* Connect caller on vmrequest wait queue. */	
	if(!(caller->p_vmrequest.nextrequestor = vmrequest))
		send_sig(VM_PROC_NR, SIGKMEM);
	vmrequest = caller;
}

/*===========================================================================*
 *                              delivermsg                                *
 *===========================================================================*/
PUBLIC void delivermsg(struct proc *rp)
{
	phys_bytes addr;  
	int r;

	assert(rp->p_misc_flags & MF_DELIVERMSG);
	assert(rp->p_delivermsg.m_source != NONE);

	if (copy_msg_to_user(rp, &rp->p_delivermsg,
				(message *) rp->p_delivermsg_vir)) {
		printf("WARNING wrong user pointer 0x%08x from "
				"process %s / %d\n",
				rp->p_delivermsg_vir,
				rp->p_name,
				rp->p_endpoint);
		r = EFAULT;
	} else {
		/* Indicate message has been delivered; address is 'used'. */
		rp->p_delivermsg.m_source = NONE;
		rp->p_misc_flags &= ~MF_DELIVERMSG;
		r = OK;
	}

	if(!(rp->p_misc_flags & MF_CONTEXT_SET)) {
		rp->p_reg.retreg = r;
	}
}

PRIVATE char *flagstr(u32_t e, const int dir)
{
	static char str[80];
	strcpy(str, "");
	FLAG(I386_VM_PRESENT);
	FLAG(I386_VM_WRITE);
	FLAG(I386_VM_USER);
	FLAG(I386_VM_PWT);
	FLAG(I386_VM_PCD);
	FLAG(I386_VM_GLOBAL);
	if(dir)
		FLAG(I386_VM_BIGPAGE);	/* Page directory entry only */
	else
		FLAG(I386_VM_DIRTY);	/* Page table entry only */
	return str;
}

PRIVATE void vm_pt_print(u32_t *pagetable, const u32_t v)
{
	int pte;
	int col = 0;

	assert(!((u32_t) pagetable % I386_PAGE_SIZE));

	for(pte = 0; pte < I386_VM_PT_ENTRIES; pte++) {
		u32_t pte_v, pfa;
		pte_v = phys_get32((u32_t) (pagetable + pte));
		if(!(pte_v & I386_VM_PRESENT))
			continue;
		pfa = I386_VM_PFA(pte_v);
		printf("%4d:%08lx:%08lx %2s ",
			pte, v + I386_PAGE_SIZE*pte, pfa,
			(pte_v & I386_VM_WRITE) ? "rw":"RO");
		col++;
		if(col == 3) { printf("\n"); col = 0; }
	}
	if(col > 0) printf("\n");

	return;
}

PRIVATE void vm_print(u32_t *root)
{
	int pde;

	assert(!((u32_t) root % I386_PAGE_SIZE));

	printf("page table 0x%lx:\n", root);

	for(pde = 0; pde < I386_VM_DIR_ENTRIES; pde++) {
		u32_t pde_v;
		u32_t *pte_a;
		pde_v = phys_get32((u32_t) (root + pde));
		if(!(pde_v & I386_VM_PRESENT))
			continue;
		if(pde_v & I386_VM_BIGPAGE) {
			printf("%4d: 0x%lx, flags %s\n",
				pde, I386_VM_PFA(pde_v), flagstr(pde_v, 1));
		} else {
			pte_a = (u32_t *) I386_VM_PFA(pde_v);
			printf("%4d: pt %08lx %s\n",
				pde, pte_a, flagstr(pde_v, 1));
			vm_pt_print(pte_a, pde * I386_VM_PT_ENTRIES * I386_PAGE_SIZE);
			printf("\n");
		}
	}


	return;
}

/*===========================================================================*
 *				lin_memset				     *
 *===========================================================================*/
int vm_phys_memset(phys_bytes ph, const u8_t c, phys_bytes bytes)
{
	u32_t p;

	p = c | (c << 8) | (c << 16) | (c << 24);

	if(!vm_running) {
		phys_memset(ph, p, bytes);
		return OK;
	}

	assert(nfreepdes >= 3);

	assert(ptproc->p_seg.p_cr3_v);

	/* With VM, we have to map in the physical memory. 
	 * We can do this 4MB at a time.
	 */
	while(bytes > 0) {
		int changed = 0;
		phys_bytes chunk = bytes, ptr;
		ptr = createpde(NULL, ph, &chunk, 0, &changed);
		if(changed)
			reload_cr3(); 

		/* We can memset as many bytes as we have remaining,
		 * or as many as remain in the 4MB chunk we mapped in.
		 */
		phys_memset(ptr, p, chunk);
		bytes -= chunk;
		ph += chunk;
	}

	assert(ptproc->p_seg.p_cr3_v);

	return OK;
}

/*===========================================================================*
 *				virtual_copy_f				     *
 *===========================================================================*/
PUBLIC int virtual_copy_f(caller, src_addr, dst_addr, bytes, vmcheck)
struct proc * caller;
struct vir_addr *src_addr;	/* source virtual address */
struct vir_addr *dst_addr;	/* destination virtual address */
vir_bytes bytes;		/* # of bytes to copy  */
int vmcheck;			/* if nonzero, can return VMSUSPEND */
{
/* Copy bytes from virtual address src_addr to virtual address dst_addr. 
 * Virtual addresses can be in ABS, LOCAL_SEG, REMOTE_SEG, or BIOS_SEG.
 */
  struct vir_addr *vir_addr[2];	/* virtual source and destination address */
  phys_bytes phys_addr[2];	/* absolute source and destination */ 
  int seg_index;
  int i;
  struct proc *procs[2];

  assert((vmcheck && caller) || (!vmcheck && !caller));

  /* Check copy count. */
  if (bytes <= 0) return(EDOM);

  /* Do some more checks and map virtual addresses to physical addresses. */
  vir_addr[_SRC_] = src_addr;
  vir_addr[_DST_] = dst_addr;

  for (i=_SRC_; i<=_DST_; i++) {
	int proc_nr, type;
	struct proc *p;

 	type = vir_addr[i]->segment & SEGMENT_TYPE;
	if((type != PHYS_SEG && type != BIOS_SEG) &&
	   isokendpt(vir_addr[i]->proc_nr_e, &proc_nr))
		p = proc_addr(proc_nr);
	else 
		p = NULL;

	procs[i] = p;

      /* Get physical address. */
      switch(type) {
      case LOCAL_SEG:
      case LOCAL_VM_SEG:
	  if(!p) {
		return EDEADSRCDST;
	  }
          seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
	  if(type == LOCAL_SEG)
	          phys_addr[i] = umap_local(p, seg_index, vir_addr[i]->offset,
			bytes);
	  else
	  	phys_addr[i] = umap_virtual(p, seg_index,
				vir_addr[i]->offset, bytes);
	  if(phys_addr[i] == 0) {
		printf("virtual_copy: map 0x%x failed for %s seg %d, "
			"offset %lx, len %d, i %d\n",
			type, p->p_name, seg_index, vir_addr[i]->offset,
			bytes, i);
	  }
          break;
      case REMOTE_SEG:
	  if(!p) {
		return EDEADSRCDST;
	  }
          seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
          phys_addr[i] = umap_remote(p, seg_index, vir_addr[i]->offset, bytes);
          break;
#if _MINIX_CHIP == _CHIP_INTEL
      case BIOS_SEG:
          phys_addr[i] = umap_bios(vir_addr[i]->offset, bytes );
          break;
#endif
      case PHYS_SEG:
          phys_addr[i] = vir_addr[i]->offset;
          break;
      default:
	  printf("virtual_copy: strange type 0x%x\n", type);
	  return EINVAL;
      }

      /* Check if mapping succeeded. */
      if (phys_addr[i] <= 0 && vir_addr[i]->segment != PHYS_SEG)  {
      printf("virtual_copy EFAULT\n");
	  return EFAULT;
      }
  }

  if(vm_running) {
	int r;

	if(caller && (caller->p_misc_flags & MF_KCALL_RESUME)) {
		assert(caller->p_vmrequest.vmresult != VMSUSPEND);
		if(caller->p_vmrequest.vmresult != OK) {
	  		return caller->p_vmrequest.vmresult;
		}
	}

	if((r=lin_lin_copy(procs[_SRC_], phys_addr[_SRC_],
		procs[_DST_], phys_addr[_DST_], bytes)) != OK) {
		struct proc *target = NULL;
		phys_bytes lin;
		if(r != EFAULT_SRC && r != EFAULT_DST)
			panic("lin_lin_copy failed: %d",  r);
		if(!vmcheck || !caller) {
	  		return r;
		}

		if(r == EFAULT_SRC) {
			lin = phys_addr[_SRC_];
			target = procs[_SRC_];
		} else if(r == EFAULT_DST) {
			lin = phys_addr[_DST_];
			target = procs[_DST_];
		} else {
			panic("r strange: %d",  r);
		}

		assert(caller);
		assert(target);

		vm_suspend(caller, target, lin, bytes, VMSTYPE_KERNELCALL);
		return VMSUSPEND;
	}

  	return OK;
  }

  assert(!vm_running);

  /* can't copy to/from process with PT without VM */
#define NOPT(p) (!(p) || !HASPT(p))
  if(!NOPT(procs[_SRC_])) {
	printf("ignoring page table src: %s / %d at 0x%lx\n",
		procs[_SRC_]->p_name, procs[_SRC_]->p_endpoint, procs[_SRC_]->p_seg.p_cr3);
}
  if(!NOPT(procs[_DST_])) {
	printf("ignoring page table dst: %s / %d at 0x%lx\n",
		procs[_DST_]->p_name, procs[_DST_]->p_endpoint,
		procs[_DST_]->p_seg.p_cr3);
  }

  /* Now copy bytes between physical addresseses. */
  if(phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes))
  	return EFAULT;
 
  return OK;
}

/*===========================================================================*
 *				data_copy				     *
 *===========================================================================*/
PUBLIC int data_copy(const endpoint_t from_proc, const vir_bytes from_addr,
	const endpoint_t to_proc, const vir_bytes to_addr,
	size_t bytes)
{
  struct vir_addr src, dst;

  src.segment = dst.segment = D;
  src.offset = from_addr;
  dst.offset = to_addr;
  src.proc_nr_e = from_proc;
  dst.proc_nr_e = to_proc;

  return virtual_copy(&src, &dst, bytes);
}

/*===========================================================================*
 *				data_copy_vmcheck			     *
 *===========================================================================*/
PUBLIC int data_copy_vmcheck(struct proc * caller,
	const endpoint_t from_proc, const vir_bytes from_addr,
	const endpoint_t to_proc, const vir_bytes to_addr,
	size_t bytes)
{
  struct vir_addr src, dst;

  src.segment = dst.segment = D;
  src.offset = from_addr;
  dst.offset = to_addr;
  src.proc_nr_e = from_proc;
  dst.proc_nr_e = to_proc;

  return virtual_copy_vmcheck(caller, &src, &dst, bytes);
}

/*===========================================================================*
 *				arch_pre_exec				     *
 *===========================================================================*/
PUBLIC void arch_pre_exec(struct proc *pr, const u32_t ip, const u32_t sp)
{
/* wipe extra LDT entries, set program counter, and stack pointer. */
	memset(pr->p_seg.p_ldt + EXTRA_LDT_INDEX, 0,
		sizeof(pr->p_seg.p_ldt[0]) * (LDT_SIZE - EXTRA_LDT_INDEX));
	pr->p_reg.pc = ip;
	pr->p_reg.sp = sp;
}

/*===========================================================================*
 *				arch_umap				     *
 *===========================================================================*/
PUBLIC int arch_umap(const struct proc *pr, vir_bytes offset, vir_bytes count,
	int seg, phys_bytes *addr)
{
	switch(seg) {
		case BIOS_SEG:
			*addr = umap_bios(offset, count);
			return OK;
	}

	/* This must be EINVAL; the umap fallback function in
	 * lib/syslib/alloc_util.c depends on it to detect an
	 * older kernel (as opposed to mapping error).
	 */
	return EINVAL;
}

/* VM reports page directory slot we're allowed to use freely. */
void i386_freepde(const int pde)
{
	if(nfreepdes >= MAX_FREEPDES)
		return;
	freepdes[nfreepdes++] = pde;
}

PRIVATE int oxpcie_mapping_index = -1;

PUBLIC int arch_phys_map(const int index, phys_bytes *addr,
  phys_bytes *len, int *flags)
{
	static int first = 1;
	int freeidx = 0;
	static char *ser_var = NULL;

	if(first) {
#ifdef CONFIG_APIC
		if(lapic_addr)
			freeidx++;
		if (ioapic_enabled)
			freeidx += nioapics;
#endif

#ifdef CONFIG_OXPCIE
		if((ser_var = env_get("oxpcie"))) {
			if(ser_var[0] != '0' || ser_var[1] != 'x') {
				printf("oxpcie address in hex please\n");
			} else {
				oxpcie_mapping_index = freeidx++;
			}
		}
#endif
		first = 0;
	}

#ifdef CONFIG_APIC
	/* map the local APIC if enabled */
	if (index == 0) {
		if (!lapic_addr)
			return EINVAL;
		*addr = vir2phys(lapic_addr);
		*len = 4 << 10 /* 4kB */;
		*flags = VMMF_UNCACHED;
		return OK;
	}
	else if (ioapic_enabled && index <= nioapics) {
		*addr = io_apic[index - 1].paddr;
		*len = 4 << 10 /* 4kB */;
		*flags = VMMF_UNCACHED;
		return OK;
	}
#endif

#if CONFIG_OXPCIE
	if(index == oxpcie_mapping_index) {
		*addr = strtoul(ser_var+2, NULL, 16);
		*len = 0x4000;
		*flags = VMMF_UNCACHED;
		return OK;
	}
#endif

	return EINVAL;
}

PUBLIC int arch_phys_map_reply(const int index, const vir_bytes addr)
{
#ifdef CONFIG_APIC
	/* if local APIC is enabled */
	if (index == 0 && lapic_addr) {
		lapic_addr_vaddr = addr;
		return OK;
	}
	else if (ioapic_enabled && index <= nioapics) {
		io_apic[index - 1].vaddr = addr;
		return OK;
	}
#endif

#if CONFIG_OXPCIE
	if (index == oxpcie_mapping_index) {
		oxpcie_set_vaddr((unsigned char *) addr);
		return OK;
	}
#endif

	return EINVAL;
}

PUBLIC int arch_enable_paging(struct proc * caller, const message * m_ptr)
{
	struct vm_ep_data ep_data;
	int r;

	/*
	 * copy the extra data associated with the call from userspace
	 */
	if((r=data_copy(caller->p_endpoint, (vir_bytes)m_ptr->SVMCTL_VALUE,
		KERNEL, (vir_bytes) &ep_data, sizeof(ep_data))) != OK) {
		printf("vmctl_enable_paging: data_copy failed! (%d)\n", r);
		return r;
	}

	/*
	 * when turning paging on i386 we also change the segment limits to make
	 * the special mappings requested by the kernel reachable
	 */
	if ((r = prot_set_kern_seg_limit(ep_data.data_seg_limit)) != OK)
		return r;

	/*
	 * install the new map provided by the call
	 */
	if (newmap(caller, caller, ep_data.mem_map) != OK)
		panic("arch_enable_paging: newmap failed");

#ifdef CONFIG_APIC
	/* start using the virtual addresses */

	/* if local APIC is enabled */
	if (lapic_addr) {
		lapic_addr = lapic_addr_vaddr;
		lapic_eoi_addr = LAPIC_EOI;
	}
	/* if IO apics are enabled */
	if (ioapic_enabled) {
		int i;

		for (i = 0; i < nioapics; i++) {
			io_apic[i].addr = io_apic[i].vaddr;
		}
	}
#endif
#ifdef CONFIG_WATCHDOG
	/*
	 * We make sure that we don't enable the watchdog until paging is turned
	 * on as we might get a NMI while switching and we might still use wrong
	 * lapic address. Bad things would happen. It is unfortunate but such is
	 * life
	 */
	i386_watchdog_start();
#endif

	return OK;
}

PUBLIC void release_address_space(struct proc *pr)
{
	pr->p_seg.p_cr3_v = NULL;
}

/* computes a checksum of a buffer of a given length. The byte sum must be zero */
PUBLIC int platform_tbl_checksum_ok(void *ptr, unsigned int length)
{
	u8_t total = 0;
	unsigned int i;
	for (i = 0; i < length; i++)
		total += ((unsigned char *)ptr)[i];
	return !total;
}

PUBLIC int platform_tbl_ptr(phys_bytes start,
					phys_bytes end,
					unsigned increment,
					void * buff,
					unsigned size,
					phys_bytes * phys_addr,
					int ((* cmp_f)(void *)))
{
	phys_bytes addr;

	for (addr = start; addr < end; addr += increment) {
		phys_copy (addr, vir2phys(buff), size);
		if (cmp_f(buff)) {
			if (phys_addr)
				*phys_addr = addr;
			return 1;
		}
	}
	return 0;
}
