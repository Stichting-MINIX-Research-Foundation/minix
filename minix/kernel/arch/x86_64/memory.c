
#include "kernel/kernel.h"
#include "kernel/vm.h"

#include <machine/vm.h>

#include <minix/syslib.h>
#include <minix/cpufeature.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>

#include <machine/vm.h>

#include "oxpcie.h"
#include "arch_proto.h"

#ifdef USE_APIC
#include "apic.h"
#ifdef USE_WATCHDOG
#include "kernel/watchdog.h"
#endif
#endif

#define MULTIBOOT_VIDEO_BUFFER		0xB8000

phys_bytes video_mem_vaddr = 0;

#define HASPT(procptr) ((procptr)->p_seg.p_cr3 != 0)
static int nfreepdes = 0;
#define MAXFREEPDES	2
static int freepdes[MAXFREEPDES];

void mem_clear_mapcache(void)
{
	/* x86_64: createpde() uses the kernel identity map and never transplants
	 * PDE entries into the current page table, so there is nothing to clear.
	 */
}

/* Kernel-high physical direct map base; must match PG_DIRECTMAP_BASE in
 * pg_utils.c (PML4[511], PDPT[0]). */
#define PHYS_DIRECTMAP_BASE  0xFFFFFF8000000000ULL

/*
 * Translate a physical address to a kernel-accessible (dereferenceable) virtual
 * address via the direct map (pg_utils.c pg_identity()).  The direct map lives
 * under PML4[511] and is shared into every address space through kern_pml4_hi,
 * so this works on any process's CR3.  The low identity map (phys == virt) only
 * exists in the bootstrap pg_pml4, not in VM-built per-process page tables, so
 * it cannot be used for physical access once processes are running.
 */
phys_bytes phys_to_kacc(phys_bytes pa)
{
	return (phys_bytes)(PHYS_DIRECTMAP_BASE + pa);
}

static const u64_t *pt_vaddr(phys_bytes pa)
{
	return (const u64_t *)(vir_bytes) phys_to_kacc(pa);
}

/*
 * createpde — resolve a linear address to a kernel-accessible virtual address.
 *
 * x86_64 strategy: the kernel identity-maps all physical RAM at boot
 * (pg_identity: physical addr 0..N GB == virtual addr 0..N GB).
 * For physical addresses (pr == NULL) or the current process, return linaddr
 * directly.  For other processes, walk their 4-level page table via the
 * identity map to recover the physical address, then return it (phys == virt).
 *
 * *bytes is truncated to the contiguous physical region reachable from linaddr
 * within the same page (4 KB, 2 MB, or 1 GB depending on page size).
 *
 * If 'writable' is set, the caller intends to write through the returned
 * mapping.  Because we hand back the always-writable kernel direct-map alias
 * (phys_to_kacc), the destination page's read-only protection would otherwise
 * be bypassed (i386 avoids this via CR0.WP + a process-PDE window).  So for a
 * write we return 0 (the not-present sentinel) when the leaf entry lacks the
 * write bit; lin_lin_copy then routes the access through vm_suspend(writeflag=1)
 * and VM performs copy-on-write or rejects it with EFAULT, as appropriate.
 */
static phys_bytes createpde(
	const struct proc *pr,
	const phys_bytes linaddr,
	phys_bytes *bytes,
	int writable,		/* caller will write through the mapping */
	int *changed		/* unused on x86_64 */
	)
{
	phys_bytes offset;

	if (!pr) {
		/* Caller passed a physical address (NONE/PHYS_SEG).  Make it
		 * kernel-accessible (high alias for the kernel image). */
		return phys_to_kacc(linaddr);
	}

	if (pr == get_cpulocal_var(ptproc) || iskernelp(pr)) {
		/* Already a virtual address in the current page table. */
		return linaddr;
	}

	/* Walk process 4-level page table.  Page-table pages are reached via
	 * phys_to_kacc(), not the raw physical address, and the resolved data
	 * address is returned the same way (see phys_to_kacc comment). */
	{
		const u64_t *pml4 = pt_vaddr((phys_bytes) pr->p_seg.p_cr3);
		u64_t pml4e, pdpte, pde, pte;
		const u64_t *pdpt, *pd, *pt;

		pml4e = pml4[AMD64_VM_PML4(linaddr)];
		if (!(pml4e & AMD64_VM_PRESENT))
			return 0;

		pdpt  = pt_vaddr(pml4e & AMD64_VM_ADDR_MASK);
		pdpte = pdpt[AMD64_VM_PDPT(linaddr)];
		if (!(pdpte & AMD64_VM_PRESENT))
			return 0;
		if (pdpte & AMD64_VM_PS) {		/* 1 GB page */
			phys_bytes base = pdpte & AMD64_VM_ADDR_MASK &
			                  ~((phys_bytes)((1ULL << 30) - 1));
			if (writable && !(pdpte & AMD64_VM_WRITE))
				return 0;
			offset = linaddr & ((1ULL << 30) - 1);
			*bytes = MIN(*bytes, (phys_bytes)(1ULL << 30) - offset);
			return phys_to_kacc(base + offset);
		}

		pd  = pt_vaddr(pdpte & AMD64_VM_ADDR_MASK);
		pde = pd[AMD64_VM_PD(linaddr)];
		if (!(pde & AMD64_VM_PRESENT))
			return 0;
		if (pde & AMD64_VM_PS) {		/* 2 MB page */
			phys_bytes base = pde & AMD64_VM_ADDR_MASK &
			                  ~((phys_bytes)AMD64_VM_OFFSET_MASK_2MB);
			if (writable && !(pde & AMD64_VM_WRITE))
				return 0;
			offset = linaddr & AMD64_VM_OFFSET_MASK_2MB;
			*bytes = MIN(*bytes, (phys_bytes)AMD64_BIG_PAGE_SIZE - offset);
			return phys_to_kacc(base + offset);
		}

		pt  = pt_vaddr(pde & AMD64_VM_ADDR_MASK);
		pte = pt[AMD64_VM_PT(linaddr)];
		if (!(pte & AMD64_VM_PRESENT))
			return 0;
		if (writable && !(pte & AMD64_VM_WRITE))
			return 0;
		offset = linaddr & (AMD64_PAGE_SIZE - 1);
		*bytes = MIN(*bytes, (phys_bytes)AMD64_PAGE_SIZE - offset);
		return phys_to_kacc((pte & AMD64_VM_ADDR_MASK) + offset);
	}
}


/*===========================================================================*
 *                           check_resumed_caller                            *
 *===========================================================================*/
static int check_resumed_caller(struct proc *caller)
{
	/* Returns the result from VM if caller was resumed, otherwise OK. */
	if (caller && (caller->p_misc_flags & MF_KCALL_RESUME)) {
		assert(caller->p_vmrequest.vmresult != VMSUSPEND);
		return caller->p_vmrequest.vmresult;
	}

	return OK;
}

/*===========================================================================*
 *				lin_lin_copy				     *
 *===========================================================================*/
static int lin_lin_copy(struct proc *srcproc, vir_bytes srclinaddr,
	struct proc *dstproc, vir_bytes dstlinaddr, vir_bytes bytes)
{
	phys_bytes addr;
	proc_nr_t procslot;

	assert(get_cpulocal_var(ptproc));
	assert(get_cpulocal_var(proc_ptr));
	assert(read_cr3() == get_cpulocal_var(ptproc)->p_seg.p_cr3);

	procslot = get_cpulocal_var(ptproc)->p_nr;

	assert(procslot >= 0 && procslot < AMD64_VM_PT_ENTRIES);

	if(srcproc) assert(!RTS_ISSET(srcproc, RTS_SLOT_FREE));
	if(dstproc) assert(!RTS_ISSET(dstproc, RTS_SLOT_FREE));
	assert(!RTS_ISSET(get_cpulocal_var(ptproc), RTS_SLOT_FREE));
	assert(get_cpulocal_var(ptproc)->p_seg.p_cr3_v);
	if(srcproc) assert(!RTS_ISSET(srcproc, RTS_VMINHIBIT));
	if(dstproc) assert(!RTS_ISSET(dstproc, RTS_VMINHIBIT));

	while(bytes > 0) {
		phys_bytes srcptr, dstptr;
		vir_bytes chunk = bytes;
		int changed = 0;

#ifdef CONFIG_SMP
		unsigned cpu = cpuid;

		if (srcproc && GET_BIT(srcproc->p_stale_tlb, cpu)) {
			changed = 1;
			UNSET_BIT(srcproc->p_stale_tlb, cpu);
		}
		if (dstproc && GET_BIT(dstproc->p_stale_tlb, cpu)) {
			changed = 1;
			UNSET_BIT(dstproc->p_stale_tlb, cpu);
		}
#endif

		/* Resolve addresses via identity map.  The destination is resolved
		 * with writable=1 so a present-but-read-only page is also reported
		 * not-present (see createpde): the kernel direct-map alias is always
		 * writable and would otherwise bypass the page's RO protection. */
		srcptr = createpde(srcproc, srclinaddr, &chunk, 0, &changed);
		dstptr = createpde(dstproc, dstlinaddr, &chunk, 1, &changed);
		/* changed is never set to 1 on x86_64; no CR3 reload needed. */

		/* createpde() returns 0 when a process page is not present (e.g. a
		 * demand-zero stack page during an exec frame copy), and for the
		 * destination also when the page is present but read-only.  We MUST
		 * detect that explicitly here and return EFAULT_SRC/DST so
		 * virtual_copy_f() asks VM to fault the page in (and, for a write to a
		 * read-only page, perform copy-on-write or reject it with EFAULT).  We
		 * cannot rely on the PHYS_COPY_CATCH fault below: a not-present page
		 * makes the copy touch virtual address 0, whose fault yields a caught
		 * address of 0 — indistinguishable from "no fault" in the `if(addr)`
		 * test, so the copy would silently succeed writing nowhere and the page
		 * would never be mapped.  And a write through the writable direct-map
		 * alias of a read-only page would not fault at all. */
		if (srcproc && !srcptr) return EFAULT_SRC;
		if (dstproc && !dstptr) return EFAULT_DST;

		/* Check for overflow. */
		if (srcptr + chunk < srcptr) return EFAULT_SRC;
		if (dstptr + chunk < dstptr) return EFAULT_DST;

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
	assert(!RTS_ISSET(get_cpulocal_var(ptproc), RTS_SLOT_FREE));
	assert(get_cpulocal_var(ptproc)->p_seg.p_cr3_v);

	return OK;
}


static u32_t phys_get32(phys_bytes addr)
{
	u32_t v;
	int r;

	if((r=lin_lin_copy(NULL, addr,
		proc_addr(SYSTEM), (phys_bytes) &v, sizeof(v))) != OK) {
		panic("lin_lin_copy for phys_get32 failed: %d",  r);
	}

	return v;
}

/*===========================================================================*
 *                              umap_virtual                                 *
 *===========================================================================*/
phys_bytes umap_virtual(
  register struct proc *rp,		/* pointer to proc table entry for process */
  int seg,				/* T, D, or S segment */
  vir_bytes vir_addr,			/* virtual address in bytes within the seg */
  vir_bytes bytes			/* # of bytes to be copied */
)
{
	phys_bytes phys = 0;

	if(vm_lookup(rp, vir_addr, &phys, NULL) != OK) {
		printf("SYSTEM:umap_virtual: vm_lookup of %s: seg 0x%x: 0x%lx failed\n", rp->p_name, seg, vir_addr);
		phys = 0;
	} else {
		if(phys == 0)
			panic("vm_lookup returned phys: 0x%lx",  phys);
	}

	if(phys == 0) {
		printf("SYSTEM:umap_virtual: lookup failed\n");
		return 0;
	}

	/* Now make sure addresses are contiguous in physical memory
	 * so that the umap makes sense.
	 */
	if(bytes > 0 && vm_lookup_range(rp, vir_addr, NULL, bytes) != bytes) {
		printf("umap_virtual: %s: %lu at 0x%lx (vir 0x%lx) not contiguous\n",
			rp->p_name, bytes, vir_addr, vir_addr);
		return 0;
	}

	assert(phys);

	return phys;
}


/*===========================================================================*
 *                              vm_lookup                                    *
 *===========================================================================*/
/*
 * Return a kernel-accessible virtual pointer for a page-table page located at
 * physical address pa.  We cannot blindly treat pa as a virtual address via
 * the low identity map: loading a user process (pg_map) overwrites the low
 * identity alias of the kernel-image virtual range, and the bootstrap page
 * tables (pg_pml4 etc.) live in the kernel image.  Reach those through the
 * stable high kernel alias (pg_mapkernel); other page tables (free RAM) still
 * use the identity map.
 *
 * TODO: replace the identity-map page-table access with a proper physical
 * direct-map window (cf. i386's phys_get32) so this is robust for all PTs.
 */
int vm_lookup(const struct proc *proc, const vir_bytes virtual,
 phys_bytes *physical, u32_t *ptent)
{
	/* Walk the 4-level page table.  Page-table pages are reached via
	 * pt_vaddr() (kernel-image high alias or identity), not the raw
	 * physical address, because the low identity alias of the kernel image
	 * is clobbered once user processes are mapped in. */
	const u64_t *pml4, *pdpt, *pd, *pt;
	u64_t pml4e, pdpte, pde_v, pte_v;

	assert(proc);
	assert(physical);
	assert(!isemptyp(proc));
	assert(HASPT(proc));

	pml4  = pt_vaddr((phys_bytes) proc->p_seg.p_cr3);
	pml4e = pml4[AMD64_VM_PML4(virtual)];
	if (!(pml4e & AMD64_VM_PRESENT))
		return EFAULT;

	pdpt  = pt_vaddr(pml4e & AMD64_VM_ADDR_MASK);
	pdpte = pdpt[AMD64_VM_PDPT(virtual)];
	if (!(pdpte & AMD64_VM_PRESENT))
		return EFAULT;
	if (pdpte & AMD64_VM_PS) {		/* 1 GB page */
		phys_bytes base = pdpte & AMD64_VM_ADDR_MASK &
		                  ~((phys_bytes)((1ULL << 30) - 1));
		if (ptent) *ptent = (u32_t)pdpte;
		*physical = base + (virtual & ((1ULL << 30) - 1));
		return OK;
	}

	pd    = pt_vaddr(pdpte & AMD64_VM_ADDR_MASK);
	pde_v = pd[AMD64_VM_PD(virtual)];
	if (!(pde_v & AMD64_VM_PRESENT))
		return EFAULT;
	if (pde_v & AMD64_VM_PS) {		/* 2 MB page */
		phys_bytes base = pde_v & AMD64_VM_ADDR_MASK &
		                  ~((phys_bytes)AMD64_VM_OFFSET_MASK_2MB);
		if (ptent) *ptent = (u32_t)pde_v;
		*physical = base + (virtual & AMD64_VM_OFFSET_MASK_2MB);
		return OK;
	}

	pt    = pt_vaddr(pde_v & AMD64_VM_ADDR_MASK);
	pte_v = pt[AMD64_VM_PT(virtual)];
	if (!(pte_v & AMD64_VM_PRESENT))
		return EFAULT;
	if (ptent) *ptent = (u32_t)pte_v;
	*physical = (pte_v & AMD64_VM_ADDR_MASK) + (virtual & (AMD64_PAGE_SIZE - 1));
	return OK;
}

/*===========================================================================*
 *				vm_lookup_range				     *
 *===========================================================================*/
size_t vm_lookup_range(const struct proc *proc, vir_bytes vir_addr,
	phys_bytes *phys_addr, size_t bytes)
{
	phys_bytes phys, next_phys;
	size_t len;

	assert(proc);
	assert(bytes > 0);
	assert(HASPT(proc));

	if (vm_lookup(proc, vir_addr, &phys, NULL) != OK)
		return 0;

	if (phys_addr != NULL)
		*phys_addr = phys;

	len = AMD64_PAGE_SIZE - (vir_addr % AMD64_PAGE_SIZE);
	vir_addr += len;
	next_phys = phys + len;

	while (len < bytes) {
		if (vm_lookup(proc, vir_addr, &phys, NULL) != OK)
			break;

		if (next_phys != phys)
			break;

		len += AMD64_PAGE_SIZE;
		vir_addr += AMD64_PAGE_SIZE;
		next_phys += AMD64_PAGE_SIZE;
	}

	return MIN(bytes, len);
}

/*===========================================================================*
 *				vm_check_range				     *
 *===========================================================================*/
int vm_check_range(struct proc *caller, struct proc *target,
	vir_bytes vir_addr, size_t bytes, int writeflag)
{
	int r;

	if ((caller->p_misc_flags & MF_KCALL_RESUME) &&
			(r = caller->p_vmrequest.vmresult) != OK)
		return r;

	vm_suspend(caller, target, vir_addr, bytes, VMSTYPE_KERNELCALL,
		writeflag);

	return VMSUSPEND;
}

/*===========================================================================*
 *				vm_memset				     *
 *===========================================================================*/
int vm_memset(struct proc *caller, endpoint_t who, phys_bytes ph, int c,
	phys_bytes count)
{
	u32_t pattern;
	struct proc *whoptr = NULL;
	phys_bytes cur_ph = ph;
	phys_bytes left = count;
	phys_bytes ptr, chunk, pfa = 0;
	int new_cr3, r = OK;

	if ((r = check_resumed_caller(caller)) != OK)
		return r;

	if (who != NONE && !(whoptr = endpoint_lookup(who)))
		return ESRCH;

	c &= 0xFF;
	pattern = c | (c << 8) | (c << 16) | (c << 24);

	assert(get_cpulocal_var(ptproc)->p_seg.p_cr3_v);
	assert(!catch_pagefaults);
	catch_pagefaults = 1;

	while (left > 0) {
		new_cr3 = 0;
		chunk = left;
		ptr = createpde(whoptr, cur_ph, &chunk, 1 /*writable*/, &new_cr3);

		/* createpde() returns 0 for a process page that is not present
		 * or present-but-read-only (it is a write).  We MUST detect that
		 * explicitly and ask VM to fault it in: relying on phys_memset()
		 * to fault is unsafe, because a not-present page makes it touch
		 * virtual address 0, whose caught fault address 0 is
		 * indistinguishable from "no fault" — so the memset would silently
		 * succeed writing nowhere.  This is the safememset() analogue of
		 * the lin_lin_copy() not-present handling (e.g. read(2) from
		 * /dev/zero into an unmapped or read-only buffer must EFAULT). */
		if (whoptr && !ptr) {
			vm_suspend(caller, whoptr, ph, count,
				VMSTYPE_KERNELCALL, 1);
			assert(catch_pagefaults);
			catch_pagefaults = 0;
			return VMSUSPEND;
		}

		if ((pfa = phys_memset(ptr, pattern, chunk))) {
			if (whoptr) {
				vm_suspend(caller, whoptr, ph, count,
					VMSTYPE_KERNELCALL, 1);
				assert(catch_pagefaults);
				catch_pagefaults = 0;
				return VMSUSPEND;
			}
			panic("vm_memset: pf %lx addr=%lx len=%lu\n",
				pfa, ptr, chunk);
		}

		cur_ph += chunk;
		left -= chunk;
	}

	assert(get_cpulocal_var(ptproc)->p_seg.p_cr3_v);
	assert(catch_pagefaults);
	catch_pagefaults = 0;

	return OK;
}

/*===========================================================================*
 *				virtual_copy_f				     *
 *===========================================================================*/
int virtual_copy_f(struct proc *caller, struct vir_addr *src_addr,
	struct vir_addr *dst_addr, vir_bytes bytes, int vmcheck)
{
	struct vir_addr *vir_addr[2];
	int i, r;
	struct proc *procs[2];

	assert((vmcheck && caller) || (!vmcheck && !caller));

	if (bytes <= 0) return EDOM;

	vir_addr[_SRC_] = src_addr;
	vir_addr[_DST_] = dst_addr;

	for (i = _SRC_; i <= _DST_; i++) {
		endpoint_t proc_e = vir_addr[i]->proc_nr_e;
		int proc_nr;
		struct proc *p;

		if (proc_e == NONE) {
			p = NULL;
		} else {
			if (!isokendpt(proc_e, &proc_nr)) {
				printf("virtual_copy: no reasonable endpoint\n");
				return ESRCH;
			}
			p = proc_addr(proc_nr);
		}
		procs[i] = p;
	}

	if ((r = check_resumed_caller(caller)) != OK)
		return r;

	if ((r = lin_lin_copy(procs[_SRC_], vir_addr[_SRC_]->offset,
		procs[_DST_], vir_addr[_DST_]->offset, bytes)) != OK) {
		struct proc *target = NULL;
		phys_bytes lin;
		int writeflag;

		if (r != EFAULT_SRC && r != EFAULT_DST)
			panic("lin_lin_copy failed: %d", r);
		if (!vmcheck || !caller)
			return r;

		if (r == EFAULT_SRC) {
			lin = vir_addr[_SRC_]->offset;
			target = procs[_SRC_];
			writeflag = 0;
		} else {
			lin = vir_addr[_DST_]->offset;
			target = procs[_DST_];
			writeflag = 1;
		}

		assert(caller);
		assert(target);

		vm_suspend(caller, target, lin, bytes, VMSTYPE_KERNELCALL, writeflag);
		return VMSUSPEND;
	}

	return OK;
}

/*===========================================================================*
 *				data_copy				     *
 *===========================================================================*/
int data_copy(const endpoint_t from_proc, const vir_bytes from_addr,
	const endpoint_t to_proc, const vir_bytes to_addr, size_t bytes)
{
	struct vir_addr src, dst;

	src.offset = from_addr;
	dst.offset = to_addr;
	src.proc_nr_e = from_proc;
	dst.proc_nr_e = to_proc;
	assert(src.proc_nr_e != NONE);
	assert(dst.proc_nr_e != NONE);

	return virtual_copy(&src, &dst, bytes);
}

/*===========================================================================*
 *			data_copy_vmcheck				     *
 *===========================================================================*/
int data_copy_vmcheck(struct proc *caller,
	const endpoint_t from_proc, const vir_bytes from_addr,
	const endpoint_t to_proc, const vir_bytes to_addr, size_t bytes)
{
	struct vir_addr src, dst;

	src.offset = from_addr;
	dst.offset = to_addr;
	src.proc_nr_e = from_proc;
	dst.proc_nr_e = to_proc;
	assert(src.proc_nr_e != NONE);
	assert(dst.proc_nr_e != NONE);

	return virtual_copy_vmcheck(caller, &src, &dst, bytes);
}

/*===========================================================================*
 *				memory_init				     *
 *===========================================================================*/
void memory_init(void)
{
	/* x86_64: createpde() uses the identity map; freepdes not needed. */
}

/*===========================================================================*
 *				arch_proc_init				     *
 *===========================================================================*/
void arch_proc_init(struct proc *pr, const vir_bytes ip, const vir_bytes sp,
	const vir_bytes ps_str, char *name)
{
	arch_proc_reset(pr);
	strlcpy(pr->p_name, name, sizeof(pr->p_name));

	pr->p_reg.pc = (vir_bytes)ip;
	pr->p_reg.sp = (vir_bytes)sp;
	pr->p_reg.bx = ps_str;		/* ps_strings ptr in rbx */
}

static int oxpcie_mapping_index = -1,
	lapic_mapping_index = -1,
	ioapic_first_index = -1,
	ioapic_last_index = -1,
	video_mem_mapping_index = -1,
	usermapped_glo_index = -1,
	usermapped_index = -1, first_um_idx = -1;

extern char *video_mem;
extern char usermapped_start, usermapped_end, usermapped_nonglo_start;

/*===========================================================================*
 *				arch_phys_map				     *
 *===========================================================================*/
int arch_phys_map(const int index, phys_bytes *addr, phys_bytes *len, int *flags)
{
	static int first = 1;
	int freeidx = 0;
	static char *ser_var = NULL;
	vir_bytes glo_len = (vir_bytes)&usermapped_nonglo_start -
			    (vir_bytes)&usermapped_start;

	if (first) {
		memset(&minix_kerninfo, 0, sizeof(minix_kerninfo));
		video_mem_mapping_index = freeidx++;
		if (glo_len > 0)
			usermapped_glo_index = freeidx++;
		usermapped_index = freeidx++;
		first_um_idx = usermapped_index;
		if (usermapped_glo_index != -1)
			first_um_idx = usermapped_glo_index;

#ifdef USE_APIC
		if (lapic_addr)
			lapic_mapping_index = freeidx++;
		if (ioapic_enabled) {
			ioapic_first_index = freeidx;
			assert(nioapics > 0);
			freeidx += nioapics;
			ioapic_last_index = freeidx - 1;
		}
#endif

#if CONFIG_OXPCIE
		if ((ser_var = env_get("oxpcie"))) {
			if (ser_var[0] != '0' || ser_var[1] != 'x') {
				printf("oxpcie address in hex please\n");
			} else {
				oxpcie_mapping_index = freeidx++;
			}
		}
#endif
		first = 0;
	}

	if (index == usermapped_glo_index) {
		*addr = vir2phys(&usermapped_start);
		*len  = glo_len;
		*flags = VMMF_USER | VMMF_GLO;
		return OK;
	} else if (index == usermapped_index) {
		*addr = vir2phys(&usermapped_nonglo_start);
		*len  = (vir_bytes)&usermapped_end -
			(vir_bytes)&usermapped_nonglo_start;
		*flags = VMMF_USER;
		return OK;
	} else if (index == video_mem_mapping_index) {
		*addr = MULTIBOOT_VIDEO_BUFFER;
		*len  = AMD64_PAGE_SIZE;
		*flags = VMMF_WRITE;
		return OK;
	}

#ifdef USE_APIC
	if (index == lapic_mapping_index) {
		if (!lapic_addr)
			return EINVAL;
		*addr  = lapic_addr;
		*len   = 4 << 10;
		*flags = VMMF_UNCACHED | VMMF_WRITE;
		return OK;
	} else if (ioapic_enabled && index >= ioapic_first_index &&
		   index <= ioapic_last_index) {
		int i = index - ioapic_first_index;
		*addr  = io_apic[i].paddr;
		assert(*addr);
		*len   = 4 << 10;
		*flags = VMMF_UNCACHED | VMMF_WRITE;
		return OK;
	}
#endif

#if CONFIG_OXPCIE
	if (index == oxpcie_mapping_index) {
		*addr  = strtoul(ser_var + 2, NULL, 16);
		*len   = 0x4000;
		*flags = VMMF_UNCACHED | VMMF_WRITE;
		return OK;
	}
#endif

	return EINVAL;
}

/*===========================================================================*
 *			arch_phys_map_reply				     *
 *===========================================================================*/
int arch_phys_map_reply(const int index, const vir_bytes addr)
{
#ifdef USE_APIC
	if (index == lapic_mapping_index && lapic_addr) {
		/* Only remember the vaddr VM mapped the LAPIC at.  The switch to
		 * it (lapic_addr = lapic_addr_vaddr) happens in arch_enable_paging()
		 * once we run on VM-managed page tables — by then the boot identity
		 * mapping of the LAPIC MMIO is gone.  Switching here (with
		 * lapic_addr_vaddr still 0) left lapic_addr = 0, so the timer EOI
		 * wrote to 0x0b0 and faulted. */
		lapic_addr_vaddr = addr;
		return OK;
	} else if (ioapic_enabled && index >= ioapic_first_index &&
		   index <= ioapic_last_index) {
		int i = index - ioapic_first_index;
		io_apic[i].vaddr = addr;
		return OK;
	}
#endif

#if CONFIG_OXPCIE
	if (index == oxpcie_mapping_index) {
		oxpcie_set_vaddr((unsigned char *)addr);
		return OK;
	}
#endif

	if (index == first_um_idx) {
		extern struct minix_ipcvecs minix_ipcvecs_sysenter,
			minix_ipcvecs_syscall, minix_ipcvecs_softint;
		extern vir_bytes usermapped_offset;

		/* On x86_64 the usermapped region is mapped into the user half
		 * (below the kernel's high link address), so addr < usermapped_start;
		 * usermapped_offset wraps (mod 2^64) but FIXEDPTR(ptr) = addr +
		 * (ptr - usermapped_start) still resolves to the correct user
		 * address.  Only require a non-zero user mapping. */
		assert(addr);
		usermapped_offset = addr - (vir_bytes)&usermapped_start;
#define FIXEDPTR(ptr) (void *)((vir_bytes)(ptr) + usermapped_offset)
#define FIXPTR(ptr)   ptr = FIXEDPTR(ptr)
#define ASSIGN(s)     minix_kerninfo.s = FIXEDPTR(&s)
		ASSIGN(kinfo);
		ASSIGN(machine);
		ASSIGN(kmessages);
		ASSIGN(loadinfo);
		ASSIGN(kuserinfo);
		ASSIGN(arm_frclock);
		ASSIGN(kclockinfo);

		/* x86_64: SYSCALL is always available */
		if (minix_feature_flags & MKF_I386_AMD_SYSCALL) {
			DEBUGBASIC(("kernel: selecting amd syscall ipc style\n"));
			minix_kerninfo.minix_ipcvecs = &minix_ipcvecs_syscall;
		} else {
			DEBUGBASIC(("kernel: selecting fallback (int) ipc style\n"));
			minix_kerninfo.minix_ipcvecs = &minix_ipcvecs_softint;
		}

		FIXPTR(minix_kerninfo.minix_ipcvecs->send);
		FIXPTR(minix_kerninfo.minix_ipcvecs->receive);
		FIXPTR(minix_kerninfo.minix_ipcvecs->sendrec);
		FIXPTR(minix_kerninfo.minix_ipcvecs->senda);
		FIXPTR(minix_kerninfo.minix_ipcvecs->sendnb);
		FIXPTR(minix_kerninfo.minix_ipcvecs->notify);
		FIXPTR(minix_kerninfo.minix_ipcvecs->do_kernel_call);
		FIXPTR(minix_kerninfo.minix_ipcvecs);

		minix_kerninfo.kerninfo_magic = KERNINFO_MAGIC;
		minix_kerninfo.minix_feature_flags = minix_feature_flags;
		minix_kerninfo_user = (vir_bytes)FIXEDPTR(&minix_kerninfo);

		if (env_get("libc_ipc")) {
			printf("kernel: forcing in-libc fallback ipc style\n");
			minix_kerninfo.minix_ipcvecs = NULL;
		} else {
			minix_kerninfo.ki_flags |= MINIX_KIF_IPCVECS;
		}

		minix_kerninfo.ki_flags |= MINIX_KIF_USERINFO;
		return OK;
	}

	if (index == usermapped_index)
		return OK;

	if (index == video_mem_mapping_index) {
		video_mem_vaddr = addr;
		return OK;
	}

	return EINVAL;
}

/*===========================================================================*
 *			arch_enable_paging				     *
 *===========================================================================*/
int arch_enable_paging(struct proc *caller)
{
	assert(caller->p_seg.p_cr3);

	switch_address_space(caller);

	video_mem = (char *)video_mem_vaddr;

#ifdef USE_APIC
	/* Reach the LAPIC/IOAPIC MMIO through the kernel physical direct map
	 * (PML4[511], shared into every address space via kern_pml4_hi) rather
	 * than the VM-supplied user-half (PML4[0], VM_DATATOP) mapping.  The
	 * kernel stores to the LAPIC EOI register on every timer tick on every
	 * CPU, in whatever process address space is current at the time; a
	 * user-half mapping is subject to per-process page-table churn and can be
	 * absent — that was the test42/SMP "pagefault in kernel ... address
	 * 0xf00060b0" reboot, where the EOI store on an AP faulted because the
	 * current CR3 lacked the user-half LAPIC page.  The direct-map alias
	 * (phys_to_kacc) is kernel-only and unconditionally present. */
	/* Idempotent: arch_enable_paging() runs once per VMCTL_ENABLE_PAGING, so
	 * derive the alias from the fixed LAPIC physical base — never transform
	 * lapic_addr in place, or a second call would re-alias the already-aliased
	 * value (phys_to_kacc twice overflows bit 39 -> PML4[510], unmapped). */
	if (lapic_addr) {
		lapic_addr = phys_to_kacc(LOCAL_APIC_DEF_ADDR);
		lapic_eoi_addr = LAPIC_EOI;
	}
	if (ioapic_enabled) {
		int i;
		for (i = 0; i < nioapics; i++)
			io_apic[i].addr = phys_to_kacc(io_apic[i].paddr);
	}
#endif

	return OK;
}

/*===========================================================================*
 *			release_address_space				     *
 *===========================================================================*/
void release_address_space(struct proc *pr)
{
	pr->p_seg.p_cr3_v = NULL;
}

/*===========================================================================*
 *		platform_tbl_checksum_ok				     *
 *===========================================================================*/
int platform_tbl_checksum_ok(void *ptr, unsigned int length)
{
	u8_t total = 0;
	unsigned int i;
	for (i = 0; i < length; i++)
		total += ((unsigned char *)ptr)[i];
	return !total;
}

/*===========================================================================*
 *			platform_tbl_ptr				     *
 *===========================================================================*/
int platform_tbl_ptr(phys_bytes start, phys_bytes end, unsigned increment,
	void *buff, unsigned size, phys_bytes *phys_addr,
	int (*cmp_f)(void *))
{
	phys_bytes addr;

	for (addr = start; addr < end; addr += increment) {
		phys_copy(addr, (phys_bytes)buff, size);
		if (cmp_f(buff)) {
			if (phys_addr)
				*phys_addr = addr;
			return 1;
		}
	}
	return 0;
}
