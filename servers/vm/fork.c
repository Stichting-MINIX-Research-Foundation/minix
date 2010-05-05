
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
#include <minix/debug.h>
#include <minix/bitmap.h>

#include <string.h>
#include <errno.h>
#include <env.h>
#include <assert.h>

#include "glo.h"
#include "vm.h"
#include "proto.h"
#include "util.h"
#include "sanitycheck.h"
#include "region.h"
#include "memory.h"

/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
PUBLIC int do_fork(message *msg)
{
  int r, proc, s, childproc, fullvm;
  struct vmproc *vmp, *vmc;
  pt_t origpt;
  vir_bytes msgaddr;

  SANITYCHECK(SCL_FUNCTIONS);

  if(vm_isokendpt(msg->VMF_ENDPOINT, &proc) != OK) {
	printf("VM: bogus endpoint VM_FORK %d\n", msg->VMF_ENDPOINT);
  SANITYCHECK(SCL_FUNCTIONS);
	return EINVAL;
  }

  childproc = msg->VMF_SLOTNO;
  if(childproc < 0 || childproc >= NR_PROCS) {
	printf("VM: bogus slotno VM_FORK %d\n", msg->VMF_SLOTNO);
  SANITYCHECK(SCL_FUNCTIONS);
	return EINVAL;
  }

  vmp = &vmproc[proc];		/* parent */
  vmc = &vmproc[childproc];	/* child */
  assert(vmc->vm_slot == childproc);

  if(vmp->vm_flags & VMF_HAS_DMA) {
	printf("VM: %d has DMA memory and may not fork\n", msg->VMF_ENDPOINT);
	return EINVAL;
  }

  fullvm = vmp->vm_flags & VMF_HASPT;

  /* The child is basically a copy of the parent. */
  origpt = vmc->vm_pt;
  *vmc = *vmp;
  vmc->vm_slot = childproc;
  vmc->vm_regions = NULL;
  yielded_init(&vmc->vm_yielded_blocks);
  vmc->vm_endpoint = NONE;	/* In case someone tries to use it. */
  vmc->vm_pt = origpt;
  vmc->vm_flags &= ~VMF_HASPT;

#if VMSTATS
  vmc->vm_bytecopies = 0;
#endif

  if(pt_new(&vmc->vm_pt) != OK) {
	printf("VM: fork: pt_new failed\n");
	return ENOMEM;
  }

  vmc->vm_flags |= VMF_HASPT;

  if(fullvm) {
	SANITYCHECK(SCL_DETAIL);

	if(map_proc_copy(vmc, vmp) != OK) {
		printf("VM: fork: map_proc_copy failed\n");
		pt_free(&vmc->vm_pt);
		return(ENOMEM);
	}

	if(vmp->vm_heap) {
		vmc->vm_heap = map_region_lookup_tag(vmc, VRT_HEAP);
		assert(vmc->vm_heap);
	}

	SANITYCHECK(SCL_DETAIL);
  } else {
	vir_bytes sp;
	struct vir_region *heap, *stack;
	vir_bytes text_bytes, data_bytes, stack_bytes, parent_gap_bytes,
		child_gap_bytes;

	/* Get SP of new process (using parent). */
	if(get_stack_ptr(vmp->vm_endpoint, &sp) != OK) {
		printf("VM: fork: get_stack_ptr failed for %d\n",
			vmp->vm_endpoint);
		return ENOMEM;
	}

	/* Update size of stack segment using current SP. */
	if(adjust(vmp, vmp->vm_arch.vm_seg[D].mem_len, sp) != OK) {
		printf("VM: fork: adjust failed for %d\n",
			vmp->vm_endpoint);
		return ENOMEM;
	}

	/* Copy newly adjust()ed stack segment size to child. */
	vmc->vm_arch.vm_seg[S] = vmp->vm_arch.vm_seg[S];

        text_bytes = CLICK2ABS(vmc->vm_arch.vm_seg[T].mem_len);
        data_bytes = CLICK2ABS(vmc->vm_arch.vm_seg[D].mem_len);
	stack_bytes = CLICK2ABS(vmc->vm_arch.vm_seg[S].mem_len);

	/* how much space after break and before lower end (which is the
	 * logical top) of stack for the parent
	 */
	parent_gap_bytes = CLICK2ABS(vmc->vm_arch.vm_seg[S].mem_vir -
		vmc->vm_arch.vm_seg[D].mem_len);

	/* how much space can the child stack grow downwards, below
	 * the current SP? The rest of the gap is available for the
	 * heap to grow upwards.
	 */
	child_gap_bytes = VM_PAGE_SIZE;

        if((r=proc_new(vmc, VM_PROCSTART,
		text_bytes, data_bytes, stack_bytes, child_gap_bytes, 0, 0,
		CLICK2ABS(vmc->vm_arch.vm_seg[S].mem_vir +
                        vmc->vm_arch.vm_seg[S].mem_len), 1)) != OK) {
			printf("VM: fork: proc_new failed\n");
			return r;
	}

	if(!(heap = map_region_lookup_tag(vmc, VRT_HEAP)))
		panic("couldn't lookup heap");
	assert(heap->phys);
	if(!(stack = map_region_lookup_tag(vmc, VRT_STACK)))
		panic("couldn't lookup stack");
	assert(stack->phys);

	/* Now copy the memory regions. */

	if(vmc->vm_arch.vm_seg[T].mem_len > 0) {
		struct vir_region *text;
		if(!(text = map_region_lookup_tag(vmc, VRT_TEXT)))
			panic("couldn't lookup text");
		assert(text->phys);
		if(copy_abs2region(CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys),
			text, 0, text_bytes) != OK)
				panic("couldn't copy text");
	}

	if(copy_abs2region(CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys),
		heap, 0, data_bytes) != OK)
			panic("couldn't copy heap");

	if(copy_abs2region(CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys +
			vmc->vm_arch.vm_seg[D].mem_len) + parent_gap_bytes,
			stack, child_gap_bytes, stack_bytes) != OK)
			panic("couldn't copy stack");
  }

  /* Only inherit these flags. */
  vmc->vm_flags &= (VMF_INUSE|VMF_SEPARATE|VMF_HASPT);

  /* inherit the priv call bitmaps */
  memcpy(&vmc->vm_call_mask, &vmp->vm_call_mask, sizeof(vmc->vm_call_mask));

  /* Tell kernel about the (now successful) FORK. */
  if((r=sys_fork(vmp->vm_endpoint, childproc,
	&vmc->vm_endpoint, vmc->vm_arch.vm_seg,
	PFF_VMINHIBIT, &msgaddr)) != OK) {
        panic("do_fork can't sys_fork: %d", r);
  }

  if(fullvm) {
	vir_bytes vir;
	/* making these messages writable is an optimisation
	 * and its return value needn't be checked.
	 */
	vir = arch_vir2map(vmc, msgaddr);
	handle_memory(vmc, vir, sizeof(message), 1);
	vir = arch_vir2map(vmp, msgaddr);
	handle_memory(vmp, vir, sizeof(message), 1);
  }

  if((r=pt_bind(&vmc->vm_pt, vmc)) != OK)
	panic("fork can't pt_bind: %d", r);

  /* Inform caller of new child endpoint. */
  msg->VMF_CHILD_ENDPOINT = vmc->vm_endpoint;

  SANITYCHECK(SCL_FUNCTIONS);
  return OK;
}

