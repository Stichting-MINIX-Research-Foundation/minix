
#include "../../kernel.h"
#include "../../proc.h"
#include "../../vm.h"

#include <minix/type.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/cpufeature.h>
#include <string.h>

#define FREEPDE_SRC	0
#define FREEPDE_DST	1
#define FREEPDE_MEMSET	2

#include <sys/vm_i386.h>

#include <minix/portio.h>

#include "proto.h"
#include "../../proto.h"
#include "../../debug.h"

PRIVATE int psok = 0;

extern u32_t createpde, linlincopies, physzero;

#define PROCPDEPTR(pr, pi) ((u32_t *) ((u8_t *) vm_pagedirs +\
				I386_PAGE_SIZE * pr->p_nr +	\
				I386_VM_PT_ENT_SIZE * pi))

/* Signal to exception handler that pagefaults can happen. */
int catch_pagefaults = 0;

u8_t *vm_pagedirs = NULL;

u32_t i386_invlpg_addr = 0;

#define WANT_FREEPDES 4
PRIVATE int nfreepdes = 0, freepdes[WANT_FREEPDES];

#define HASPT(procptr) ((procptr)->p_seg.p_cr3 != 0)

FORWARD _PROTOTYPE( u32_t phys_get32, (vir_bytes v)			);
FORWARD _PROTOTYPE( void vm_set_cr3, (u32_t value)			);
FORWARD _PROTOTYPE( void set_cr3, (void)				);
FORWARD _PROTOTYPE( void vm_enable_paging, (void)			);

/* *** Internal VM Functions *** */

PUBLIC void vm_init(struct proc *newptproc)
{
	u32_t newcr3;

	if(vm_running)
		minix_panic("vm_init: vm_running", NO_NUM);

	ptproc = newptproc;
	newcr3 = ptproc->p_seg.p_cr3;
	kprintf("vm_init: ptproc: %s / %d, cr3 0x%lx\n",
		ptproc->p_name, ptproc->p_endpoint,
		ptproc->p_seg.p_cr3);
	vmassert(newcr3);

	/* Set this cr3 now (not active until paging enabled). */
	vm_set_cr3(newcr3);

	kprintf("vm_init: writing cr3 0x%lx done; cr3: 0x%lx\n",
		newcr3, read_cr3());

	kprintf("vm_init: enabling\n");
	/* Actually enable paging (activating cr3 load above). */
	level0(vm_enable_paging);

	kprintf("vm_init: enabled\n");

	/* Don't do this init in the future. */
	vm_running = 1;

	kprintf("vm_init done\n");
}

PRIVATE u32_t phys_get32(addr)
phys_bytes addr;
{
	u32_t v;
	int r;

	if(!vm_running) {
		phys_copy(addr, vir2phys(&v), sizeof(v));
		return v;
	}

	if((r=lin_lin_copy(NULL, addr, NULL, D,
		proc_addr(SYSTEM), &v, &v, D, 
		sizeof(v))) != OK) {
		minix_panic("lin_lin_copy for phys_get32 failed", r);
	}

	return v;
}

PRIVATE u32_t vm_cr3;	/* temp arg to level0() func */

PRIVATE void vm_set_cr3(value)
u32_t value;
{
	vm_cr3= value;
	level0(set_cr3);
}

PRIVATE void set_cr3()
{
	write_cr3(vm_cr3);
}

char *cr0_str(u32_t e)
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

char *cr4_str(u32_t e)
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
	segframe_t *segments, int index, phys_bytes phys, vir_bytes size,
	int priv)
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

PUBLIC phys_bytes umap_remote(struct proc* rp, int seg,
	vir_bytes vir_addr, vir_bytes bytes)
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
	minix_panic("umap_local: wrong seg", seg);

  if (bytes <= 0) return( (phys_bytes) 0);
  if (vir_addr + bytes <= vir_addr) return 0;   /* overflow */
  vc = (vir_addr + bytes - 1) >> CLICK_SHIFT;   /* last click of data */
 
  if (seg != T)
        seg = (vc < rp->p_memmap[D].mem_vir + rp->p_memmap[D].mem_len ? D : S);
 
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
		phys = umap_grant(rp, vir_addr, bytes);
	} else {
		if(!(linear = umap_local(rp, seg, vir_addr, bytes))) {
			kprintf("SYSTEM:umap_virtual: umap_local failed\n");
			phys = 0;
		} else {
			if(vm_lookup(rp, linear, &phys, NULL) != OK) {
				kprintf("SYSTEM:umap_virtual: vm_lookup of %s: seg 0x%lx: 0x%lx failed\n", rp->p_name, seg, vir_addr);
				phys = 0;
			}
			if(phys == 0)
				minix_panic("vm_lookup returned phys", phys);
		}
	}

	if(phys == 0) {
		kprintf("SYSTEM:umap_virtual: lookup failed\n");
		return 0;
	}

	/* Now make sure addresses are contiguous in physical memory
	 * so that the umap makes sense.
	 */
	if(bytes > 0 && !vm_contiguous(rp, linear, bytes)) {
		kprintf("umap_virtual: %s: %d at 0x%lx (vir 0x%lx) not contiguous\n",
			rp->p_name, bytes, linear, vir_addr);
		return 0;
	}

	/* phys must be larger than 0 (or the caller will think the call
	 * failed), and address must not cross a page boundary.
	 */
	vmassert(phys);

	return phys;
}


/*===========================================================================*
 *                              vm_lookup                                    *
 *===========================================================================*/
PUBLIC int vm_lookup(struct proc *proc, vir_bytes virtual, vir_bytes *physical, u32_t *ptent)
{
	u32_t *root, *pt;
	int pde, pte;
	u32_t pde_v, pte_v;
	NOREC_ENTER(vmlookup);

	vmassert(proc);
	vmassert(physical);
	vmassert(!(proc->p_rts_flags & SLOT_FREE));

	if(!HASPT(proc)) {
		*physical = virtual;
		NOREC_RETURN(vmlookup, OK);
	}

	/* Retrieve page directory entry. */
	root = (u32_t *) proc->p_seg.p_cr3;
	vmassert(!((u32_t) root % I386_PAGE_SIZE));
	pde = I386_VM_PDE(virtual);
	vmassert(pde >= 0 && pde < I386_VM_DIR_ENTRIES);
	pde_v = phys_get32((u32_t) (root + pde));

	if(!(pde_v & I386_VM_PRESENT)) {
#if 0
		kprintf("vm_lookup: %d:%s:0x%lx: cr3 0x%lx: pde %d not present\n",
			proc->p_endpoint, proc->p_name, virtual, root, pde);
		kprintf("kernel stack: ");
		util_stacktrace();
#endif
		NOREC_RETURN(vmlookup, EFAULT);
	}

	/* We don't expect to ever see this. */
	if(pde_v & I386_VM_BIGPAGE) {
		*physical = pde_v & I386_VM_ADDR_MASK_4MB;
		if(ptent) *ptent = pde_v;
		*physical += virtual & I386_VM_OFFSET_MASK_4MB;
	} else {
		/* Retrieve page table entry. */
		pt = (u32_t *) I386_VM_PFA(pde_v);
		vmassert(!((u32_t) pt % I386_PAGE_SIZE));
		pte = I386_VM_PTE(virtual);
		vmassert(pte >= 0 && pte < I386_VM_PT_ENTRIES);
		pte_v = phys_get32((u32_t) (pt + pte));
		if(!(pte_v & I386_VM_PRESENT)) {
			NOREC_RETURN(vmlookup, EFAULT);
		}

		if(ptent) *ptent = pte_v;

		/* Actual address now known; retrieve it and add page offset. */
		*physical = I386_VM_PFA(pte_v);
		*physical += virtual % I386_PAGE_SIZE;
	}

	NOREC_RETURN(vmlookup, OK);
}

/* From virtual address v in process p,
 * lookup physical address and assign it to d.
 * If p is NULL, assume it's already a physical address.
 */
#define LOOKUP(d, p, v, flagsp) {	\
	int r; 				\
	if(!(p)) { (d) = (v); } 	\
	else {				\
		if((r=vm_lookup((p), (v), &(d), flagsp)) != OK) { \
			kprintf("vm_copy: lookup failed of 0x%lx in %d (%s)\n"\
				"kernel stacktrace: ", (v), (p)->p_endpoint, \
					(p)->p_name);		\
			util_stacktrace();			\
			return r;				\
		} } }

/*===========================================================================*
 *                              vm_contiguous                                *
 *===========================================================================*/
PUBLIC int vm_contiguous(struct proc *targetproc, u32_t vir_buf, size_t bytes)
{
	int first = 1, r, boundaries = 0;
	u32_t prev_phys, po;
	u32_t prev_vir;

	vmassert(targetproc);
	vmassert(bytes > 0);

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
			kprintf("vm_contiguous: vm_lookup failed, %d\n", r);
			kprintf("kernel stack: ");
			util_stacktrace();
			return 0;
		}

		if(!first) {
			if(prev_phys+I386_PAGE_SIZE != phys) {
				kprintf("vm_contiguous: no (0x%lx, 0x%lx)\n",
					prev_phys, phys);
				kprintf("kernel stack: ");
				util_stacktrace();
				return 0;
			}
		}

		first = 0;

		prev_phys = phys;
		prev_vir = vir_buf;
		vir_buf += I386_PAGE_SIZE;
		bytes -= I386_PAGE_SIZE;
		boundaries++;
	}

	if(verbose_vm)
		kprintf("vm_contiguous: yes (%d boundaries tested)\n",
			boundaries);

	return 1;
}

int vm_checkrange_verbose = 0;

/*===========================================================================*
 *                              vm_suspend                                *
 *===========================================================================*/
PUBLIC int vm_suspend(struct proc *caller, struct proc *target)
{
	/* This range is not OK for this process. Set parameters  
	 * of the request and notify VM about the pending request. 
	 */								
	if(RTS_ISSET(caller, VMREQUEST))			
		minix_panic("VMREQUEST already set", caller->p_endpoint); 
	RTS_LOCK_SET(caller, VMREQUEST);		
			
	/* Set caller in target. */			
	target->p_vmrequest.requestor = caller;		
							
	/* Connect caller on vmrequest wait queue. */	
	caller->p_vmrequest.nextrequestor = vmrequest;
	vmrequest = caller;	
	if(!caller->p_vmrequest.nextrequestor)
		lock_notify(SYSTEM, VM_PROC_NR);		
}

/*===========================================================================*
 *                              vm_checkrange                                *
 *===========================================================================*/
PUBLIC int vm_checkrange(struct proc *caller, struct proc *target,
	vir_bytes vir, vir_bytes bytes, int wrfl, int checkonly)
{
	u32_t flags, po, v;
	int r;

	NOREC_ENTER(vmcheckrange);

	if(!HASPT(target))
		NOREC_RETURN(vmcheckrange, OK);

	/* If caller has had a reply to this request, return it. */
	if(RTS_ISSET(caller, VMREQUEST)) {
		if(caller->p_vmrequest.who == target->p_endpoint) {
			if(caller->p_vmrequest.vmresult == VMSUSPEND)
				minix_panic("check sees VMSUSPEND?", NO_NUM);
			RTS_LOCK_UNSET(caller, VMREQUEST);
#if 0
			kprintf("SYSTEM: vm_checkrange: returning vmresult %d\n",
				caller->p_vmrequest.vmresult);
#endif
			NOREC_RETURN(vmcheckrange, caller->p_vmrequest.vmresult);
		} else {
#if 0
			kprintf("SYSTEM: vm_checkrange: caller has a request for %d, "
				"but our target is %d\n",
				caller->p_vmrequest.who, target->p_endpoint);
#endif
		}
	}

	po = vir % I386_PAGE_SIZE;
	if(po > 0) {
		vir -= po;
		bytes += po;
	}

	vmassert(target);
	vmassert(bytes > 0);

	for(v = vir; v < vir + bytes;  v+= I386_PAGE_SIZE) {
		u32_t phys;

		/* If page exists and it's writable if desired, we're OK
		 * for this page.
		 */
		if(vm_lookup(target, v, &phys, &flags) == OK &&
			!(wrfl && !(flags & I386_VM_WRITE))) {
			continue;
		}

		if(!checkonly) {
			/* Set parameters in caller. */			
			vm_suspend(caller, target);
			caller->p_vmrequest.writeflag = wrfl;			
			caller->p_vmrequest.start = vir;			
			caller->p_vmrequest.length = bytes;			
			caller->p_vmrequest.who = target->p_endpoint;		
		}

		/* SYSTEM loop will fill in VMSTYPE_SYS_MESSAGE. */
		NOREC_RETURN(vmcheckrange, VMSUSPEND);
	}

	NOREC_RETURN(vmcheckrange, OK);
}

char *flagstr(u32_t e, int dir)
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

void vm_pt_print(u32_t *pagetable, u32_t v)
{
	int pte, l = 0;
	int col = 0;

	vmassert(!((u32_t) pagetable % I386_PAGE_SIZE));

	for(pte = 0; pte < I386_VM_PT_ENTRIES; pte++) {
		u32_t pte_v, pfa;
		pte_v = phys_get32((u32_t) (pagetable + pte));
		if(!(pte_v & I386_VM_PRESENT))
			continue;
		pfa = I386_VM_PFA(pte_v);
		kprintf("%4d:%08lx:%08lx %2s ",
			pte, v + I386_PAGE_SIZE*pte, pfa,
			(pte_v & I386_VM_WRITE) ? "rw":"RO");
		col++;
		if(col == 3) { kprintf("\n"); col = 0; }
	}
	if(col > 0) kprintf("\n");

	return;
}

void vm_print(u32_t *root)
{
	int pde;

	vmassert(!((u32_t) root % I386_PAGE_SIZE));

	printf("page table 0x%lx:\n", root);

	for(pde = 10; pde < I386_VM_DIR_ENTRIES; pde++) {
		u32_t pde_v;
		u32_t *pte_a;
		pde_v = phys_get32((u32_t) (root + pde));
		if(!(pde_v & I386_VM_PRESENT))
			continue;
		if(pde_v & I386_VM_BIGPAGE) {
			kprintf("%4d: 0x%lx, flags %s\n",
				pde, I386_VM_PFA(pde_v), flagstr(pde_v, 1));
		} else {
			pte_a = (u32_t *) I386_VM_PFA(pde_v);
			kprintf("%4d: pt %08lx %s\n",
				pde, pte_a, flagstr(pde_v, 1));
			vm_pt_print(pte_a, pde * I386_VM_PT_ENTRIES * I386_PAGE_SIZE);
			kprintf("\n");
		}
	}


	return;
}

/*===========================================================================*
 *				invlpg_range				     *
 *===========================================================================*/
void invlpg_range(u32_t lin, u32_t bytes)
{
/* Remove a range of translated addresses from the TLB. 
 * Addresses are in linear, i.e., post-segment, pre-pagetable
 * form. Parameters are byte values, any offset and any multiple.
 */
	u32_t cr3;
	u32_t o, limit, addr;
	limit = lin + bytes - 1;
	o = lin % I386_PAGE_SIZE;
	lin -= o;
	limit = (limit + o) & I386_VM_ADDR_MASK;
	for(i386_invlpg_addr = lin; i386_invlpg_addr <= limit;
	    i386_invlpg_addr += I386_PAGE_SIZE)
		level0(i386_invlpg_level0);
}

u32_t thecr3;

u32_t read_cr3(void)
{
	level0(getcr3val);
	return thecr3;
}

/* This macro sets up a mapping from within the kernel's address
 * space to any other area of memory, either straight physical
 * memory (PROC == NULL) or a process view of memory, in 4MB chunks.
 * It recognizes PROC having kernel address space as a special case.
 *
 * It sets PTR to the pointer within kernel address space at the start
 * of the 4MB chunk, and OFFSET to the offset within that chunk
 * that corresponds to LINADDR.
 *
 * It needs FREEPDE (available and addressable PDE within kernel
 * address space), SEG (hardware segment), VIRT (in-datasegment
 * address if known).
 */
#define CREATEPDE(PROC, PTR, LINADDR, OFFSET, FREEPDE, VIRT, SEG, REMAIN, BYTES) { \
	FIXME("CREATEPDE: check if invlpg is necessary");	\
	if(PROC == ptproc) {					\
		FIXME("CREATEPDE: use in-memory process");	\
	}							\
	if((PROC) && iskernelp(PROC) && SEG == D) {		\
		PTR = VIRT;					\
		OFFSET = 0;					\
	} else {						\
		u32_t pdeval, *pdevalptr, newlin;		\
		int pde_index;					\
		vmassert(psok);					\
		pde_index = I386_VM_PDE(LINADDR);		\
		vmassert(!iskernelp(PROC));			\
		createpde++;					\
		if(PROC) {					\
			u32_t *pdeptr; \
			vmassert(!iskernelp(PROC));		\
			vmassert(HASPT(PROC));			\
			pdeptr = PROCPDEPTR(PROC, pde_index);		\
			pdeval = *pdeptr;	\
		} else {						\
			pdeval = (LINADDR & I386_VM_ADDR_MASK_4MB) | 	\
				I386_VM_BIGPAGE | I386_VM_PRESENT | 	\
				I386_VM_WRITE | I386_VM_USER;		\
		}							\
		*PROCPDEPTR(ptproc, FREEPDE) = pdeval;			\
		newlin = I386_BIG_PAGE_SIZE*FREEPDE;			\
		PTR = (u8_t *) phys2vir(newlin);			\
		OFFSET = LINADDR & I386_VM_OFFSET_MASK_4MB;		\
		REMAIN = MIN(REMAIN, I386_BIG_PAGE_SIZE - OFFSET); 	\
		invlpg_range(newlin + OFFSET, REMAIN);			\
	}								\
}


/*===========================================================================*
 *				arch_switch_copymsg			     *
 *===========================================================================*/
phys_bytes arch_switch_copymsg(struct proc *rp, message *m, phys_bytes lin)
{
	phys_bytes r;
	if(rp->p_seg.p_cr3) {
		vm_set_cr3(rp->p_seg.p_cr3);
		ptproc = rp;
	}
	r = phys_copy(vir2phys(m), lin, sizeof(message));
}

/*===========================================================================*
 *				lin_lin_copy				     *
 *===========================================================================*/
int lin_lin_copy(struct proc *srcproc, vir_bytes srclinaddr, u8_t *vsrc,
	int srcseg,
	struct proc *dstproc, vir_bytes dstlinaddr, u8_t *vdst,
	int dstseg,
	vir_bytes bytes)
{
	u32_t addr;
	int procslot;
	u32_t catchrange_dst, catchrange_lo, catchrange_hi;
	NOREC_ENTER(linlincopy);

	linlincopies++;

	if(srcproc && dstproc && iskernelp(srcproc) && iskernelp(dstproc)) {
		memcpy(vdst, vsrc, bytes);
		NOREC_RETURN(linlincopy, OK);
	}

	FIXME("lin_lin_copy requires big pages");
	vmassert(vm_running);
	vmassert(!catch_pagefaults);
	vmassert(nfreepdes >= 3);

	vmassert(ptproc);
	vmassert(proc_ptr);
	vmassert(read_cr3() == ptproc->p_seg.p_cr3);

	procslot = ptproc->p_nr;

	vmassert(procslot >= 0 && procslot < I386_VM_DIR_ENTRIES);
	vmassert(freepdes[FREEPDE_SRC] < freepdes[FREEPDE_DST]);

	catchrange_lo  = I386_BIG_PAGE_SIZE*freepdes[FREEPDE_SRC];
	catchrange_dst = I386_BIG_PAGE_SIZE*freepdes[FREEPDE_DST];
	catchrange_hi  = I386_BIG_PAGE_SIZE*(freepdes[FREEPDE_DST]+1);

	while(bytes > 0) {
		u8_t *srcptr, *dstptr;
		vir_bytes srcoffset, dstoffset;
		vir_bytes chunk = bytes;

		/* Set up 4MB ranges. */
		CREATEPDE(srcproc, srcptr, srclinaddr, srcoffset,
			freepdes[FREEPDE_SRC], vsrc, srcseg, chunk, bytes);
		CREATEPDE(dstproc, dstptr, dstlinaddr, dstoffset,
			freepdes[FREEPDE_DST], vdst, dstseg, chunk, bytes);

		/* Copy pages. */
		vmassert(intr_disabled());
		vmassert(!catch_pagefaults);
		catch_pagefaults = 1;
		addr=_memcpy_k(dstptr + dstoffset, srcptr + srcoffset, chunk);
		vmassert(intr_disabled());
		vmassert(catch_pagefaults);
		catch_pagefaults = 0;

		if(addr) {
			if(addr >= catchrange_lo && addr < catchrange_dst) {
				NOREC_RETURN(linlincopy, EFAULT_SRC);
			}
			if(addr >= catchrange_dst && addr < catchrange_hi) {
				NOREC_RETURN(linlincopy, EFAULT_DST);
			}
			minix_panic("lin_lin_copy fault out of range", NO_NUM);

			/* Not reached. */
			NOREC_RETURN(linlincopy, EFAULT);
		}
		
		vmassert(memcmp(dstptr + dstoffset, srcptr + srcoffset, chunk) == 0);

		/* Update counter and addresses for next iteration, if any. */
		bytes -= chunk;
		srclinaddr += chunk;
		dstlinaddr += chunk;
		vsrc += chunk;
		vdst += chunk;
	}

	NOREC_RETURN(linlincopy, OK);
}

/*===========================================================================*
 *				lin_memset				     *
 *===========================================================================*/
int vm_phys_memset(phys_bytes ph, u8_t c, phys_bytes bytes)
{
	char *v;

	physzero++;

	if(!vm_running) {
		u32_t p;
		p = c | (c << 8) | (c << 16) | (c << 24);
		phys_memset(ph, p, bytes);
		return OK;
	}

	vmassert(nfreepdes >= 3);

	/* With VM, we have to map in the physical memory. 
	 * We can do this 4MB at a time.
	 */
	while(bytes > 0) {
		vir_bytes chunk = bytes;
		u8_t *ptr;
		u32_t offset;
		CREATEPDE(((struct proc *) NULL), ptr, ph,
			offset, freepdes[FREEPDE_MEMSET], 0, 0, chunk, bytes);
		/* We can memset as many bytes as we have remaining,
		 * or as many as remain in the 4MB chunk we mapped in.
		 */
		memset(ptr + offset, c, chunk);
		bytes -= chunk;
		ph += chunk;
	}


	return OK;
}

/*===========================================================================*
 *				virtual_copy_f				     *
 *===========================================================================*/
PUBLIC int virtual_copy_f(src_addr, dst_addr, bytes, vmcheck)
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
  int i, r;
  struct proc *procs[2];
  NOREC_ENTER(virtualcopy);

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
		NOREC_RETURN(virtualcopy, EDEADSRCDST);
	  }
          seg_index = vir_addr[i]->segment & SEGMENT_INDEX;
	  if(type == LOCAL_SEG)
	          phys_addr[i] = umap_local(p, seg_index, vir_addr[i]->offset,
			bytes);
	  else
	  	phys_addr[i] = umap_virtual(p, seg_index, vir_addr[i]->offset,
			bytes);
	  if(phys_addr[i] == 0) {
		kprintf("virtual_copy: map 0x%x failed for %s seg %d, "
			"offset %lx, len %d, i %d\n",
			type, p->p_name, seg_index, vir_addr[i]->offset,
			bytes, i);
	  }
          break;
      case REMOTE_SEG:
	  if(!p) {
		NOREC_RETURN(virtualcopy, EDEADSRCDST);
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
      case GRANT_SEG:
	  phys_addr[i] = umap_grant(p, vir_addr[i]->offset, bytes);
	  break;
      default:
	  kprintf("virtual_copy: strange type 0x%x\n", type);
	  NOREC_RETURN(virtualcopy, EINVAL);
      }

      /* Check if mapping succeeded. */
      if (phys_addr[i] <= 0 && vir_addr[i]->segment != PHYS_SEG)  {
      kprintf("virtual_copy EFAULT\n");
	  NOREC_RETURN(virtualcopy, EFAULT);
      }
  }

  if(vm_running) {
	int r;
	struct proc *target, *caller;

	if((r=lin_lin_copy(procs[_SRC_], phys_addr[_SRC_],
		 (u8_t *) src_addr->offset, src_addr->segment,
		procs[_DST_], phys_addr[_DST_], (u8_t *) dst_addr->offset,
		dst_addr->segment, bytes)) != OK) {
		if(r != EFAULT_SRC && r != EFAULT_DST)
			minix_panic("lin_lin_copy failed", r);
		if(!vmcheck) {
	  		NOREC_RETURN(virtualcopy, r);
		}

		caller = proc_addr(who_p);

		vmassert(procs[_SRC_] && procs[_DST_]);

		if(r == EFAULT_SRC) {
			caller->p_vmrequest.start = phys_addr[_SRC_];
			target = procs[_SRC_];
			caller->p_vmrequest.writeflag = 0;
		} else if(r == EFAULT_DST) {
			caller->p_vmrequest.start = phys_addr[_DST_];
			target = procs[_DST_];
			caller->p_vmrequest.writeflag = 1;
		} else {
			minix_panic("r strange", r);
		}

#if 0
		printf("virtual_copy: suspending caller %d / %s, target %d / %s\n",
			caller->p_endpoint, caller->p_name,
			target->p_endpoint, target->p_name);
#endif

		caller->p_vmrequest.length = bytes;
		caller->p_vmrequest.who = target->p_endpoint;

		vm_suspend(caller, target);

	  	NOREC_RETURN(virtualcopy, VMSUSPEND);
	}

  	NOREC_RETURN(virtualcopy, OK);
  }

  vmassert(!vm_running);

  /* can't copy to/from process with PT without VM */
#define NOPT(p) (!(p) || !HASPT(p))
  if(!NOPT(procs[_SRC_])) {
	kprintf("ignoring page table src: %s / %d at 0x%lx\n",
		procs[_SRC_]->p_name, procs[_SRC_]->p_endpoint, procs[_SRC_]->p_seg.p_cr3);
}
  if(!NOPT(procs[_DST_])) {
	kprintf("ignoring page table dst: %s / %d at 0x%lx\n",
		procs[_DST_]->p_name, procs[_DST_]->p_endpoint,
		procs[_DST_]->p_seg.p_cr3);
  }

  /* Now copy bytes between physical addresseses. */
  phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes);
 
  NOREC_RETURN(virtualcopy, OK);
}

/*===========================================================================*
 *				data_copy				     *
 *===========================================================================*/
PUBLIC int data_copy(
	endpoint_t from_proc, vir_bytes from_addr,
	endpoint_t to_proc, vir_bytes to_addr,
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
 *				arch_pre_exec				     *
 *===========================================================================*/
PUBLIC int arch_pre_exec(struct proc *pr, u32_t ip, u32_t sp)
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
PUBLIC int arch_umap(struct proc *pr, vir_bytes offset, vir_bytes count,
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
void i386_freepde(int pde)
{
	if(nfreepdes >= WANT_FREEPDES)
		return;
	freepdes[nfreepdes++] = pde;
}
