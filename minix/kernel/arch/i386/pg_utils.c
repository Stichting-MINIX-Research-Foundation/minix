#include <minix/cpufeature.h> // Kept

// #include <assert.h> // Replaced
#include "kernel/kernel.h"
#include "arch_proto.h"

// #include <string.h> // Replaced

// Added kernel headers
#include <minix/kernel_types.h> // For fixed-width types if not from minix_types.h
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/* These are set/computed in kernel.lds. */
extern char _kern_vir_base, _kern_phys_base, _kern_size;

/* Retrieve the absolute values to something we can use. */
static phys_bytes kern_vir_start = (phys_bytes) &_kern_vir_base;
static phys_bytes kern_phys_start = (phys_bytes) &_kern_phys_base;
static phys_bytes kern_kernlen = (phys_bytes) &_kern_size;

/* page directory we can use to map things */
static u32_t pagedir[1024]  __aligned(4096); // u32_t might be undefined

void print_memmap(kinfo_t *cbi)
{
        int m;
        KASSERT_PLACEHOLDER(cbi->mmap_size < MAXMEMMAP); // MODIFIED
        for(m = 0; m < cbi->mmap_size; m++) {
		phys_bytes addr = cbi->memmap[m].mm_base_addr, endit = cbi->memmap[m].mm_base_addr + cbi->memmap[m].mm_length;
                kprintf_stub("%08lx-%08lx ",addr, endit); // MODIFIED
        }
        kprintf_stub("\nsize %08lx\n", cbi->mmap_size); // MODIFIED
}

void cut_memmap(kinfo_t *cbi, phys_bytes start, phys_bytes end)
{
        int m;
        phys_bytes o;

        if((o=start % I386_PAGE_SIZE))
                start -= o;
        if((o=end % I386_PAGE_SIZE))
                end += I386_PAGE_SIZE - o;

	KASSERT_PLACEHOLDER(kernel_may_alloc); // MODIFIED

        for(m = 0; m < cbi->mmap_size; m++) {
                phys_bytes substart = start, subend = end;
                phys_bytes memaddr = cbi->memmap[m].mm_base_addr,
                        memend = cbi->memmap[m].mm_base_addr + cbi->memmap[m].mm_length;

                /* adjust cut range to be a subset of the free memory */
                if(substart < memaddr) substart = memaddr;
                if(subend > memend) subend = memend;
                if(substart >= subend) continue;

                /* if there is any overlap, forget this one and add
                 * 1-2 subranges back
                 */
                cbi->memmap[m].mm_base_addr = cbi->memmap[m].mm_length = 0;
                if(substart > memaddr)
                        add_memmap(cbi, memaddr, substart-memaddr);
                if(subend < memend)
                        add_memmap(cbi, subend, memend-subend);
        }
}

phys_bytes alloc_lowest(kinfo_t *cbi, phys_bytes len)
{
	/* Allocate the lowest physical page we have. */
	int m;
#define EMPTY 0xffffffff
	phys_bytes lowest = EMPTY;
	KASSERT_PLACEHOLDER(len > 0); // MODIFIED
	len = roundup(len, I386_PAGE_SIZE);

	KASSERT_PLACEHOLDER(kernel_may_alloc); // MODIFIED

	for(m = 0; m < cbi->mmap_size; m++) {
		if(cbi->memmap[m].mm_length < len) continue;
		if(cbi->memmap[m].mm_base_addr < lowest) lowest = cbi->memmap[m].mm_base_addr;
	}
	KASSERT_PLACEHOLDER(lowest != EMPTY); // MODIFIED
	cut_memmap(cbi, lowest, len);
	cbi->kernel_allocated_bytes_dynamic += len;
	return lowest;
}

void add_memmap(kinfo_t *cbi, u64_t addr, u64_t len)
{
        int m;
#define LIMIT 0xFFFFF000
        /* Truncate available memory at 4GB as the rest of minix
         * currently can't deal with any bigger.
         */
        if(addr > LIMIT) return;
        if(addr + len > LIMIT) {
                len -= (addr + len - LIMIT);
        }
        KASSERT_PLACEHOLDER(cbi->mmap_size < MAXMEMMAP); // MODIFIED
        if(len == 0) return;
	addr = roundup(addr, I386_PAGE_SIZE);
	len = rounddown(len, I386_PAGE_SIZE);

	KASSERT_PLACEHOLDER(kernel_may_alloc); // MODIFIED

        for(m = 0; m < MAXMEMMAP; m++) {
		phys_bytes highmark;
                if(cbi->memmap[m].mm_length) continue;
                cbi->memmap[m].mm_base_addr = addr;
                cbi->memmap[m].mm_length = len;
                cbi->memmap[m].mm_type = MULTIBOOT_MEMORY_AVAILABLE;
                if(m >= cbi->mmap_size)
                        cbi->mmap_size = m+1;
		highmark = addr + len;
		if(highmark > cbi->mem_high_phys) {
			cbi->mem_high_phys = highmark;
		}

                return;
        }

        panic("no available memmap slot");
}

u32_t *alloc_pagetable(phys_bytes *ph) // u32_t might be undefined
{
	u32_t *ret; // u32_t might be undefined
#define PG_PAGETABLES 6
	static u32_t pagetables[PG_PAGETABLES][1024]  __aligned(4096); // u32_t might be undefined
	static int pt_inuse = 0;
	if(pt_inuse >= PG_PAGETABLES) panic("no more pagetables");
	KASSERT_PLACEHOLDER(sizeof(pagetables[pt_inuse]) == I386_PAGE_SIZE); // MODIFIED
	ret = pagetables[pt_inuse++];
	*ph = vir2phys(ret);
	return ret;
}

#define PAGE_KB (I386_PAGE_SIZE / 1024)

phys_bytes pg_alloc_page(kinfo_t *cbi)
{
	int m;
	multiboot_memory_map_t *mmap;

	KASSERT_PLACEHOLDER(kernel_may_alloc); // MODIFIED

	for(m = cbi->mmap_size-1; m >= 0; m--) {
		mmap = &cbi->memmap[m];
		if(!mmap->mm_length) continue;
		KASSERT_PLACEHOLDER(mmap->mm_length > 0); // MODIFIED
		KASSERT_PLACEHOLDER(!(mmap->mm_length % I386_PAGE_SIZE)); // MODIFIED
		KASSERT_PLACEHOLDER(!(mmap->mm_base_addr % I386_PAGE_SIZE)); // MODIFIED

		mmap->mm_length -= I386_PAGE_SIZE;

                cbi->kernel_allocated_bytes_dynamic += I386_PAGE_SIZE;

		return mmap->mm_base_addr + mmap->mm_length;
	}

	panic("can't find free memory");
}

void pg_identity(kinfo_t *cbi)
{
	uint32_t i; // uint32_t might be undefined
	phys_bytes phys;

	/* We map memory that does not correspond to physical memory
	 * as non-cacheable. Make sure we know what it is.
	 */
	KASSERT_PLACEHOLDER(cbi->mem_high_phys); // MODIFIED

        /* Set up an identity mapping page directory */
        for(i = 0; i < I386_VM_DIR_ENTRIES; i++) {
		u32_t flags = I386_VM_PRESENT | I386_VM_BIGPAGE // u32_t might be undefined
			| I386_VM_USER
			| I386_VM_WRITE;
                phys = i * I386_BIG_PAGE_SIZE;
		if((cbi->mem_high_phys & I386_VM_ADDR_MASK_4MB)
			<= (phys & I386_VM_ADDR_MASK_4MB)) {
			flags |= I386_VM_PWT | I386_VM_PCD;
		}
                pagedir[i] =  phys | flags;
        }
}

int pg_mapkernel(void)
{
	int pde;
	u32_t mapped = 0, kern_phys = kern_phys_start; // u32_t might be undefined

        KASSERT_PLACEHOLDER(!(kern_vir_start % I386_BIG_PAGE_SIZE)); // MODIFIED
        KASSERT_PLACEHOLDER(!(kern_phys % I386_BIG_PAGE_SIZE)); // MODIFIED
        pde = kern_vir_start / I386_BIG_PAGE_SIZE; /* start pde */
	while(mapped < kern_kernlen) {
	        pagedir[pde] = kern_phys | I386_VM_PRESENT | 
			I386_VM_BIGPAGE | I386_VM_WRITE;
		mapped += I386_BIG_PAGE_SIZE;
		kern_phys += I386_BIG_PAGE_SIZE;
		pde++;
	}
	return pde;	/* free pde */
}

void vm_enable_paging(void)
{
        u32_t cr0, cr4; // u32_t might be undefined
        int pgeok;

        pgeok = _cpufeature(_CPUF_I386_PGE);

#ifdef PAE
	if(_cpufeature(_CPUF_I386_PAE) == 0)
		panic("kernel built with PAE support, CPU seems to lack PAE support?\n");
#endif

        cr0= read_cr0();
        cr4= read_cr4();

	/* The boot loader should have put us in protected mode. */
	KASSERT_PLACEHOLDER(cr0 & I386_CR0_PE); // MODIFIED

        /* First clear PG and PGE flag, as PGE must be enabled after PG. */
        write_cr0(cr0 & ~I386_CR0_PG);
        write_cr4(cr4 & ~(I386_CR4_PGE | I386_CR4_PSE));

        cr0= read_cr0();
        cr4= read_cr4();

        /* Our page table contains 4MB entries. */
        cr4 |= I386_CR4_PSE;

        write_cr4(cr4);

        /* First enable paging, then enable global page flag. */
        cr0 |= I386_CR0_PG;
        write_cr0(cr0);
        cr0 |= I386_CR0_WP;
        write_cr0(cr0);

        /* May we enable these features? */
        if(pgeok)
                cr4 |= I386_CR4_PGE;

        write_cr4(cr4);
}

phys_bytes pg_load(void)
{
	phys_bytes phpagedir = vir2phys(pagedir);
        write_cr3(phpagedir);
	return phpagedir;
}

void pg_clear(void)
{
	kmemset(pagedir, 0, sizeof(pagedir)); // MODIFIED
}

phys_bytes pg_rounddown(phys_bytes b)
{
	phys_bytes o;
	if(!(o = b % I386_PAGE_SIZE))
		return b;
	return b  - o;
}

void pg_map(phys_bytes phys, vir_bytes vaddr, vir_bytes vaddr_end,
	kinfo_t *cbi)
{
	static int mapped_pde = -1;
	static u32_t *pt = NULL; // u32_t might be undefined, NULL might be undefined
	int pde, pte;

	KASSERT_PLACEHOLDER(kernel_may_alloc); // MODIFIED

	if(phys == PG_ALLOCATEME) {
		KASSERT_PLACEHOLDER(!(vaddr % I386_PAGE_SIZE)); // MODIFIED
	} else  {
		KASSERT_PLACEHOLDER((vaddr % I386_PAGE_SIZE) == (phys % I386_PAGE_SIZE)); // MODIFIED
		vaddr = pg_rounddown(vaddr);
		phys = pg_rounddown(phys);
	}	
	KASSERT_PLACEHOLDER(vaddr < kern_vir_start); // MODIFIED

	while(vaddr < vaddr_end) {
		phys_bytes source = phys;
		KASSERT_PLACEHOLDER(!(vaddr % I386_PAGE_SIZE)); // MODIFIED
		if(phys == PG_ALLOCATEME) {
			source = pg_alloc_page(cbi);
		} else {
			KASSERT_PLACEHOLDER(!(phys % I386_PAGE_SIZE)); // MODIFIED
		}
		KASSERT_PLACEHOLDER(!(source % I386_PAGE_SIZE)); // MODIFIED
		pde = I386_VM_PDE(vaddr);
		pte = I386_VM_PTE(vaddr);
		if(mapped_pde < pde) {
			phys_bytes ph;
			pt = alloc_pagetable(&ph);
			pagedir[pde] = (ph & I386_VM_ADDR_MASK)
		                | I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE;
			mapped_pde = pde;
		}
		KASSERT_PLACEHOLDER(pt); // MODIFIED
		pt[pte] = (source & I386_VM_ADDR_MASK) |
			I386_VM_PRESENT | I386_VM_USER | I386_VM_WRITE;
		vaddr += I386_PAGE_SIZE;
		if(phys != PG_ALLOCATEME)
			phys += I386_PAGE_SIZE;
	}
}

void pg_info(reg_t *pagedir_ph, u32_t **pagedir_v) // u32_t might be undefined
{
	*pagedir_ph = vir2phys(pagedir);
	*pagedir_v = pagedir;
}
