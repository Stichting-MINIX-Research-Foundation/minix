
#define _SYSTEM 1

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
#include <minix/const.h>
#include <minix/bitmap.h>

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <env.h>
#include <pagetable.h>
#include <sys/param.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "vm.h"
#include "region.h"
#include "sanitycheck.h"

#include "memory.h"

/*===========================================================================*
 *				find_kernel_top				     *
 *===========================================================================*/
phys_bytes find_kernel_top(void)
{
/* Find out where the kernel is, so we know where to start mapping
 * user processes.
 */
	u32_t kernel_top = 0;
#define MEMTOP(v, i) \
  (vmproc[v].vm_arch.vm_seg[i].mem_phys + vmproc[v].vm_arch.vm_seg[i].mem_len)
	assert(vmproc[VMP_SYSTEM].vm_flags & VMF_INUSE);
	kernel_top = MEMTOP(VMP_SYSTEM, T);
	kernel_top = MAX(kernel_top, MEMTOP(VMP_SYSTEM, D));
	kernel_top = MAX(kernel_top, MEMTOP(VMP_SYSTEM, S));
	assert(kernel_top);

	return CLICK2ABS(kernel_top);
}

void regular_segs(struct vmproc *vmp)
{
        int s;
        memset(vmp->vm_arch.vm_seg, 0, sizeof(vmp->vm_arch.vm_seg));
        vmp->vm_arch.vm_seg[T].mem_phys =
	        vmp->vm_arch.vm_seg[D].mem_phys = ABS2CLICK(VM_PROCSTART);
        vmp->vm_arch.vm_seg[T].mem_len =
	        vmp->vm_arch.vm_seg[D].mem_len =
	        vmp->vm_arch.vm_seg[S].mem_len = ABS2CLICK(VM_DATATOP-VM_PROCSTART);
        if((s=sys_newmap(vmp->vm_endpoint, vmp->vm_arch.vm_seg)) != OK)
                panic("regular_segs: sys_newmap failed: %d", s);
        if((s=pt_bind(&vmp->vm_pt, vmp)) != OK)
                panic("regular_segs: pt_bind failed: %d", s);
}

/*===========================================================================*
 *				proc_new				     *
 *===========================================================================*/
int proc_new(struct vmproc *vmp,
  phys_bytes vstart,	  /* where to start the process in page table */
  phys_bytes text_addr,   /* address at which to load code */
  phys_bytes text_bytes,  /* how much code, in bytes but page aligned */
  phys_bytes data_addr,   /* address at which to load data */
  phys_bytes data_bytes,  /* how much data + bss, in bytes but page aligned */
  phys_bytes stack_bytes, /* stack space to reserve, in bytes, page aligned */
  phys_bytes gap_bytes,   /* gap bytes, page aligned */
  phys_bytes text_start,  /* text starts here, if preallocated, otherwise 0 */
  phys_bytes data_start,  /* data starts here, if preallocated, otherwise 0 */
  phys_bytes stacktop,
  int prealloc_stack,
  int is_elf,
  int full_memview
)
{
	int s;
	vir_bytes hole_bytes;
	struct vir_region *reg;
	phys_bytes map_text_addr, map_data_addr, map_stack_addr;

	assert(!(vstart % VM_PAGE_SIZE));
	assert(!(text_addr % VM_PAGE_SIZE));
	assert(!(text_bytes % VM_PAGE_SIZE));
	assert(!(data_addr % VM_PAGE_SIZE));
	assert(!(data_bytes % VM_PAGE_SIZE));
	assert(!(stack_bytes % VM_PAGE_SIZE));
	assert(!(gap_bytes % VM_PAGE_SIZE));
	assert(!(text_start % VM_PAGE_SIZE));
	assert(!(data_start % VM_PAGE_SIZE));
	assert((!text_start && !data_start) || (text_start && data_start));

	/* Place text at start of process. */
	map_text_addr = vstart + text_addr;
	vmp->vm_arch.vm_seg[T].mem_phys = ABS2CLICK(map_text_addr);
	vmp->vm_arch.vm_seg[T].mem_vir = ABS2CLICK(text_addr);
	if(full_memview) {
		vmp->vm_arch.vm_seg[T].mem_len = ABS2CLICK(VM_DATATOP) -
			vmp->vm_arch.vm_seg[T].mem_phys;
	} else {
		vmp->vm_arch.vm_seg[T].mem_len = ABS2CLICK(text_bytes);
	}

	vmp->vm_offset = vstart;

	/* page mapping flags for code */
#define TEXTFLAGS (PTF_PRESENT | PTF_USER)
	SANITYCHECK(SCL_DETAIL);
	if(text_bytes > 0) {
		if(!(reg=map_page_region(vmp, map_text_addr, 0, text_bytes,
		  text_start ? text_start : MAP_NONE,
		  VR_ANON | VR_WRITABLE, text_start ? 0 : MF_PREALLOC))) {
			SANITYCHECK(SCL_DETAIL);
			printf("VM: proc_new: map_page_region failed (text)\n");
			map_free_proc(vmp);
			SANITYCHECK(SCL_DETAIL);
			return(ENOMEM);
		}

		map_region_set_tag(reg, VRT_TEXT);
		SANITYCHECK(SCL_DETAIL);
	}
	SANITYCHECK(SCL_DETAIL);

	/* Allocate memory for data (including bss, but not including gap
	 * or stack), make sure it's cleared, and map it in after text
	 * (if any).
	 */
	if (is_elf) {
	    map_data_addr = vstart + data_addr;
	} else {
	    map_data_addr = vstart + text_bytes;
	}

	if(!(vmp->vm_heap = map_page_region(vmp, map_data_addr, 0,
	  data_bytes, data_start ? data_start : MAP_NONE, VR_ANON | VR_WRITABLE,
		data_start ? 0 : MF_PREALLOC))) {
		printf("VM: exec: map_page_region for data failed\n");
		map_free_proc(vmp);
		SANITYCHECK(SCL_DETAIL);
		return ENOMEM;
	}

	/* Tag the heap so brk() call knows which region to extend. */
	map_region_set_tag(vmp->vm_heap, VRT_HEAP);

	/* How many address space clicks between end of data
	 * and start of stack?
	 * stacktop is the first address after the stack, as addressed
	 * from within the user process.
	 */
	hole_bytes = stacktop - data_bytes - stack_bytes
	    - gap_bytes - data_addr;

	map_stack_addr = map_data_addr + data_bytes + hole_bytes;

	if(!(reg=map_page_region(vmp,
		map_stack_addr,
	  0, stack_bytes + gap_bytes, MAP_NONE,
	  VR_ANON | VR_WRITABLE, prealloc_stack ? MF_PREALLOC : 0)) != OK) {
	  	panic("map_page_region failed for stack");
	}

	map_region_set_tag(reg, VRT_STACK);

	vmp->vm_arch.vm_seg[D].mem_phys = ABS2CLICK(map_data_addr);
	vmp->vm_arch.vm_seg[D].mem_vir = ABS2CLICK(data_addr);
	vmp->vm_arch.vm_seg[D].mem_len = ABS2CLICK(data_bytes);

	vmp->vm_arch.vm_seg[S].mem_phys = ABS2CLICK(map_data_addr +
		data_bytes + gap_bytes + hole_bytes);
	vmp->vm_arch.vm_seg[S].mem_vir = ABS2CLICK(data_addr +
		data_bytes + gap_bytes + hole_bytes);

	/* Where are we allowed to start using the rest of the virtual
	 * address space?
	 */
	vmp->vm_stacktop = stacktop;

	vmp->vm_flags |= VMF_HASPT;

	if(vmp->vm_endpoint != NONE) {

		/* Pretend the stack is the full size of the data segment, so
		 * we get a full-sized data segment, up to VM_DATATOP.
		 * After sys_newmap(), change the stack to what we know the
		 * stack to be (up to stacktop).
		 */
		vmp->vm_arch.vm_seg[S].mem_len = (VM_DATATOP >> CLICK_SHIFT) -
		    vmp->vm_arch.vm_seg[S].mem_vir - ABS2CLICK(map_data_addr);

		/* What is the final size of the data segment in bytes? */
		vmp->vm_arch.vm_data_top =
		    (vmp->vm_arch.vm_seg[S].mem_vir +
		     vmp->vm_arch.vm_seg[S].mem_len) << CLICK_SHIFT;

		if((s=sys_newmap(vmp->vm_endpoint, vmp->vm_arch.vm_seg)) != OK)
			panic("sys_newmap (vm) failed: %d", s);
		if((s=pt_bind(&vmp->vm_pt, vmp)) != OK)
			panic("exec_newmem: pt_bind failed: %d", s);
	}

	return OK;
}
