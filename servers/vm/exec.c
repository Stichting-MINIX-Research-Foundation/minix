
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
	vir_bytes stk_bytes, phys_bytes tot_bytes, vir_bytes *stack_top));

static int failcount;

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
		printf("VM: exec_newmem: bad endpoint %d from %d\n",
			proc_e, msg->m_source);
		return ESRCH;
	}
	vmp= &vmproc[proc_n];
	ptr= msg->VMEN_ARGSPTR;

	NOTRUNNABLE(vmp->vm_endpoint);

	if(msg->VMEN_ARGSSIZE != sizeof(args)) {
		printf("VM: exec_newmem: args size %d != %ld\n",
			msg->VMEN_ARGSSIZE, sizeof(args));
		return EINVAL;
	}
SANITYCHECK(SCL_DETAIL);

	r= sys_datacopy(msg->m_source, (vir_bytes)ptr,
		SELF, (vir_bytes)&args, sizeof(args));
	if (r != OK)
		vm_panic("exec_newmem: sys_datacopy failed", r);

	/* Minimum stack region (not preallocated)
	 * Stopgap for better rlimit-based stack size system
	 */
	if(args.tot_bytes < MINSTACKREGION) {
		args.tot_bytes = MINSTACKREGION;
	}

	/* Check to see if segment sizes are feasible. */
	tc = (vir_clicks) (CLICK_CEIL(args.text_bytes) >> CLICK_SHIFT);
	dc = (vir_clicks) (CLICK_CEIL(args.data_bytes+args.bss_bytes) >> CLICK_SHIFT);
	totc = (vir_clicks) (CLICK_CEIL(args.tot_bytes) >> CLICK_SHIFT);
	sc = (vir_clicks) (CLICK_CEIL(args.args_bytes) >> CLICK_SHIFT);
	if (dc >= totc) {
		printf("VM: newmem: no stack?\n");
		return(ENOEXEC); /* stack must be at least 1 click */
	}

	dvir = (args.sep_id ? 0 : tc);
	s_vir = dvir + (totc - sc);
	r = (dvir + dc > s_vir) ? ENOMEM : OK;
	if (r != OK) {
		printf("VM: newmem: no virtual space?\n");
		return r;
	}

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
		args.bss_bytes, args.args_bytes, args.tot_bytes, &stack_top);
	if (r != OK) {
		printf("VM: newmem: new_mem failed\n");
		return(r);
	}

	/* Save file identification to allow it to be shared. */
	vmp->vm_ino = args.st_ino;
	vmp->vm_dev = args.st_dev;
	vmp->vm_ctime = args.st_ctime;

	/* set/clear separate I&D flag */
	if (args.sep_id)
		vmp->vm_flags |= VMF_SEPARATE;	
	else
		vmp->vm_flags &= ~VMF_SEPARATE;

	msg->VMEN_STACK_TOP = (void *) stack_top;
	msg->VMEN_FLAGS = 0;
	if (!sh_mp)			 /* Load text if sh_mp = NULL */
		msg->VMEN_FLAGS |= EXC_NM_RF_LOAD_TEXT;

	NOTRUNNABLE(vmp->vm_endpoint);

	return OK;
}

/*===========================================================================*
 *				new_mem					     *
 *===========================================================================*/
PRIVATE int new_mem(rmp, sh_mp, text_bytes, data_bytes,
	bss_bytes,stk_bytes,tot_bytes,stack_top)
struct vmproc *rmp;		/* process to get a new memory map */
struct vmproc *sh_mp;		/* text can be shared with this process */
vir_bytes text_bytes;		/* text segment size in bytes */
vir_bytes data_bytes;		/* size of initialized data in bytes */
vir_bytes bss_bytes;		/* size of bss in bytes */
vir_bytes stk_bytes;		/* size of initial stack segment in bytes */
phys_bytes tot_bytes;		/* total memory to allocate, including gap */
vir_bytes *stack_top;		/* top of process stack */
{
/* Allocate new memory and release the old memory.  Change the map and report
 * the new map to the kernel.  Zero the new core image's bss, gap and stack.
 */

  vir_clicks text_clicks, data_clicks, gap_clicks, stack_clicks, tot_clicks;
  phys_bytes bytes, base, bss_offset;
  int s, r2, r, hadpt = 0;
  struct vmproc *vmpold = &vmproc[VMP_EXECTMP];

  SANITYCHECK(SCL_FUNCTIONS);

  if(rmp->vm_flags & VMF_HASPT) {
	hadpt = 1;
  }

  /* No need to allocate text if it can be shared. */
  if (sh_mp != NULL) {
	text_bytes = 0;
	vm_assert(!vm_paged);
  }

  /* Acquire the new memory.  Each of the 4 parts: text, (data+bss), gap,
   * and stack occupies an integral number of clicks, starting at click
   * boundary.  The data and bss parts are run together with no space.
   */
  text_clicks = (vir_clicks) (CLICK_CEIL(text_bytes) >> CLICK_SHIFT);
  data_clicks = (vir_clicks) (CLICK_CEIL(data_bytes + bss_bytes) >> CLICK_SHIFT);
  stack_clicks = (vir_clicks) (CLICK_CEIL(stk_bytes) >> CLICK_SHIFT);
  tot_clicks = (vir_clicks) (CLICK_CEIL(tot_bytes) >> CLICK_SHIFT);
  gap_clicks = tot_clicks - data_clicks - stack_clicks;
  if ( (int) gap_clicks < 0) {
	printf("VM: new_mem: no gap?\n");
	return(ENOMEM);
  }


  /* Keep previous process state for recovery; the sanity check functions
   * know about the 'vmpold' slot, so the memory that the exec()ing
   * process is still holding is referenced there.
   *
   * Throw away the old page table to avoid having two process slots
   * using the same vm_pt.
   * Just recreate it in the case that we have to revert.
   */
SANITYCHECK(SCL_DETAIL);
  if(hadpt) {
	  pt_free(&rmp->vm_pt);
	  rmp->vm_flags &= ~VMF_HASPT;
  }
  vm_assert(!(vmpold->vm_flags & VMF_INUSE));
  *vmpold = *rmp;	/* copy current state. */
  rmp->vm_regions = NULL; /* exec()ing process regions thrown out. */
SANITYCHECK(SCL_DETAIL);

  if(!hadpt) {
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

  /* Build new process in current slot, without freeing old
   * one. If it fails, revert.
   */

  if(vm_paged) {
	int ptok = 1;
	SANITYCHECK(SCL_DETAIL);
	if((r=pt_new(&rmp->vm_pt)) != OK) {
		ptok = 0;
		printf("exec_newmem: no new pagetable\n");
	}

	SANITYCHECK(SCL_DETAIL);
	if(r != OK || (r=proc_new(rmp,
	 VM_PROCSTART,	/* where to start the process in the page table */
	 CLICK2ABS(text_clicks),/* how big is the text in bytes, page-aligned */
	 CLICK2ABS(data_clicks),/* how big is data+bss, page-aligned */
	 CLICK2ABS(stack_clicks),/* how big is stack, page-aligned */
	 CLICK2ABS(gap_clicks),	/* how big is gap, page-aligned */
	 0,0,			/* not preallocated */
	 VM_STACKTOP		/* regular stack top */
	 )) != OK) {
		SANITYCHECK(SCL_DETAIL);
		printf("VM: new_mem: failed\n");
		if(ptok) {
			pt_free(&rmp->vm_pt);
		}
		*rmp = *vmpold;	/* undo. */
		clear_proc(vmpold);	/* disappear. */
		SANITYCHECK(SCL_DETAIL);
		if(hadpt) {
			if(pt_new(&rmp->vm_pt) != OK) {
			/* We secretly know that making a new pagetable
			 * in the same slot if one was there will never fail.
			 */
				vm_panic("new_mem: pt_new failed", ENOMEM);
			}
			rmp->vm_flags |= VMF_HASPT;
			SANITYCHECK(SCL_DETAIL);
			if(map_writept(rmp) != OK) {
				printf("VM: warning: exec undo failed\n");
			}
			SANITYCHECK(SCL_DETAIL);
		}
		return r;
	}
	SANITYCHECK(SCL_DETAIL);
	/* new process is made; free and unreference
	 * page table and memory still held by exec()ing process.
	 */
	SANITYCHECK(SCL_DETAIL);
	free_proc(vmpold);
	clear_proc(vmpold);	/* disappear. */
	SANITYCHECK(SCL_DETAIL);
	*stack_top = VM_STACKTOP;
  } else {
  	phys_clicks new_base;

	new_base = ALLOC_MEM(text_clicks + tot_clicks, 0);
	if (new_base == NO_MEM) {
		printf("VM: new_mem: ALLOC_MEM failed\n");
		return(ENOMEM);
	}

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

	  /* Tell kernel this thing has no page table. */
	  if((s=pt_bind(NULL, rmp)) != OK)
		vm_panic("exec_newmem: pt_bind failed", s);
	*stack_top= ((vir_bytes)rmp->vm_arch.vm_seg[S].mem_vir << CLICK_SHIFT) +
               ((vir_bytes)rmp->vm_arch.vm_seg[S].mem_len << CLICK_SHIFT);
  }

SANITYCHECK(SCL_FUNCTIONS);

  return(OK);
}

/*===========================================================================*
 *				find_kernel_top				     *
 *===========================================================================*/
PUBLIC phys_bytes find_kernel_top(void)
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

	return CLICK2ABS(kernel_top);
}

/*===========================================================================*
 *				proc_new				     *
 *===========================================================================*/
PUBLIC int proc_new(struct vmproc *vmp,
  phys_bytes vstart,	  /* where to start the process in page table */
  phys_bytes text_bytes,  /* how much code, in bytes but page aligned */
  phys_bytes data_bytes,  /* how much data + bss, in bytes but page aligned */
  phys_bytes stack_bytes, /* stack space to reserve, in bytes, page aligned */
  phys_bytes gap_bytes,   /* gap bytes, page aligned */
  phys_bytes text_start,  /* text starts here, if preallocated, otherwise 0 */
  phys_bytes data_start,  /* data starts here, if preallocated, otherwise 0 */
  phys_bytes stacktop
)
{
	int s;
	vir_bytes hole_bytes;
	int prealloc;

	vm_assert(!(vstart % VM_PAGE_SIZE));
	vm_assert(!(text_bytes % VM_PAGE_SIZE));
	vm_assert(!(data_bytes % VM_PAGE_SIZE));
	vm_assert(!(stack_bytes % VM_PAGE_SIZE));
	vm_assert(!(gap_bytes % VM_PAGE_SIZE));
	vm_assert(!(text_start % VM_PAGE_SIZE));
	vm_assert(!(data_start % VM_PAGE_SIZE));
	vm_assert((!text_start && !data_start) || (text_start && data_start));

	/* Place text at start of process. */
	vmp->vm_arch.vm_seg[T].mem_phys = ABS2CLICK(vstart);
	vmp->vm_arch.vm_seg[T].mem_vir = 0;
	vmp->vm_arch.vm_seg[T].mem_len = ABS2CLICK(text_bytes);

	vmp->vm_offset = vstart;

	/* page mapping flags for code */
#define TEXTFLAGS (PTF_PRESENT | PTF_USER)
	SANITYCHECK(SCL_DETAIL);
	if(text_bytes > 0) {
		if(!map_page_region(vmp, vstart, 0, text_bytes,
		  text_start ? text_start : MAP_NONE,
		  VR_ANON | VR_WRITABLE, text_start ? 0 : MF_PREALLOC)) {
			SANITYCHECK(SCL_DETAIL);
			printf("VM: proc_new: map_page_region failed (text)\n");
			map_free_proc(vmp);
			SANITYCHECK(SCL_DETAIL);
			return(ENOMEM);
		}
		SANITYCHECK(SCL_DETAIL);
	}
	SANITYCHECK(SCL_DETAIL);

	/* Allocate memory for data (including bss, but not including gap
	 * or stack), make sure it's cleared, and map it in after text
	 * (if any).
	 */
	if(!(vmp->vm_heap = map_page_region(vmp, vstart + text_bytes, 0,
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
	hole_bytes = stacktop - data_bytes - stack_bytes - gap_bytes;

	if(!map_page_region(vmp, vstart + text_bytes + data_bytes + hole_bytes,
	  0, stack_bytes + gap_bytes, MAP_NONE,
	  VR_ANON | VR_WRITABLE, 0) != OK) {
	  	vm_panic("map_page_region failed for stack", NO_NUM);
	}

	vmp->vm_arch.vm_seg[D].mem_phys = ABS2CLICK(vstart + text_bytes);
	vmp->vm_arch.vm_seg[D].mem_vir = 0;
	vmp->vm_arch.vm_seg[D].mem_len = ABS2CLICK(data_bytes);

	vmp->vm_arch.vm_seg[S].mem_phys = ABS2CLICK(vstart +
		text_bytes + data_bytes + gap_bytes + hole_bytes);
	vmp->vm_arch.vm_seg[S].mem_vir = ABS2CLICK(data_bytes + gap_bytes + hole_bytes);

	/* Pretend the stack is the full size of the data segment, so 
	 * we get a full-sized data segment, up to VM_DATATOP.
	 * After sys_newmap(), change the stack to what we know the
	 * stack to be (up to stacktop).
	 */
	vmp->vm_arch.vm_seg[S].mem_len = (VM_DATATOP >> CLICK_SHIFT) -
		vmp->vm_arch.vm_seg[S].mem_vir - ABS2CLICK(vstart) - ABS2CLICK(text_bytes);

	/* Where are we allowed to start using the rest of the virtual
	 * address space?
	 */
	vmp->vm_stacktop = stacktop;

	/* What is the final size of the data segment in bytes? */
	vmp->vm_arch.vm_data_top = 
		(vmp->vm_arch.vm_seg[S].mem_vir + 
		vmp->vm_arch.vm_seg[S].mem_len) << CLICK_SHIFT;

	vmp->vm_flags |= VMF_HASPT;

	if((s=sys_newmap(vmp->vm_endpoint, vmp->vm_arch.vm_seg)) != OK)
		vm_panic("sys_newmap (vm) failed", s);

	if((s=pt_bind(&vmp->vm_pt, vmp)) != OK)
		vm_panic("exec_newmem: pt_bind failed", s);

	return OK;
}
