
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

#include <errno.h>
#include <assert.h>
#include <env.h>
#include <pagetable.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "vm.h"
#include "region.h"
#include "sanitycheck.h"

#include "memory.h"

FORWARD _PROTOTYPE( int new_mem, (struct vmproc *vmp, struct vmproc *sh_vmp,
	vir_bytes text_bytes, vir_bytes data_bytes, vir_bytes bss_bytes,
	vir_bytes stk_bytes, phys_bytes tot_bytes)	);
FORWARD _PROTOTYPE( u32_t find_kernel_top, (void));

/*===========================================================================*
 *                              find_share                                   *
 *===========================================================================*/
PUBLIC struct vmproc *find_share(vmp_ign, ino, dev, ctime)
struct vmproc *vmp_ign;         /* process that should not be looked at */
ino_t ino;                      /* parameters that uniquely identify a file */
dev_t dev;
time_t ctime;
{
/* Look for a process that is the file <ino, dev, ctime> in execution.  Don't
 * accidentally "find" vmp_ign, because it is the process on whose behalf this
 * call is made.
 */
  struct vmproc *vmp;
  for (vmp = &vmproc[0]; vmp < &vmproc[NR_PROCS]; vmp++) {
        if (!(vmp->vm_flags & VMF_INUSE)) continue;
        if (!(vmp->vm_flags & VMF_SEPARATE)) continue;
        if (vmp->vm_flags & VMF_HASPT) continue;
        if (vmp == vmp_ign) continue;
        if (vmp->vm_ino != ino) continue;
        if (vmp->vm_dev != dev) continue;
        if (vmp->vm_ctime != ctime) continue;
        return vmp;
  }
  return(NULL);
}


/*===========================================================================*
 *				exec_newmem				     *
 *===========================================================================*/
PUBLIC int do_exec_newmem(message *msg)
{
	int r, proc_e, proc_n;
	vir_bytes stack_top;
	vir_clicks tc, dc, sc, totc, dvir, s_vir;
	struct vmproc *vmp, *sh_mp;
	char *ptr;
	struct exec_newmem args;

	SANITYCHECK(SCL_FUNCTIONS);

	proc_e= msg->VMEN_ENDPOINT;
	if (vm_isokendpt(proc_e, &proc_n) != OK)
	{
		printf("VM:exec_newmem: bad endpoint %d from %d\n",
			proc_e, msg->m_source);
		return ESRCH;
	}
	vmp= &vmproc[proc_n];
	ptr= msg->VMEN_ARGSPTR;

	if(msg->VMEN_ARGSSIZE != sizeof(args)) {
		printf("VM:exec_newmem: args size %d != %ld\n",
			msg->VMEN_ARGSSIZE, sizeof(args));
		return EINVAL;
	}
SANITYCHECK(SCL_DETAIL);

	r= sys_datacopy(msg->m_source, (vir_bytes)ptr,
		SELF, (vir_bytes)&args, sizeof(args));
	if (r != OK)
		vm_panic("exec_newmem: sys_datacopy failed", r);

	/* Check to see if segment sizes are feasible. */
	tc = ((unsigned long) args.text_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
	dc = (args.data_bytes+args.bss_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
	totc = (args.tot_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
	sc = (args.args_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
	if (dc >= totc) return(ENOEXEC); /* stack must be at least 1 click */

	dvir = (args.sep_id ? 0 : tc);
	s_vir = dvir + (totc - sc);
	r = (dvir + dc > s_vir) ? ENOMEM : OK;
	if (r != OK)
		return r;

	/* Can the process' text be shared with that of one already running? */
	if(!vm_paged) {
		sh_mp = find_share(vmp, args.st_ino, args.st_dev, args.st_ctime);
	} else {
		sh_mp = NULL;
	}

	/* Allocate new memory and release old memory.  Fix map and tell
	 * kernel.
	 */
	r = new_mem(vmp, sh_mp, args.text_bytes, args.data_bytes,
		args.bss_bytes, args.args_bytes, args.tot_bytes);
	if (r != OK) return(r);

	/* Save file identification to allow it to be shared. */
	vmp->vm_ino = args.st_ino;
	vmp->vm_dev = args.st_dev;
	vmp->vm_ctime = args.st_ctime;

	stack_top= ((vir_bytes)vmp->vm_arch.vm_seg[S].mem_vir << CLICK_SHIFT) +
		((vir_bytes)vmp->vm_arch.vm_seg[S].mem_len << CLICK_SHIFT);

	/* set/clear separate I&D flag */
	if (args.sep_id)
		vmp->vm_flags |= VMF_SEPARATE;	
	else
		vmp->vm_flags &= ~VMF_SEPARATE;

	
	msg->VMEN_STACK_TOP = (void *) stack_top;
	msg->VMEN_FLAGS = 0;
	if (!sh_mp)			 /* Load text if sh_mp = NULL */
		msg->VMEN_FLAGS |= EXC_NM_RF_LOAD_TEXT;

	return OK;
}

/*===========================================================================*
 *				new_mem					     *
 *===========================================================================*/
PRIVATE int new_mem(rmp, sh_mp, text_bytes, data_bytes,
	bss_bytes,stk_bytes,tot_bytes)
struct vmproc *rmp;		/* process to get a new memory map */
struct vmproc *sh_mp;		/* text can be shared with this process */
vir_bytes text_bytes;		/* text segment size in bytes */
vir_bytes data_bytes;		/* size of initialized data in bytes */
vir_bytes bss_bytes;		/* size of bss in bytes */
vir_bytes stk_bytes;		/* size of initial stack segment in bytes */
phys_bytes tot_bytes;		/* total memory to allocate, including gap */
{
/* Allocate new memory and release the old memory.  Change the map and report
 * the new map to the kernel.  Zero the new core image's bss, gap and stack.
 */

  vir_clicks text_clicks, data_clicks, gap_clicks, stack_clicks, tot_clicks;
  phys_bytes bytes, base, bss_offset;
  int s, r2;
  static u32_t kernel_top = 0;

  SANITYCHECK(SCL_FUNCTIONS);

  /* No need to allocate text if it can be shared. */
  if (sh_mp != NULL) {
	text_bytes = 0;
	vm_assert(!vm_paged);
  }

  /* Acquire the new memory.  Each of the 4 parts: text, (data+bss), gap,
   * and stack occupies an integral number of clicks, starting at click
   * boundary.  The data and bss parts are run together with no space.
   */
  text_clicks = ((unsigned long) text_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  data_clicks = (data_bytes + bss_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  stack_clicks = (stk_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  tot_clicks = (tot_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  gap_clicks = tot_clicks - data_clicks - stack_clicks;
  if ( (int) gap_clicks < 0) return(ENOMEM);

SANITYCHECK(SCL_DETAIL);


  /* We've got memory for the new core image.  Release the old one. */

  if(rmp->vm_flags & VMF_HASPT) {
  	/* Free page table and memory allocated by pagetable functions. */
	rmp->vm_flags &= ~VMF_HASPT;
	free_proc(rmp);
  } else {

  	if (find_share(rmp, rmp->vm_ino, rmp->vm_dev, rmp->vm_ctime) == NULL) {
		/* No other process shares the text segment, so free it. */
		FREE_MEM(rmp->vm_arch.vm_seg[T].mem_phys, rmp->vm_arch.vm_seg[T].mem_len);
  	}

  	/* Free the data and stack segments. */
  	FREE_MEM(rmp->vm_arch.vm_seg[D].mem_phys,
		rmp->vm_arch.vm_seg[S].mem_vir
		+ rmp->vm_arch.vm_seg[S].mem_len
		- rmp->vm_arch.vm_seg[D].mem_vir);
  }

  /* We have now passed the point of no return.  The old core image has been
   * forever lost, memory for a new core image has been allocated.  Set up
   * and report new map.
   */

  if(vm_paged) {
	vir_bytes hole_clicks;

	if(pt_new(&rmp->vm_pt) != OK)
		vm_panic("exec_newmem: no new pagetable", NO_NUM);
	SANITYCHECK(SCL_DETAIL);

	if(!map_proc_kernel(rmp)) {
		printf("VM: exec: map_proc_kernel failed\n");
		return ENOMEM;
	}

	if(!kernel_top)
		kernel_top = find_kernel_top();

	/* Place text at kernel top. */
	rmp->vm_arch.vm_seg[T].mem_phys = kernel_top;
	rmp->vm_arch.vm_seg[T].mem_vir = 0;
	rmp->vm_arch.vm_seg[T].mem_len = text_clicks;

	rmp->vm_offset = CLICK2ABS(kernel_top);

	vm_assert(!sh_mp);
	/* page mapping flags for code */
#define TEXTFLAGS (PTF_PRESENT | PTF_USER | PTF_WRITE)
	SANITYCHECK(SCL_DETAIL);
	if(text_clicks > 0) {
		if(!map_page_region(rmp, CLICK2ABS(kernel_top), 0,
		  CLICK2ABS(rmp->vm_arch.vm_seg[T].mem_len), 0,
		  VR_ANON | VR_WRITABLE, 0)) {
			SANITYCHECK(SCL_DETAIL);
			printf("VM: map_page_region failed (text)\n");
			return(ENOMEM);
		}
		SANITYCHECK(SCL_DETAIL);
	}
	SANITYCHECK(SCL_DETAIL);

	/* Allocate memory for data (including bss, but not including gap
	 * or stack), make sure it's cleared, and map it in after text
	 * (if any).
	 */
	if(!(rmp->vm_heap = map_page_region(rmp,
	  CLICK2ABS(kernel_top + text_clicks), 0,
	  CLICK2ABS(data_clicks), 0, VR_ANON | VR_WRITABLE, 0))) {
		printf("VM: exec: map_page_region for data failed\n");
		return ENOMEM;
	}

	map_region_set_tag(rmp->vm_heap, VRT_HEAP);

	/* How many address space clicks between end of data
	 * and start of stack?
	 * VM_STACKTOP is the first address after the stack, as addressed
	 * from within the user process.
	 */
	hole_clicks = VM_STACKTOP >> CLICK_SHIFT;
	hole_clicks -= data_clicks + stack_clicks + gap_clicks;

	if(!map_page_region(rmp,
	  CLICK2ABS(kernel_top + text_clicks + data_clicks + hole_clicks),
	  0, CLICK2ABS(stack_clicks+gap_clicks), 0,
	  VR_ANON | VR_WRITABLE, 0) != OK) {
	  	vm_panic("map_page_region failed for stack", NO_NUM);
	}

	rmp->vm_arch.vm_seg[D].mem_phys = kernel_top + text_clicks;
	rmp->vm_arch.vm_seg[D].mem_vir = 0;
	rmp->vm_arch.vm_seg[D].mem_len = data_clicks;


	rmp->vm_arch.vm_seg[S].mem_phys = kernel_top +
		text_clicks + data_clicks + gap_clicks + hole_clicks;
	rmp->vm_arch.vm_seg[S].mem_vir = data_clicks + gap_clicks + hole_clicks;
	
	/* Pretend the stack is the full size of the data segment, so 
	 * we get a full-sized data segment, up to VM_DATATOP.
	 * After sys_newmap(),, change the stack to what we know the
	 * stack to be (up to VM_STACKTOP).
	 */
	rmp->vm_arch.vm_seg[S].mem_len = (VM_DATATOP >> CLICK_SHIFT) -
		rmp->vm_arch.vm_seg[S].mem_vir - kernel_top - text_clicks;

	/* Where are we allowed to start using the rest of the virtual
	 * address space?
	 */
	rmp->vm_stacktop = VM_STACKTOP;

	/* What is the final size of the data segment in bytes? */
	rmp->vm_arch.vm_data_top = 
		(rmp->vm_arch.vm_seg[S].mem_vir + 
		rmp->vm_arch.vm_seg[S].mem_len) << CLICK_SHIFT;

	rmp->vm_flags |= VMF_HASPT;

	if((s=sys_newmap(rmp->vm_endpoint, rmp->vm_arch.vm_seg)) != OK) {
		vm_panic("sys_newmap (vm) failed", s);
	}


	/* This is the real stack clicks. */
	rmp->vm_arch.vm_seg[S].mem_len = stack_clicks;

  } else {
  	phys_clicks new_base;

	new_base = ALLOC_MEM(text_clicks + tot_clicks, 0);
	if (new_base == NO_MEM) return(ENOMEM);

	if (sh_mp != NULL) {
		/* Share the text segment. */
		rmp->vm_arch.vm_seg[T] = sh_mp->vm_arch.vm_seg[T];
	} else {
		rmp->vm_arch.vm_seg[T].mem_phys = new_base;
		rmp->vm_arch.vm_seg[T].mem_vir = 0;
		rmp->vm_arch.vm_seg[T].mem_len = text_clicks;
	
		if (text_clicks > 0)
		{
			/* Zero the last click of the text segment. Otherwise the
			 * part of that click may remain unchanged.
			 */
			base = (phys_bytes)(new_base+text_clicks-1) << CLICK_SHIFT;
			if ((s= sys_memset(0, base, CLICK_SIZE)) != OK)
				vm_panic("new_mem: sys_memset failed", s);
		}
  	}

	/* No paging stuff. */
	rmp->vm_flags &= ~VMF_HASPT;
	rmp->vm_regions = NULL;

	  rmp->vm_arch.vm_seg[D].mem_phys = new_base + text_clicks;
	  rmp->vm_arch.vm_seg[D].mem_vir = 0;
	  rmp->vm_arch.vm_seg[D].mem_len = data_clicks;
	  rmp->vm_arch.vm_seg[S].mem_phys = rmp->vm_arch.vm_seg[D].mem_phys +
		data_clicks + gap_clicks;
	  rmp->vm_arch.vm_seg[S].mem_vir = rmp->vm_arch.vm_seg[D].mem_vir +
		data_clicks + gap_clicks;
	  rmp->vm_arch.vm_seg[S].mem_len = stack_clicks;
	rmp->vm_stacktop =
		CLICK2ABS(rmp->vm_arch.vm_seg[S].mem_vir +
			rmp->vm_arch.vm_seg[S].mem_len);

	rmp->vm_arch.vm_data_top = 
		(rmp->vm_arch.vm_seg[S].mem_vir + 
		rmp->vm_arch.vm_seg[S].mem_len) << CLICK_SHIFT;

	  if((r2=sys_newmap(rmp->vm_endpoint, rmp->vm_arch.vm_seg)) != OK) {
		/* report new map to the kernel */
		vm_panic("sys_newmap failed", r2);
	  }

	  /* Zero the bss, gap, and stack segment. */
	  bytes = (phys_bytes)(data_clicks + gap_clicks + stack_clicks) << CLICK_SHIFT;
	  base = (phys_bytes) rmp->vm_arch.vm_seg[D].mem_phys << CLICK_SHIFT;
	  bss_offset = (data_bytes >> CLICK_SHIFT) << CLICK_SHIFT;
	  base += bss_offset;
	  bytes -= bss_offset;

	  if ((s=sys_memset(0, base, bytes)) != OK) {
		vm_panic("new_mem can't zero", s);
	  }
  }

  /* Whether vm_pt is NULL or a new pagetable, tell kernel about it. */
  if((s=pt_bind(&rmp->vm_pt, rmp)) != OK)
	vm_panic("exec_newmem: pt_bind failed", s);

SANITYCHECK(SCL_FUNCTIONS);

  return(OK);
}

/*===========================================================================*
 *				find_kernel_top				     *
 *===========================================================================*/
PRIVATE u32_t find_kernel_top(void)
{
/* Find out where the kernel is, so we know where to start mapping
 * user processes.
 */
	u32_t kernel_top = 0;
#define MEMTOP(v, i) \
  (vmproc[v].vm_arch.vm_seg[i].mem_phys + vmproc[v].vm_arch.vm_seg[i].mem_len)
	vm_assert(vmproc[VMP_SYSTEM].vm_flags & VMF_INUSE);
	kernel_top = MEMTOP(VMP_SYSTEM, T);
	kernel_top = MAX(kernel_top, MEMTOP(VMP_SYSTEM, D));
	kernel_top = MAX(kernel_top, MEMTOP(VMP_SYSTEM, S));
	vm_assert(kernel_top);

	return kernel_top;
}

