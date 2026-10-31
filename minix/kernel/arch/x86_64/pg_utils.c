
#include <minix/cpufeature.h>

#include <assert.h>
#include "kernel/kernel.h"
#include "arch_proto.h"

#include <string.h>

/* These are set/computed in kernel.lds. */
extern char _kern_vir_base, _kern_phys_base, _kern_size;

/* Retrieve the absolute values to something we can use. */
static vir_bytes kern_vir_start   = (vir_bytes)  &_kern_vir_base;
static phys_bytes kern_phys_start = (phys_bytes) &_kern_phys_base;
static phys_bytes kern_kernlen    = (phys_bytes) &_kern_size;

/* Page-table entry flags */
#define PG_PRESENT   (1ULL << 0)
#define PG_WRITE     (1ULL << 1)
#define PG_USER      (1ULL << 2)
#define PG_PWT       (1ULL << 3)
#define PG_PCD       (1ULL << 4)
#define PG_PS        (1ULL << 7)   /* 2MB page in PDE */
#define PG_PHYS_MASK (0x000FFFFFFFFFF000ULL)

/* Virtual-address field extractors */
#define PML4_INDEX(v)  (((vir_bytes)(v) >> 39) & 0x1FFU)
#define PDPT_INDEX(v)  (((vir_bytes)(v) >> 30) & 0x1FFU)
#define PD_INDEX(v)    (((vir_bytes)(v) >> 21) & 0x1FFU)
#define PT_INDEX(v)    (((vir_bytes)(v) >> 12) & 0x1FFU)

#define PAGE_2MB  (2UL * 1024 * 1024)
#define PAGE_4KB  (4096UL)

/* Standard local-APIC / IOAPIC MMIO window (IOAPIC 0xFEC00000, LAPIC
 * 0xFEE00000).  These 2MB direct-map/identity leaves must be uncacheable even
 * on machines whose RAM extends past the hole (mem_high_phys > 4GB), so the
 * kernel's direct-map alias of the LAPIC (used for the timer EOI, see
 * arch_enable_paging) is not cached. */
#define PG_APIC_MMIO(phys) \
	((phys) >= 0xFEC00000ULL && (phys) < 0xFF000000ULL)

/*
 * Static 4-level page-table storage.
 * Compiled twice: as an unpaged object (physical addresses) and as a
 * regular kernel object (high-virtual addresses).  The unpaged version
 * is used by pre_init(); the regular version is rebuilt by prot_init().
 */
#define PG_IDENT_PD_MAX 64  /* covers 64 GB of identity-mapped physical RAM */

static u64_t pg_pml4[512]                    __aligned(4096);
static u64_t pg_pdpt_low[512]                __aligned(4096);
static u64_t pg_pdpt_high[512]               __aligned(4096);
static u64_t pg_pd_ident[PG_IDENT_PD_MAX][512] __aligned(4096);
static u64_t pg_pd_kern[512]                 __aligned(4096);
/* Direct-map PDs: map physical RAM at DM_BASE under PML4[511] (the kernel high
 * PDPT, pg_pdpt_high, shared by every address space via kern_pml4_hi).  This is
 * how the kernel reaches arbitrary physical memory (page-table walks, phys
 * copies) while running on any process's CR3 — the low identity map exists only
 * in the bootstrap pg_pml4, not in VM-built per-process tables.  Separate from
 * pg_pd_ident because pg_map() splits those when loading boot processes. */
static u64_t pg_pd_dm[PG_IDENT_PD_MAX][512]  __aligned(4096);
/* PML4[511] base, PDPT[0]: must match PHYS_DIRECTMAP_BASE in memory.c. */
#define PG_DIRECTMAP_BASE  0xFFFFFF8000000000ULL

/* Page tables (level 1) for 4KB-page mappings used by pg_map() */
#define PG_PAGETABLES 32
static u64_t pagetables[PG_PAGETABLES][512]  __aligned(4096);
static int   pt_inuse = 0;

void print_memmap(kinfo_t *cbi)
{
        int m;
        assert(cbi->mmap_size < MAXMEMMAP);
        for (m = 0; m < cbi->mmap_size; m++) {
		phys_bytes addr  = cbi->memmap[m].mm_base_addr;
		phys_bytes endit = addr + cbi->memmap[m].mm_length;
                printf("%08lx-%08lx ", (unsigned long)addr, (unsigned long)endit);
        }
        printf("\nsize %08lx\n", (unsigned long)cbi->mmap_size);
}

void cut_memmap(kinfo_t *cbi, phys_bytes start, phys_bytes end)
{
        int m;
        phys_bytes o;

        if ((o = start % PAGE_4KB))
                start -= o;
        if ((o = end % PAGE_4KB))
                end += PAGE_4KB - o;

	assert(kernel_may_alloc);

        for (m = 0; m < cbi->mmap_size; m++) {
                phys_bytes substart = start, subend = end;
                phys_bytes memaddr  = cbi->memmap[m].mm_base_addr;
                phys_bytes memend   = memaddr + cbi->memmap[m].mm_length;

                if (substart < memaddr) substart = memaddr;
                if (subend   > memend)  subend   = memend;
                if (substart >= subend) continue;

                cbi->memmap[m].mm_base_addr = cbi->memmap[m].mm_length = 0;
                if (substart > memaddr)
                        add_memmap(cbi, memaddr, substart - memaddr);
                if (subend < memend)
                        add_memmap(cbi, subend, memend - subend);
        }
}

phys_bytes alloc_lowest(kinfo_t *cbi, phys_bytes len)
{
	int m;
	phys_bytes lowest = (phys_bytes)-1ULL;
	assert(len > 0);
	len = roundup(len, PAGE_4KB);

	assert(kernel_may_alloc);

	for (m = 0; m < cbi->mmap_size; m++) {
		if (cbi->memmap[m].mm_length < len) continue;
		if (cbi->memmap[m].mm_base_addr < lowest)
			lowest = cbi->memmap[m].mm_base_addr;
	}
	assert(lowest != (phys_bytes)-1ULL);
	if (lowest == 0) lowest = 0x8000;
	cut_memmap(cbi, lowest, lowest + len);
	cbi->kernel_allocated_bytes_dynamic += len;
	return lowest;
}

void add_memmap(kinfo_t *cbi, u64_t addr, u64_t len)
{
        int m;

        assert(cbi->mmap_size < MAXMEMMAP);
        if (len == 0) return;
	addr = roundup(addr, PAGE_4KB);
	len  = rounddown(len, PAGE_4KB);
	if (len == 0) return;

	assert(kernel_may_alloc);

        for (m = 0; m < MAXMEMMAP; m++) {
		phys_bytes highmark;
                if (cbi->memmap[m].mm_length) continue;
                cbi->memmap[m].mm_base_addr = addr;
                cbi->memmap[m].mm_length    = len;
                cbi->memmap[m].mm_type      = MULTIBOOT_MEMORY_AVAILABLE;
                if (m >= cbi->mmap_size)
                        cbi->mmap_size = m + 1;
		highmark = addr + len;
		if (highmark > cbi->mem_high_phys)
			cbi->mem_high_phys = highmark;
                return;
        }

        panic("no available memmap slot");
}

static u64_t *alloc_pagetable(phys_bytes *ph)
{
	u64_t *ret;
	if (pt_inuse >= PG_PAGETABLES) panic("no more pagetables");
	assert(sizeof(pagetables[pt_inuse]) == PAGE_4KB);
	ret = pagetables[pt_inuse++];
	*ph = vir2phys(ret);
	return ret;
}

static phys_bytes pg_alloc_page(kinfo_t *cbi)
{
	int m;
	kinfo_memory_map_t *mmap;

	assert(kernel_may_alloc);

	for (m = cbi->mmap_size - 1; m >= 0; m--) {
		mmap = &cbi->memmap[m];
		if (!mmap->mm_length) continue;
		assert(mmap->mm_length > 0);
		assert(!(mmap->mm_length   % PAGE_4KB));
		assert(!(mmap->mm_base_addr % PAGE_4KB));

		mmap->mm_length -= PAGE_4KB;
		cbi->kernel_allocated_bytes_dynamic += PAGE_4KB;
		return mmap->mm_base_addr + mmap->mm_length;
	}

	panic("can't find free memory");
}

void pg_clear(void)
{
	memset(pg_pml4,      0, sizeof(pg_pml4));
	memset(pg_pdpt_low,  0, sizeof(pg_pdpt_low));
	memset(pg_pdpt_high, 0, sizeof(pg_pdpt_high));
	memset(pg_pd_ident,  0, sizeof(pg_pd_ident));
	memset(pg_pd_kern,   0, sizeof(pg_pd_kern));
}

void pg_identity(kinfo_t *cbi)
{
	int i, j, num_pds;
	u64_t phys = 0;

	assert(cbi->mem_high_phys);

	/* Round up to the next GB boundary, capped at PG_IDENT_PD_MAX GBs */
	num_pds = (int)((cbi->mem_high_phys + (u64_t)(512 * PAGE_2MB) - 1)
		  / (512 * PAGE_2MB));
	/* Always map at least the low 4 GB so below-4GB MMIO (the Local APIC at
	 * 0xFEE00000, the IOAPIC at 0xFEC00000, video memory, etc.) is reachable
	 * before VM is up.  Pages above mem_high_phys are mapped uncacheable. */
	if (num_pds < 4) num_pds = 4;
	if (num_pds > PG_IDENT_PD_MAX) num_pds = PG_IDENT_PD_MAX;

	/* PML4[0] → pg_pdpt_low.  PG_USER on the upper levels (PML4E/PDPTE) is
	 * required so that user processes loaded in the low half (pg_map sets
	 * PG_USER on the PD/PT it creates) are reachable: the effective privilege
	 * is the AND of every walk level.  The 2MB identity leaves below stay
	 * supervisor (no PG_USER), so kernel memory is not exposed to userspace. */
	pg_pml4[0] = vir2phys(pg_pdpt_low) | PG_PRESENT | PG_WRITE | PG_USER;

	for (i = 0; i < num_pds; i++) {
		/* pdpt_low[i] → pg_pd_ident[i] */
		pg_pdpt_low[i] = vir2phys(pg_pd_ident[i]) |
			PG_PRESENT | PG_WRITE | PG_USER;

		for (j = 0; j < 512; j++) {
			u64_t flags = PG_PRESENT | PG_WRITE | PG_PS;
			if (phys >= cbi->mem_high_phys || PG_APIC_MMIO(phys))
				flags |= PG_PWT | PG_PCD;
			pg_pd_ident[i][j] = phys | flags;
			phys += PAGE_2MB;
		}
	}

	/*
	 * Physical direct map at DM_BASE (PML4[511], PDPT[0..num_pds-1]).  Lives
	 * in pg_pdpt_high, which pg_mapkernel() wires to PML4[511]; that entry is
	 * shared into every process via kern_pml4_hi, so the kernel can reach any
	 * physical address through DM_BASE+pa on any CR3.  Uses its own PD pages
	 * (pg_pd_dm), never split by pg_map().  Kernel-only (no PG_USER).
	 */
	phys = 0;
	for (i = 0; i < num_pds; i++) {
		pg_pdpt_high[i] = vir2phys(pg_pd_dm[i]) | PG_PRESENT | PG_WRITE;
		for (j = 0; j < 512; j++) {
			u64_t flags = PG_PRESENT | PG_WRITE | PG_PS;
			if (phys >= cbi->mem_high_phys || PG_APIC_MMIO(phys))
				flags |= PG_PWT | PG_PCD;
			pg_pd_dm[i][j] = phys | flags;
			phys += PAGE_2MB;
		}
	}
}

int pg_mapkernel(void)
{
	u64_t phys = kern_phys_start;
	u64_t virt = kern_vir_start;
	int pde    = PD_INDEX(virt);
	int mapped = 0;

	assert(!(virt % PAGE_2MB));
	assert(!(phys % PAGE_2MB));

	/* PML4[511] → pg_pdpt_high */
	pg_pml4[PML4_INDEX(virt)] = vir2phys(pg_pdpt_high) | PG_PRESENT | PG_WRITE;

	/* pg_pdpt_high[510] → pg_pd_kern */
	pg_pdpt_high[PDPT_INDEX(virt)] = vir2phys(pg_pd_kern) | PG_PRESENT | PG_WRITE;

	while (mapped < (int)kern_kernlen) {
		pg_pd_kern[pde] = phys | PG_PRESENT | PG_WRITE | PG_PS;
		mapped += PAGE_2MB;
		phys   += PAGE_2MB;
		pde++;
	}

	return pde;	/* first free PD entry after kernel */
}

phys_bytes pg_load(void)
{
	phys_bytes phpml4 = vir2phys(pg_pml4);
	write_cr3(phpml4);
	return phpml4;
}

void pg_info(reg_t *pml4_ph, u64_t **pml4_v)
{
	*pml4_ph = vir2phys(pg_pml4);
	*pml4_v  = pg_pml4;
}

void vm_enable_paging(void)
{
	reg_t cr0, cr4;

	cr0 = read_cr0();
	cr4 = read_cr4();

	/* x86_64 long mode: PAE and paging are already active from head.S.
	 * Enable write-protect and, if supported, global pages.
	 */
	cr0 |= I386_CR0_WP;
	write_cr0(cr0);

	if (_cpufeature(_CPUF_I386_PGE))
		cr4 |= I386_CR4_PGE;
	write_cr4(cr4);
}

phys_bytes pg_rounddown(phys_bytes b)
{
	phys_bytes o;
	if (!(o = b % PAGE_4KB))
		return b;
	return b - o;
}

phys_bytes pg_roundup(phys_bytes b)
{
	phys_bytes o;
	if (!(o = b % PAGE_4KB))
		return b;
	return b - o + PAGE_4KB;
}

void pg_map(phys_bytes phys, vir_bytes vaddr, vir_bytes vaddr_end,
	kinfo_t *cbi)
{
	/* Map 4KB pages; only for user-space addresses (< kern_vir_start). */

	if (phys == PG_ALLOCATEME) {
		assert(!(vaddr % PAGE_4KB));
	} else {
		assert((vaddr % PAGE_4KB) == (phys % PAGE_4KB));
		vaddr = pg_rounddown(vaddr);
		phys  = pg_rounddown(phys);
	}
	assert(vaddr < kern_vir_start);

	while (vaddr < vaddr_end) {
		phys_bytes source = phys;
		int pdpt_i = PDPT_INDEX(vaddr);
		int pd_i   = PD_INDEX(vaddr);
		int pt_i   = PT_INDEX(vaddr);
		u64_t *pd, *pt;
		phys_bytes ph;

		if (phys == PG_ALLOCATEME)
			source = pg_alloc_page(cbi);

		assert(PML4_INDEX(vaddr) == 0);	/* only low canonical half */
		assert(pdpt_i < PG_IDENT_PD_MAX);

		pd = pg_pd_ident[pdpt_i];

		/* If this 2MB slot hasn't been split into a PT yet, do it now */
		if (!(pd[pd_i] & PG_PRESENT) || (pd[pd_i] & PG_PS)) {
			pt = alloc_pagetable(&ph);
			pd[pd_i] = ph | PG_PRESENT | PG_WRITE | PG_USER;
		} else {
			/*
			 * PT already allocated; recover its KERNEL-virtual
			 * address.  Page tables come from the kernel-image
			 * pagetables[] pool, so convert phys->virt via the kernel
			 * offset.  Do NOT use the low identity alias: mapping a
			 * user vaddr in 0x400000-0x600000 splits the 2MB identity
			 * page that covers the kernel image, unmapping the pool's
			 * identity address and faulting on the next PT write.
			 */
			pt = (u64_t *)(kern_vir_start +
			    ((pd[pd_i] & PG_PHYS_MASK) - kern_phys_start));
		}

		pt[pt_i] = (source & PG_PHYS_MASK) | PG_PRESENT | PG_WRITE | PG_USER;

		vaddr += PAGE_4KB;
		if (phys != PG_ALLOCATEME)
			phys += PAGE_4KB;
	}
}
