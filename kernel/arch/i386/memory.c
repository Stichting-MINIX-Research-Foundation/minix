
#include "../../kernel.h"
#include "../../proc.h"

#include <minix/type.h>
#include <string.h>

#include <sys/vm.h>

#include <minix/portio.h>

#include "proto.h"

/* VM functions and data. */

PRIVATE int vm_needs_init= 1;
PRIVATE u32_t vm_cr3;

FORWARD _PROTOTYPE( void phys_put32, (phys_bytes addr, u32_t value)	);
FORWARD _PROTOTYPE( u32_t phys_get32, (phys_bytes addr)			);
FORWARD _PROTOTYPE( void vm_set_cr3, (u32_t value)			);
FORWARD _PROTOTYPE( void set_cr3, (void)				);
FORWARD _PROTOTYPE( void vm_enable_paging, (void)			);

/* *** Internal VM Functions *** */

PUBLIC void vm_init(void)
{
	int o;
	phys_bytes p, pt_size;
	phys_bytes vm_dir_base, vm_pt_base, phys_mem;
	u32_t entry;
	unsigned pages;

	if (!vm_size)
		panic("i386_vm_init: no space for page tables", NO_NUM);

	/* Align page directory */
	o= (vm_base % PAGE_SIZE);
	if (o != 0)
		o= PAGE_SIZE-o;
	vm_dir_base= vm_base+o;

	/* Page tables start after the page directory */
	vm_pt_base= vm_dir_base+PAGE_SIZE;

	pt_size= (vm_base+vm_size)-vm_pt_base;
	pt_size -= (pt_size % PAGE_SIZE);

	/* Compute the number of pages based on vm_mem_high */
	pages= (vm_mem_high-1)/PAGE_SIZE + 1;

	if (pages * I386_VM_PT_ENT_SIZE > pt_size)
		panic("i386_vm_init: page table too small", NO_NUM);

	for (p= 0; p*I386_VM_PT_ENT_SIZE < pt_size; p++)
	{
		phys_mem= p*PAGE_SIZE;
		entry= phys_mem | I386_VM_USER | I386_VM_WRITE |
			I386_VM_PRESENT;
		if (phys_mem >= vm_mem_high)
			entry= 0;
		phys_put32(vm_pt_base + p*I386_VM_PT_ENT_SIZE, entry);
	}

	for (p= 0; p < I386_VM_DIR_ENTRIES; p++)
	{
		phys_mem= vm_pt_base + p*PAGE_SIZE;
		entry= phys_mem | I386_VM_USER | I386_VM_WRITE |
			I386_VM_PRESENT;
		if (phys_mem >= vm_pt_base + pt_size)
			entry= 0;
		phys_put32(vm_dir_base + p*I386_VM_PT_ENT_SIZE, entry);
	}
	vm_set_cr3(vm_dir_base);
	level0(vm_enable_paging);
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

PUBLIC void vm_map_range(base, size, offset)
u32_t base;
u32_t size;
u32_t offset;
{
	u32_t curr_pt, curr_pt_addr, entry;
	int dir_ent, pt_ent;

	if (base % PAGE_SIZE != 0)
		panic("map_range: bad base", base);
	if (size % PAGE_SIZE != 0)
		panic("map_range: bad size", size);
	if (offset % PAGE_SIZE != 0)
		panic("map_range: bad offset", offset);

	curr_pt= -1;
	curr_pt_addr= 0;
	while (size != 0)
	{
		dir_ent= (base >> I386_VM_DIR_ENT_SHIFT);
		pt_ent= (base >> I386_VM_PT_ENT_SHIFT) & I386_VM_PT_ENT_MASK;
		if (dir_ent != curr_pt)
		{
			/* Get address of page table */
			curr_pt= dir_ent;
			curr_pt_addr= phys_get32(vm_cr3 +
				dir_ent * I386_VM_PT_ENT_SIZE);
			curr_pt_addr &= I386_VM_ADDR_MASK;
		}
		entry= offset | I386_VM_USER | I386_VM_WRITE |
			I386_VM_PRESENT;
#if 0	/* Do we need this for memory mapped I/O? */
		entry |= I386_VM_PCD | I386_VM_PWT;
#endif
		phys_put32(curr_pt_addr + pt_ent * I386_VM_PT_ENT_SIZE, entry);
		offset += PAGE_SIZE;
		base += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	/* reload root of page table. */
	vm_set_cr3(vm_cr3);
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

