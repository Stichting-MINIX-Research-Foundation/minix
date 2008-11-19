
#include "../../kernel.h"
#include "../../proc.h"
#include "../../vm.h"

#include <minix/type.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <string.h>

#include <sys/vm_i386.h>

#include <minix/portio.h>

#include "proto.h"
#include "../../proto.h"
#include "../../debug.h"

/* VM functions and data. */
PRIVATE u32_t vm_cr3;
PUBLIC u32_t kernel_cr3;
extern u32_t cswitch;
u32_t last_cr3 = 0;

FORWARD _PROTOTYPE( void phys_put32, (phys_bytes addr, u32_t value)	);
FORWARD _PROTOTYPE( u32_t phys_get32, (phys_bytes addr)			);
FORWARD _PROTOTYPE( void vm_set_cr3, (u32_t value)			);
FORWARD _PROTOTYPE( void set_cr3, (void)				);
FORWARD _PROTOTYPE( void vm_enable_paging, (void)			);

#if DEBUG_VMASSERT
#define vmassert(t) { \
	if(!(t)) { minix_panic("vm: assert " #t " failed\n", __LINE__); } }
#else
#define vmassert(t) { }
#endif

/* *** Internal VM Functions *** */

PUBLIC void vm_init(void)
{
	int o;
	phys_bytes p, pt_size;
	phys_bytes vm_dir_base, vm_pt_base, phys_mem;
	u32_t entry;
	unsigned pages;
	struct proc* rp;

	if (!vm_size)
		minix_panic("i386_vm_init: no space for page tables", NO_NUM);

	if(vm_running)
		return;

	/* Align page directory */
	o= (vm_base % I386_PAGE_SIZE);
	if (o != 0)
		o= I386_PAGE_SIZE-o;
	vm_dir_base= vm_base+o;

	/* Page tables start after the page directory */
	vm_pt_base= vm_dir_base+I386_PAGE_SIZE;

	pt_size= (vm_base+vm_size)-vm_pt_base;
	pt_size -= (pt_size % I386_PAGE_SIZE);

	/* Compute the number of pages based on vm_mem_high */
	pages= (vm_mem_high-1)/I386_PAGE_SIZE + 1;

	if (pages * I386_VM_PT_ENT_SIZE > pt_size)
		minix_panic("i386_vm_init: page table too small", NO_NUM);

	for (p= 0; p*I386_VM_PT_ENT_SIZE < pt_size; p++)
	{
		phys_mem= p*I386_PAGE_SIZE;
		entry= phys_mem | I386_VM_USER | I386_VM_WRITE |
			I386_VM_PRESENT;
		if (phys_mem >= vm_mem_high)
			entry= 0;
		phys_put32(vm_pt_base + p*I386_VM_PT_ENT_SIZE, entry);
	}

	for (p= 0; p < I386_VM_DIR_ENTRIES; p++)
	{
		phys_mem= vm_pt_base + p*I386_PAGE_SIZE;
		entry= phys_mem | I386_VM_USER | I386_VM_WRITE |
			I386_VM_PRESENT;
		if (phys_mem >= vm_pt_base + pt_size)
			entry= 0;
		phys_put32(vm_dir_base + p*I386_VM_PT_ENT_SIZE, entry);
	}

	/* Set this cr3 in all currently running processes for
	 * future context switches.
	 */
	for (rp=BEG_PROC_ADDR; rp<END_PROC_ADDR; rp++) {
		u32_t mycr3;
		if(isemptyp(rp)) continue;
		rp->p_seg.p_cr3 = vm_dir_base;
	}

	kernel_cr3 = vm_dir_base;

	/* Set this cr3 now (not active until paging enabled). */
	vm_set_cr3(vm_dir_base);

	/* Actually enable paging (activating cr3 load above). */
	level0(vm_enable_paging);

	/* Don't do this init in the future. */
	vm_running = 1;
}

PRIVATE void phys_put32(addr, value)
phys_bytes addr;
u32_t value;
{
	phys_copy(vir2phys((vir_bytes)&value), addr, sizeof(value));
}

PRIVATE u32_t phys_get32(addr)
phys_bytes addr;
{
	u32_t value;

	phys_copy(addr, vir2phys((vir_bytes)&value), sizeof(value));

	return value;
}

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

PRIVATE void vm_enable_paging(void)
{
	u32_t cr0;

	cr0= read_cr0();
	write_cr0(cr0 | I386_CR0_PG);
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

	vmassert(proc);
	vmassert(physical);
	vmassert(!(proc->p_rts_flags & SLOT_FREE));

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
		return EFAULT;
	}

	/* Retrieve page table entry. */
	pt = (u32_t *) I386_VM_PFA(pde_v);
	vmassert(!((u32_t) pt % I386_PAGE_SIZE));
	pte = I386_VM_PTE(virtual);
	vmassert(pte >= 0 && pte < I386_VM_PT_ENTRIES);
	pte_v = phys_get32((u32_t) (pt + pte));
	if(!(pte_v & I386_VM_PRESENT)) {
#if 0
		kprintf("vm_lookup: %d:%s:0x%lx: cr3 %lx: pde %d: pte %d not present\n",
			proc->p_endpoint, proc->p_name, virtual, root, pde, pte);
		kprintf("kernel stack: ");
		util_stacktrace();
#endif
		return EFAULT;
	}

	if(ptent) *ptent = pte_v;

	/* Actual address now known; retrieve it and add page offset. */
	*physical = I386_VM_PFA(pte_v);
	*physical += virtual % I386_PAGE_SIZE;

	return OK;
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
 *                              vm_copy                                      *
 *===========================================================================*/
int vm_copy(vir_bytes src, struct proc *srcproc,
	 vir_bytes dst, struct proc *dstproc, phys_bytes bytes)
{
#define WRAPS(v) (ULONG_MAX - (v) <= bytes)

	if(WRAPS(src) || WRAPS(dst))
		minix_panic("vm_copy: linear address wraps", NO_NUM);

	while(bytes > 0) {
		u32_t n, flags;
		phys_bytes p_src, p_dst;
#define PAGEREMAIN(v) (I386_PAGE_SIZE - ((v) % I386_PAGE_SIZE))

		/* We can copy this number of bytes without
		 * crossing a page boundary, but don't copy more
		 * than asked.
		 */
		n = MIN(PAGEREMAIN(src), PAGEREMAIN(dst));
		n = MIN(n, bytes);
		vmassert(n > 0);
		vmassert(n <= I386_PAGE_SIZE);

		/* Convert both virtual addresses to physical and do
		 * copy.
		 */
		LOOKUP(p_src, srcproc, src, NULL);
		LOOKUP(p_dst, dstproc, dst, &flags);
		if(!(flags & I386_VM_WRITE)) {
			kprintf("vm_copy: copying to nonwritable page\n");
			kprintf("kernel stack: ");
			util_stacktrace();
			return EFAULT;
		}
		phys_copy(p_src, p_dst, n);

		/* Book number of bytes copied. */
		vmassert(bytes >= n);
		bytes -= n;
		src += n;
		dst += n;
	}

	return OK;
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
	vmassert(vm_running);

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
 *                              vm_checkrange                                *
 *===========================================================================*/
PUBLIC int vm_checkrange(struct proc *caller, struct proc *target,
	vir_bytes vir, vir_bytes bytes, int wrfl, int checkonly)
{
	u32_t flags, po, v;
	int r;

	vmassert(vm_running);

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
			return caller->p_vmrequest.vmresult;
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
			if(vm_checkrange_verbose) {
#if 0
				kprintf("SYSTEM: checkrange:%s:%d: 0x%lx: write 0x%lx, flags 0x%lx, phys 0x%lx, OK\n",
				target->p_name, target->p_endpoint, v, wrfl, flags, phys);
#endif
			}
			continue;
		}

		if(vm_checkrange_verbose) {
			kprintf("SYSTEM: checkrange:%s:%d: 0x%lx: write 0x%lx, flags 0x%lx, phys 0x%lx, NOT OK\n",
			target->p_name, target->p_endpoint, v, wrfl, flags, phys);
		}

		if(checkonly)
			return VMSUSPEND;

		/* This range is not OK for this process. Set parameters
		 * of the request and notify VM about the pending request.
		 */
		if(RTS_ISSET(caller, VMREQUEST))
			minix_panic("VMREQUEST already set", caller->p_endpoint);
		RTS_LOCK_SET(caller, VMREQUEST);

		/* Set parameters in caller. */
		caller->p_vmrequest.writeflag = wrfl;
		caller->p_vmrequest.start = vir;
		caller->p_vmrequest.length = bytes;
		caller->p_vmrequest.who = target->p_endpoint;

		/* Set caller in target. */
		target->p_vmrequest.requestor = caller;

		/* Connect caller on vmrequest wait queue. */
		caller->p_vmrequest.nextrequestor = vmrequest;
		vmrequest = caller;
		soft_notify(VM_PROC_NR);

#if 0
		kprintf("SYSTEM: vm_checkrange: range bad for "
			"target %s:0x%lx-0x%lx, caller %s\n",
				target->p_name, vir, vir+bytes, caller->p_name);

		kprintf("vm_checkrange kernel trace: ");
		util_stacktrace();
		kprintf("target trace: ");
		proc_stacktrace(target);
#endif

		if(target->p_endpoint == VM_PROC_NR) {
			kprintf("caller trace: ");
			proc_stacktrace(caller);
			kprintf("target trace: ");
			proc_stacktrace(target);
			minix_panic("VM ranges should be OK", NO_NUM);
		}

		return VMSUSPEND;
	}

	return OK;
}

char *flagstr(u32_t e, int dir)
{
	static char str[80];
	strcpy(str, "");
#define FLAG(v) do { if(e & (v)) { strcat(str, #v " "); } } while(0)
	FLAG(I386_VM_PRESENT);
	FLAG(I386_VM_WRITE);
	FLAG(I386_VM_USER);
	FLAG(I386_VM_PWT);
	FLAG(I386_VM_PCD);
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
		kprintf("%4d:%08lx:%08lx ",
			pte, v + I386_PAGE_SIZE*pte, pfa);
		col++;
		if(col == 3) { kprintf("\n"); col = 0; }
	}
	if(col > 0) kprintf("\n");

	return;
}

/*===========================================================================*
 *                              vm_print                                     *
 *===========================================================================*/
void vm_print(u32_t *root)
{
	int pde;

	vmassert(!((u32_t) root % I386_PAGE_SIZE));

	for(pde = 0; pde < I386_VM_DIR_ENTRIES; pde++) {
		u32_t pde_v;
		u32_t *pte_a;
		pde_v = phys_get32((u32_t) (root + pde));
		if(!(pde_v & I386_VM_PRESENT))
			continue;
		pte_a = (u32_t *) I386_VM_PFA(pde_v);
		kprintf("%4d: pt %08lx %s\n",
			pde, pte_a, flagstr(pde_v, 1));
		vm_pt_print(pte_a, pde * I386_VM_PT_ENTRIES * I386_PAGE_SIZE);
	}


	return;
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
	  if(!p) return EDEADSRCDST;
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
	  if(!p) return EDEADSRCDST;
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
          return(EINVAL);
      }

      /* Check if mapping succeeded. */
      if (phys_addr[i] <= 0 && vir_addr[i]->segment != PHYS_SEG)  {
      kprintf("virtual_copy EFAULT\n");
          return(EFAULT);
      }
  }

  if(vmcheck && procs[_SRC_])
	CHECKRANGE_OR_SUSPEND(procs[_SRC_], phys_addr[_SRC_], bytes, 0);
  if(vmcheck && procs[_DST_])
	CHECKRANGE_OR_SUSPEND(procs[_DST_], phys_addr[_DST_], bytes, 1);

  /* Now copy bytes between physical addresseses. */
  if(!vm_running || (procs[_SRC_] == NULL && procs[_DST_] == NULL)) {
	/* Without vm, address ranges actually are physical. */
	phys_copy(phys_addr[_SRC_], phys_addr[_DST_], (phys_bytes) bytes);
	r = OK;
  } else {
	/* With vm, addresses need further interpretation. */
	r = vm_copy(phys_addr[_SRC_], procs[_SRC_], 
		phys_addr[_DST_], procs[_DST_], (phys_bytes) bytes);
	if(r != OK) {
		kprintf("vm_copy: %lx to %lx failed\n",
			phys_addr[_SRC_],phys_addr[_DST_]);
	}
  }

  return(r);
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


