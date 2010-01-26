

#include "../../kernel.h"
#include "../../proc.h"
#include "../../vm.h"

#include <minix/type.h>
#include <minix/syslib.h>
#include <minix/cpufeature.h>
#include <string.h>

#include <sys/vm_i386.h>

#include <minix/portio.h>

#include "proto.h"
#include "../../proto.h"
#include "../../proto.h"
#include "../../debug.h"

#ifdef CONFIG_APIC
#include "apic.h"
#ifdef CONFIG_WATCHDOG
#include "../../watchdog.h"
#endif
#endif

PRIVATE int psok = 0;

#define PROCPDEPTR(pr, pi) ((u32_t *) ((u8_t *) vm_pagedirs +\
				I386_PAGE_SIZE * pr->p_nr +	\
				I386_VM_PT_ENT_SIZE * pi))

PUBLIC u8_t *vm_pagedirs = NULL;

#define NOPDE (-1)
#define PDEMASK(n) (1L << (n))
PUBLIC u32_t dirtypde;  /* Accessed from assembly code. */
#define WANT_FREEPDES (sizeof(dirtypde)*8-5)
PRIVATE int nfreepdes = 0, freepdes[WANT_FREEPDES], inusepde = NOPDE;

#define HASPT(procptr) ((procptr)->p_seg.p_cr3 != 0)

FORWARD _PROTOTYPE( u32_t phys_get32, (phys_bytes v)			);
FORWARD _PROTOTYPE( void vm_enable_paging, (void)			);
FORWARD _PROTOTYPE( void set_cr3, (void)			);

	
/* *** Internal VM Functions *** */

PUBLIC void vm_init(struct proc *newptproc)
{
	if(vm_running)
		minix_panic("vm_init: vm_running", NO_NUM);
	vm_set_cr3(newptproc);
	level0(vm_enable_paging);
	vm_running = 1;

}

#define TYPEDIRECT	0
#define TYPEPROCMAP	1
#define TYPEPHYS	2

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
#define CREATEPDE(PROC, PTR, LINADDR, REMAIN, BYTES, PDE, TYPE) { \
	u32_t *pdeptr = NULL; 				\
	int proc_pde_index;					\
	proc_pde_index = I386_VM_PDE(LINADDR);			\
	PDE = NOPDE;						\
	if((PROC) && (((PROC) == ptproc) || !HASPT(PROC))) {	\
		PTR = LINADDR;					\
		TYPE = TYPEDIRECT;				\
	} else {						\
		int fp;						\
		int mustinvl;					\
		u32_t pdeval, *pdevalptr, mask;			\
		phys_bytes offset;				\
		vmassert(psok);					\
		if(PROC) {					\
			TYPE = TYPEPROCMAP;				\
			vmassert(!iskernelp(PROC));		\
			vmassert(HASPT(PROC));			\
			pdeptr = PROCPDEPTR(PROC, proc_pde_index);	\
			pdeval = *pdeptr;			\
		} else {					\
			TYPE = TYPEPHYS;				\
			pdeval = (LINADDR & I386_VM_ADDR_MASK_4MB) | 	\
				I386_VM_BIGPAGE | I386_VM_PRESENT | 	\
				I386_VM_WRITE | I386_VM_USER;		\
		}							\
		for(fp = 0; fp < nfreepdes; fp++) {			\
			int k = freepdes[fp];				\
			if(inusepde == k)				\
				continue;				\
			*PROCPDEPTR(ptproc, k) = 0;			\
			PDE = k;					\
			vmassert(k >= 0);				\
			vmassert(k < sizeof(dirtypde)*8);		\
			mask = PDEMASK(PDE);				\
			if(dirtypde & mask)				\
				continue;				\
			break;						\
		}							\
		vmassert(PDE != NOPDE);					\
		vmassert(mask);						\
		if(dirtypde & mask) {					\
			mustinvl = 1;					\
		} else {						\
			mustinvl = 0;					\
		}							\
		inusepde = PDE;						\
		*PROCPDEPTR(ptproc, PDE) = pdeval;			\
		offset = LINADDR & I386_VM_OFFSET_MASK_4MB;		\
		PTR = I386_BIG_PAGE_SIZE*PDE + offset;			\
		REMAIN = MIN(REMAIN, I386_BIG_PAGE_SIZE - offset); 	\
		if(1 || mustinvl) {					\
			level0(reload_cr3); 				\
		}							\
	}								\
}

#define DONEPDE(PDE)	{				\
	if(PDE != NOPDE) {				\
		vmassert(PDE > 0);			\
		vmassert(PDE < sizeof(dirtypde)*8);	\
		dirtypde |= PDEMASK(PDE);		\
	}						\
}

#define WIPEPDE(PDE)	{				\
	if(PDE != NOPDE) {				\
		vmassert(PDE > 0);			\
		vmassert(PDE < sizeof(dirtypde)*8);	\
		*PROCPDEPTR(ptproc, PDE) = 0;		\
	}						\
}

/*===========================================================================*
 *				lin_lin_copy				     *
 *===========================================================================*/
PRIVATE int lin_lin_copy(struct proc *srcproc, vir_bytes srclinaddr, 
	struct proc *dstproc, vir_bytes dstlinaddr, vir_bytes bytes)
{
	u32_t addr;
	int procslot;

	NOREC_ENTER(linlincopy);

	vmassert(vm_running);
	vmassert(nfreepdes >= 3);

	vmassert(ptproc);
	vmassert(proc_ptr);
	vmassert(read_cr3() == ptproc->p_seg.p_cr3);

	procslot = ptproc->p_nr;

	vmassert(procslot >= 0 && procslot < I386_VM_DIR_ENTRIES);

	while(bytes > 0) {
		phys_bytes srcptr, dstptr;
		vir_bytes chunk = bytes;
		int srcpde, dstpde;
		int srctype, dsttype;

		/* Set up 4MB ranges. */
		inusepde = NOPDE;
		CREATEPDE(srcproc, srcptr, srclinaddr, chunk, bytes, srcpde, srctype);
		CREATEPDE(dstproc, dstptr, dstlinaddr, chunk, bytes, dstpde, dsttype);

		/* Copy pages. */
		PHYS_COPY_CATCH(srcptr, dstptr, chunk, addr);

		DONEPDE(srcpde);
		DONEPDE(dstpde);

		if(addr) {
			/* If addr is nonzero, a page fault was caught. */

			if(addr >= srcptr && addr < (srcptr + chunk)) {
				WIPEPDE(srcpde);
				WIPEPDE(dstpde);
				NOREC_RETURN(linlincopy, EFAULT_SRC);
			}
			if(addr >= dstptr && addr < (dstptr + chunk)) {
				WIPEPDE(srcpde);
				WIPEPDE(dstpde);
				NOREC_RETURN(linlincopy, EFAULT_DST);
			}

			minix_panic("lin_lin_copy fault out of range", NO_NUM);

			/* Not reached. */
			NOREC_RETURN(linlincopy, EFAULT);
		}

		WIPEPDE(srcpde);
		WIPEPDE(dstpde);

		/* Update counter and addresses for next iteration, if any. */
		bytes -= chunk;
		srclinaddr += chunk;
		dstlinaddr += chunk;
	}

	NOREC_RETURN(linlincopy, OK);
}


PRIVATE u32_t phys_get32(phys_bytes addr)
{
	u32_t v;
	int r;

	if(!vm_running) {
		phys_copy(addr, vir2phys(&v), sizeof(v));
		return v;
	}

	if((r=lin_lin_copy(NULL, addr, 
		proc_addr(SYSTEM), vir2phys(&v), sizeof(v))) != OK) {
		minix_panic("lin_lin_copy for phys_get32 failed", r);
	}

	return v;
}

PRIVATE u32_t vm_cr3;	/* temp arg to level0() func */

PRIVATE void set_cr3()
{
	write_cr3(vm_cr3);
}

PUBLIC void vm_set_cr3(struct proc *newptproc)
{
	int u = 0;
	if(!intr_disabled()) { lock; u = 1; }
	vm_cr3= newptproc->p_seg.p_cr3;
	if(vm_cr3) {
		level0(set_cr3);
		ptproc = newptproc;
	}
	if(u) { unlock; }
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
		return umap_grant(rp, vir_addr, bytes);
	}
	
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
	vmassert(!isemptyp(proc));

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

	return 1;
}

/*===========================================================================*
 *                              vm_suspend                                *
 *===========================================================================*/
PRIVATE int vm_suspend(struct proc *caller, struct proc *target,
	vir_bytes linaddr, vir_bytes len, int wrflag, int type)
{
	/* This range is not OK for this process. Set parameters  
	 * of the request and notify VM about the pending request. 
	 */								
	vmassert(!RTS_ISSET(caller, RTS_VMREQUEST));
	vmassert(!RTS_ISSET(target, RTS_VMREQUEST));

	RTS_LOCK_SET(caller, RTS_VMREQUEST);

#if DEBUG_VMASSERT
	caller->p_vmrequest.stacktrace[0] = '\0';
	util_stacktrace_strcat(caller->p_vmrequest.stacktrace);
#endif

	caller->p_vmrequest.req_type = VMPTYPE_CHECK;
	caller->p_vmrequest.target = target->p_endpoint;
	caller->p_vmrequest.params.check.start = linaddr;
	caller->p_vmrequest.params.check.length = len;
	caller->p_vmrequest.params.check.writeflag = 1;
	caller->p_vmrequest.type = type;
							
	/* Connect caller on vmrequest wait queue. */	
	if(!(caller->p_vmrequest.nextrequestor = vmrequest))
		mini_notify(proc_addr(SYSTEM), VM_PROC_NR);
	vmrequest = caller;
}

/*===========================================================================*
 *                              delivermsg                                *
 *===========================================================================*/
int delivermsg(struct proc *rp)
{
	phys_bytes addr;  
	int r;
	NOREC_ENTER(deliver);

	vmassert(rp->p_misc_flags & MF_DELIVERMSG);
	vmassert(rp->p_delivermsg.m_source != NONE);

	vmassert(rp->p_delivermsg_lin);
#if DEBUG_VMASSERT
	if(rp->p_delivermsg_lin !=
		umap_local(rp, D, rp->p_delivermsg_vir, sizeof(message))) {
		printf("vir: 0x%lx lin was: 0x%lx umap now: 0x%lx\n",
		rp->p_delivermsg_vir, rp->p_delivermsg_lin,
		umap_local(rp, D, rp->p_delivermsg_vir, sizeof(message)));
		minix_panic("that's wrong", NO_NUM);
	}

#endif

	vm_set_cr3(rp);

	PHYS_COPY_CATCH(vir2phys(&rp->p_delivermsg),
		rp->p_delivermsg_lin, sizeof(message), addr);

	if(addr) {
		vm_suspend(rp, rp, rp->p_delivermsg_lin, sizeof(message), 1,
			VMSTYPE_DELIVERMSG);
		r = VMSUSPEND;
	} else {
#if DEBUG_VMASSERT
		rp->p_delivermsg.m_source = NONE;
		rp->p_delivermsg_lin = 0;
#endif
		rp->p_misc_flags &= ~MF_DELIVERMSG;
		r = OK;
	}

	NOREC_RETURN(deliver, r);
}

PRIVATE char *flagstr(u32_t e, int dir)
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

PRIVATE void vm_pt_print(u32_t *pagetable, u32_t v)
{
	int pte;
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

PRIVATE void vm_print(u32_t *root)
{
	int pde;

	vmassert(!((u32_t) root % I386_PAGE_SIZE));

	printf("page table 0x%lx:\n", root);

	for(pde = 0; pde < I386_VM_DIR_ENTRIES; pde++) {
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

u32_t thecr3;

u32_t read_cr3(void)
{
	level0(getcr3val);
	return thecr3;
}


/*===========================================================================*
 *				lin_memset				     *
 *===========================================================================*/
int vm_phys_memset(phys_bytes ph, u8_t c, phys_bytes bytes)
{
	u32_t p;
	NOREC_ENTER(physmemset);

	p = c | (c << 8) | (c << 16) | (c << 24);

	if(!vm_running) {
		phys_memset(ph, p, bytes);
		NOREC_RETURN(physmemset, OK);
	}

	vmassert(nfreepdes >= 3);

	/* With VM, we have to map in the physical memory. 
	 * We can do this 4MB at a time.
	 */
	while(bytes > 0) {
		int pde, t;
		vir_bytes chunk = (vir_bytes) bytes;
		phys_bytes ptr;
		inusepde = NOPDE;
		CREATEPDE(((struct proc *) NULL), ptr, ph, chunk, bytes, pde, t);
		/* We can memset as many bytes as we have remaining,
		 * or as many as remain in the 4MB chunk we mapped in.
		 */
		phys_memset(ptr, p, chunk);
		DONEPDE(pde);
		bytes -= chunk;
		ph += chunk;
	}


	NOREC_RETURN(physmemset, OK);
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
  int i;
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
	struct proc *caller;

	caller = proc_addr(who_p);

	if(RTS_ISSET(caller, RTS_VMREQUEST)) {
		vmassert(caller->p_vmrequest.vmresult != VMSUSPEND);
		RTS_LOCK_UNSET(caller, RTS_VMREQUEST);
		if(caller->p_vmrequest.vmresult != OK) {
#if DEBUG_VMASSERT
			printf("virtual_copy: returning VM error %d\n",
				caller->p_vmrequest.vmresult);
#endif
	  		NOREC_RETURN(virtualcopy, caller->p_vmrequest.vmresult);
		}
	}

	if((r=lin_lin_copy(procs[_SRC_], phys_addr[_SRC_],
		procs[_DST_], phys_addr[_DST_], bytes)) != OK) {
		struct proc *target;
		int wr;
		phys_bytes lin;
		if(r != EFAULT_SRC && r != EFAULT_DST)
			minix_panic("lin_lin_copy failed", r);
		if(!vmcheck) {
	  		NOREC_RETURN(virtualcopy, r);
		}

		vmassert(procs[_SRC_] && procs[_DST_]);

		if(r == EFAULT_SRC) {
			lin = phys_addr[_SRC_];
			target = procs[_SRC_];
			wr = 0;
		} else if(r == EFAULT_DST) {
			lin = phys_addr[_DST_];
			target = procs[_DST_];
			wr = 1;
		} else {
			minix_panic("r strange", r);
		}

#if 0
		printf("virtual_copy: suspending caller %d / %s, target %d / %s\n",
			caller->p_endpoint, caller->p_name,
			target->p_endpoint, target->p_name);
#endif

		vmassert(proc_ptr->p_endpoint == SYSTEM);
		vm_suspend(caller, target, lin, bytes, wr, VMSTYPE_KERNELCALL);

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
  if(phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes))
  	NOREC_RETURN(virtualcopy, EFAULT);
 
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
 *				data_copy_vmcheck			     *
 *===========================================================================*/
PUBLIC int data_copy_vmcheck(
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

  return virtual_copy_vmcheck(&src, &dst, bytes);
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

PUBLIC arch_phys_map(int index, phys_bytes *addr, phys_bytes *len, int *flags)
{
#ifdef CONFIG_APIC
	/* map the local APIC if enabled */
	if (index == 0 && lapic_addr) {
		*addr = vir2phys(lapic_addr);
		*len = 4 << 10 /* 4kB */;
		*flags = VMMF_UNCACHED;
		return OK;
	}
	return EINVAL;
#else
	/* we don't want anything */
	return EINVAL;
#endif
}

PUBLIC int arch_phys_map_reply(int index, vir_bytes addr)
{
#ifdef CONFIG_APIC
	/* if local APIC is enabled */
	if (index == 0 && lapic_addr) {
		lapic_addr_vaddr = addr;
	}
#endif
	return OK;
}

PUBLIC int arch_enable_paging(void)
{
#ifdef CONFIG_APIC
	/* if local APIC is enabled */
	if (lapic_addr) {
		lapic_addr = lapic_addr_vaddr;
		lapic_eoi_addr = LAPIC_EOI;
	}
#endif
#ifdef CONFIG_WATCHDOG
	/*
	 * We make sure that we don't enable the watchdog until paging is turned
	 * on as we might get a NMI while switching and we might still use wrong
	 * lapic address. Bad things would happen. It is unfortunate but such is
	 * life
	 */
	level0(i386_watchdog_start);
#endif

	return OK;
}
