/* The MINIX model of memory allocation reserves a fixed amount of memory for
 * the combined text, data, and stack segments.  The amount used for a child
 * process created by FORK is the same as the parent had.  If the child does
 * an EXEC later, the new size is taken from the header of the file EXEC'ed.
 *
 * The layout in memory consists of the text segment, followed by the data
 * segment, followed by a gap (unused memory), followed by the stack segment.
 * The data segment grows upward and the stack grows downward, so each can
 * take memory from the gap.  If they meet, the process must be killed.  The
 * procedures in this file deal with the growth of the data and stack segments.
 *
 * The entry points into this file are:
 *   do_brk:      BRK/SBRK system calls to grow or shrink the data segment
 *   adjust:      see if a proposed segment adjustment is allowed
 */

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
#include <minix/bitmap.h>

#include <errno.h>
#include <env.h>

#include "glo.h"
#include "vm.h"
#include "proto.h"
#include "util.h"

#define DATA_CHANGED       1    /* flag value when data segment size changed */
#define STACK_CHANGED      2    /* flag value when stack size changed */

/*===========================================================================*
 *				do_brk					     *
 *===========================================================================*/
PUBLIC int do_brk(message *msg)
{
/* Perform the brk(addr) system call.
 * The parameter, 'addr' is the new virtual address in D space.
 */
	int proc;

	if(vm_isokendpt(msg->VMB_ENDPOINT, &proc) != OK) {
		printf("VM: bogus endpoint VM_BRK %d\n", msg->VMB_ENDPOINT);
		return EINVAL;
	}

	return real_brk(&vmproc[proc], (vir_bytes) msg->VMB_ADDR);
}

/*===========================================================================*
 *				adjust					     *
 *===========================================================================*/
PUBLIC int adjust(rmp, data_clicks, sp)
struct vmproc *rmp;		/* whose memory is being adjusted? */
vir_clicks data_clicks;		/* how big is data segment to become? */
vir_bytes sp;			/* new value of sp */
{
/* See if data and stack segments can coexist, adjusting them if need be.
 * Memory is never allocated or freed.  Instead it is added or removed from the
 * gap between data segment and stack segment.  If the gap size becomes
 * negative, the adjustment of data or stack fails and ENOMEM is returned.
 */

  register struct mem_map *mem_sp, *mem_dp;
  vir_clicks sp_click, gap_base, sp_lower, old_clicks;
  int changed, r, sp_in_dp;
  long base_of_stack, sp_delta;	/* longs avoid certain problems */

  mem_dp = &rmp->vm_arch.vm_seg[D];	/* pointer to data segment map */
  mem_sp = &rmp->vm_arch.vm_seg[S];	/* pointer to stack segment map */
  changed = 0;			/* set when either segment changed */

  /* See if stack size has gone negative (i.e., sp too close to 0xFFFF...) */
  base_of_stack = (long) mem_sp->mem_vir + (long) mem_sp->mem_len;
  sp_click = sp >> CLICK_SHIFT;	/* click containing sp */
  if (sp_click >= base_of_stack)
  {
	return(ENOMEM);	/* sp too high */
  }

  /* In order to support user-space libraries, processes might change sp to
     point to somewhere inside the data segment. If that's the case, be careful
     not to erroneously think that the data and stack have collided. */
  sp_in_dp = (mem_dp->mem_vir <= sp_click) &&
	     (mem_dp->mem_vir + mem_dp->mem_len >= sp_click);

  /* Compute size of gap between stack and data segments. */
  sp_delta = (long) mem_sp->mem_vir - (long) sp_click;
  sp_lower = ((sp_delta > 0 && !sp_in_dp) ? sp_click : mem_sp->mem_vir);

  /* Add a safety margin for future stack growth. Impossible to do right. */
#define SAFETY_BYTES  (384 * sizeof(char *))
#define SAFETY_CLICKS ((vir_clicks) (CLICK_CEIL(SAFETY_BYTES) >> CLICK_SHIFT))
  gap_base = mem_dp->mem_vir + data_clicks + SAFETY_CLICKS;
  if (sp_lower < gap_base)
  {
	return(ENOMEM);	/* data and stack collided */
  }

  /* Update data length (but not data orgin) on behalf of brk() system call. */
  old_clicks = mem_dp->mem_len;
  if (data_clicks != mem_dp->mem_len) {
	mem_dp->mem_len = data_clicks;
	changed |= DATA_CHANGED;
  }

  /* Update stack length and origin due to change in stack pointer. */
  if (sp_delta > 0 && !sp_in_dp) {
	mem_sp->mem_vir -= sp_delta;
	mem_sp->mem_phys -= sp_delta;
	mem_sp->mem_len += sp_delta;
	changed |= STACK_CHANGED;
  }

  /* Do the new data and stack segment sizes fit in the address space? */
  r = (rmp->vm_arch.vm_seg[D].mem_vir + rmp->vm_arch.vm_seg[D].mem_len > 
          rmp->vm_arch.vm_seg[S].mem_vir) ? ENOMEM : OK;

  if(r == OK && (rmp->vm_flags & VMF_HASPT) &&
     rmp->vm_endpoint != VM_PROC_NR && rmp->vm_heap) {
	if(old_clicks < data_clicks) {
		vir_bytes more;
		more = (data_clicks - old_clicks) << CLICK_SHIFT;
		if(map_region_extend(rmp, rmp->vm_heap, more) != OK) {
			printf("VM: brk: map_region_extend failed\n");
			return ENOMEM;
		}
	} else if(old_clicks > data_clicks) {
		vir_bytes less;
		less = (old_clicks - data_clicks) << CLICK_SHIFT;
		if(map_region_shrink(rmp->vm_heap, less) != OK) {
			printf("VM: brk: map_region_shrink failed\n");
			return ENOMEM;
		}
	}
  }

  if (r == OK)
	return(OK);

  /* New sizes don't fit or require too many page/segment registers. Restore.*/
  if (changed & DATA_CHANGED) mem_dp->mem_len = old_clicks;
  if (changed & STACK_CHANGED) {
	mem_sp->mem_vir += sp_delta;
	mem_sp->mem_phys += sp_delta;
	mem_sp->mem_len -= sp_delta;
  }
  return(ENOMEM);
}

/*===========================================================================*
 *				real_brk				     *
 *===========================================================================*/
PUBLIC int real_brk(vmp, v)
struct vmproc *vmp;
vir_bytes v;
{
	vir_bytes new_sp;
	vir_clicks new_clicks;
	int r;

	new_clicks = (vir_clicks) (CLICK_CEIL(v) >> CLICK_SHIFT);
	if (new_clicks < vmp->vm_arch.vm_seg[D].mem_vir) {
		printf("VM: real_brk failed because new_clicks too high: %d\n",
			new_clicks);
		return(ENOMEM);
	}
	new_clicks -= vmp->vm_arch.vm_seg[D].mem_vir;
	if ((r=get_stack_ptr(vmp->vm_endpoint, &new_sp)) != OK)
  		panic("couldn't get stack pointer: %d", r);
	r = adjust(vmp, new_clicks, new_sp);
	return r;
}
