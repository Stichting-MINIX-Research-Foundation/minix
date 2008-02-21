/* This file is concerned with allocating and freeing arbitrary-size blocks of
 * physical memory on behalf of the FORK and EXEC system calls.  The key data
 * structure used is the hole table, which maintains a list of holes in memory.
 * It is kept sorted in order of increasing memory address. The addresses
 * it contains refers to physical memory, starting at absolute address 0
 * (i.e., they are not relative to the start of PM).  During system
 * initialization, that part of memory containing the interrupt vectors,
 * kernel, and PM are "allocated" to mark them as not available and to
 * remove them from the hole list.
 *
 * The entry points into this file are:
 *   alloc_mem:	allocate a given sized chunk of memory
 *   free_mem:	release a previously allocated chunk of memory
 *   mem_init:	initialize the tables when PM start up
 *   max_hole:	returns the largest hole currently available
 *   mem_holes_copy: for outsiders who want a copy of the hole-list
 */

#include "pm.h"
#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/config.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <archconst.h>
#include "mproc.h"
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"

#define NIL_HOLE (struct hole *) 0

PRIVATE struct hole hole[_NR_HOLES];
PRIVATE u32_t high_watermark = 0;

PRIVATE struct hole *hole_head;	/* pointer to first hole */
PRIVATE struct hole *free_slots;/* ptr to list of unused table slots */
#if ENABLE_SWAP
PRIVATE int swap_fd = -1;	/* file descriptor of open swap file/device */
PRIVATE u32_t swap_offset;	/* offset to start of swap area on swap file */
PRIVATE phys_clicks swap_base;	/* memory offset chosen as swap base */
PRIVATE phys_clicks swap_maxsize;/* maximum amount of swap "memory" possible */
PRIVATE struct mproc *in_queue;	/* queue of processes wanting to swap in */
PRIVATE struct mproc *outswap = &mproc[0]; 	 /* outswap candidate? */
#else /* ! ENABLE_SWAP */
#define swap_base ((phys_clicks) -1)
#endif /* ENABLE_SWAP */

FORWARD _PROTOTYPE( void del_slot, (struct hole *prev_ptr, struct hole *hp) );
FORWARD _PROTOTYPE( void merge, (struct hole *hp)			    );
#if ENABLE_SWAP
FORWARD _PROTOTYPE( int swap_out, (void)				    );
#else
#define swap_out()	(0)
#endif

/*===========================================================================*
 *				alloc_mem				     *
 *===========================================================================*/
PUBLIC phys_clicks alloc_mem(clicks)
phys_clicks clicks;		/* amount of memory requested */
{
/* Allocate a block of memory from the free list using first fit. The block
 * consists of a sequence of contiguous bytes, whose length in clicks is
 * given by 'clicks'.  A pointer to the block is returned.  The block is
 * always on a click boundary.  This procedure is called when memory is
 * needed for FORK or EXEC.  Swap other processes out if needed.
 */
  register struct hole *hp, *prev_ptr;
  phys_clicks old_base;

  do {
        prev_ptr = NIL_HOLE;
	hp = hole_head;
	while (hp != NIL_HOLE && hp->h_base < swap_base) {
		if (hp->h_len >= clicks) {
			/* We found a hole that is big enough.  Use it. */
			old_base = hp->h_base;	/* remember where it started */
			hp->h_base += clicks;	/* bite a piece off */
			hp->h_len -= clicks;	/* ditto */

			/* Remember new high watermark of used memory. */
			if(hp->h_base > high_watermark)
				high_watermark = hp->h_base;

			/* Delete the hole if used up completely. */
			if (hp->h_len == 0) del_slot(prev_ptr, hp);

			/* Return the start address of the acquired block. */
			return(old_base);
		}

		prev_ptr = hp;
		hp = hp->h_next;
	}
  } while (swap_out());		/* try to swap some other process out */
  return(NO_MEM);
}

/*===========================================================================*
 *				free_mem				     *
 *===========================================================================*/
PUBLIC void free_mem(base, clicks)
phys_clicks base;		/* base address of block to free */
phys_clicks clicks;		/* number of clicks to free */
{
/* Return a block of free memory to the hole list.  The parameters tell where
 * the block starts in physical memory and how big it is.  The block is added
 * to the hole list.  If it is contiguous with an existing hole on either end,
 * it is merged with the hole or holes.
 */
  register struct hole *hp, *new_ptr, *prev_ptr;

  if (clicks == 0) return;
  if ( (new_ptr = free_slots) == NIL_HOLE) 
  	panic(__FILE__,"hole table full", NO_NUM);
  new_ptr->h_base = base;
  new_ptr->h_len = clicks;
  free_slots = new_ptr->h_next;
  hp = hole_head;

  /* If this block's address is numerically less than the lowest hole currently
   * available, or if no holes are currently available, put this hole on the
   * front of the hole list.
   */
  if (hp == NIL_HOLE || base <= hp->h_base) {
	/* Block to be freed goes on front of the hole list. */
	new_ptr->h_next = hp;
	hole_head = new_ptr;
	merge(new_ptr);
	return;
  }

  /* Block to be returned does not go on front of hole list. */
  prev_ptr = NIL_HOLE;
  while (hp != NIL_HOLE && base > hp->h_base) {
	prev_ptr = hp;
	hp = hp->h_next;
  }

  /* We found where it goes.  Insert block after 'prev_ptr'. */
  new_ptr->h_next = prev_ptr->h_next;
  prev_ptr->h_next = new_ptr;
  merge(prev_ptr);		/* sequence is 'prev_ptr', 'new_ptr', 'hp' */
}

/*===========================================================================*
 *				del_slot				     *
 *===========================================================================*/
PRIVATE void del_slot(prev_ptr, hp)
/* pointer to hole entry just ahead of 'hp' */
register struct hole *prev_ptr;
/* pointer to hole entry to be removed */
register struct hole *hp;	
{
/* Remove an entry from the hole list.  This procedure is called when a
 * request to allocate memory removes a hole in its entirety, thus reducing
 * the numbers of holes in memory, and requiring the elimination of one
 * entry in the hole list.
 */
  if (hp == hole_head)
	hole_head = hp->h_next;
  else
	prev_ptr->h_next = hp->h_next;

  hp->h_next = free_slots;
  hp->h_base = hp->h_len = 0;
  free_slots = hp;
}

/*===========================================================================*
 *				merge					     *
 *===========================================================================*/
PRIVATE void merge(hp)
register struct hole *hp;	/* ptr to hole to merge with its successors */
{
/* Check for contiguous holes and merge any found.  Contiguous holes can occur
 * when a block of memory is freed, and it happens to abut another hole on
 * either or both ends.  The pointer 'hp' points to the first of a series of
 * three holes that can potentially all be merged together.
 */
  register struct hole *next_ptr;

  /* If 'hp' points to the last hole, no merging is possible.  If it does not,
   * try to absorb its successor into it and free the successor's table entry.
   */
  if ( (next_ptr = hp->h_next) == NIL_HOLE) return;
  if (hp->h_base + hp->h_len == next_ptr->h_base) {
	hp->h_len += next_ptr->h_len;	/* first one gets second one's mem */
	del_slot(hp, next_ptr);
  } else {
	hp = next_ptr;
  }

  /* If 'hp' now points to the last hole, return; otherwise, try to absorb its
   * successor into it.
   */
  if ( (next_ptr = hp->h_next) == NIL_HOLE) return;
  if (hp->h_base + hp->h_len == next_ptr->h_base) {
	hp->h_len += next_ptr->h_len;
	del_slot(hp, next_ptr);
  }
}

/*===========================================================================*
 *				mem_init				     *
 *===========================================================================*/
PUBLIC void mem_init(chunks, free)
struct memory *chunks;		/* list of free memory chunks */
phys_clicks *free;		/* memory size summaries */
{
/* Initialize hole lists.  There are two lists: 'hole_head' points to a linked
 * list of all the holes (unused memory) in the system; 'free_slots' points to
 * a linked list of table entries that are not in use.  Initially, the former
 * list has one entry for each chunk of physical memory, and the second
 * list links together the remaining table slots.  As memory becomes more
 * fragmented in the course of time (i.e., the initial big holes break up into
 * smaller holes), new table slots are needed to represent them.  These slots
 * are taken from the list headed by 'free_slots'.
 */
  int i;
  register struct hole *hp;

  /* Put all holes on the free list. */
  for (hp = &hole[0]; hp < &hole[_NR_HOLES]; hp++) {
	hp->h_next = hp + 1;
	hp->h_base = hp->h_len = 0;
  }
  hole[_NR_HOLES-1].h_next = NIL_HOLE;
  hole_head = NIL_HOLE;
  free_slots = &hole[0];

  /* Use the chunks of physical memory to allocate holes. */
  *free = 0;
  for (i=NR_MEMS-1; i>=0; i--) {
  	if (chunks[i].size > 0) {
		free_mem(chunks[i].base, chunks[i].size);
		*free += chunks[i].size;
#if ENABLE_SWAP
		if (swap_base < chunks[i].base + chunks[i].size) 
			swap_base = chunks[i].base + chunks[i].size;
#endif
	}
  }

#if ENABLE_SWAP
  /* The swap area is represented as a hole above and separate of regular
   * memory.  A hole at the size of the swap file is allocated on "swapon".
   */
  swap_base++;				/* make separate */
  swap_maxsize = 0 - swap_base;		/* maximum we can possibly use */
#endif
}

/*===========================================================================*
 *				mem_holes_copy				     *
 *===========================================================================*/
PUBLIC int mem_holes_copy(struct hole *holecopies, size_t *bytes, u32_t *hi)
{
	if(*bytes < sizeof(hole)) return ENOSPC;
	memcpy(holecopies, hole, sizeof(hole));
	*bytes = sizeof(hole);
	*hi = high_watermark;
	return OK;
}

#define NR_DMA	16

PRIVATE struct dmatab
{
	int dt_flags;
	endpoint_t dt_proc;
	phys_bytes dt_base;
	phys_bytes dt_size;
	phys_clicks dt_seg_base;
	phys_clicks dt_seg_size;
} dmatab[NR_DMA];

#define DTF_INUSE	1
#define DTF_RELEASE_DMA	2
#define DTF_RELEASE_SEG	4

PRIVATE endpoint_t iommu_proc_e= ANY;

/*===========================================================================*
 *				do_adddma				     *
 *===========================================================================*/
PUBLIC int do_adddma()
{
	endpoint_t req_proc_e, target_proc_e;
	int i, proc_n;
	phys_bytes base, size;
	struct mproc *rmp;

	if (mp->mp_effuid != SUPER_USER)
		return EPERM;

	req_proc_e= m_in.m_source;
	target_proc_e= m_in.m2_i1;
	base= m_in.m2_l1;
	size= m_in.m2_l2;

	iommu_proc_e= req_proc_e;

	/* Find empty slot */
	for (i= 0; i<NR_DMA; i++)
	{
		if (!(dmatab[i].dt_flags & DTF_INUSE))
			break;
	}
	if (i >= NR_DMA)
	{
		printf("pm:do_adddma: dma table full\n");
		for (i= 0; i<NR_DMA; i++)
		{
			printf("%d: flags 0x%x proc %d base 0x%x size 0x%x\n",
				i, dmatab[i].dt_flags,
				dmatab[i].dt_proc,
				dmatab[i].dt_base,
				dmatab[i].dt_size);
		}
		panic(__FILE__, "adddma: table full", NO_NUM);
		return ENOSPC;
	}

	/* Find target process */
	if (pm_isokendpt(target_proc_e, &proc_n) != OK)
	{
		printf("pm:do_adddma: endpoint %d not found\n", target_proc_e);
		return EINVAL;
	}
	rmp= &mproc[proc_n];
	rmp->mp_flags |= HAS_DMA;

	dmatab[i].dt_flags= DTF_INUSE;
	dmatab[i].dt_proc= target_proc_e;
	dmatab[i].dt_base= base;
	dmatab[i].dt_size= size;

	return OK;
}

/*===========================================================================*
 *				do_deldma				     *
 *===========================================================================*/
PUBLIC int do_deldma()
{
	endpoint_t req_proc_e, target_proc_e;
	int i, j, proc_n;
	phys_bytes base, size;
	struct mproc *rmp;

	if (mp->mp_effuid != SUPER_USER)
		return EPERM;

	req_proc_e= m_in.m_source;
	target_proc_e= m_in.m2_i1;
	base= m_in.m2_l1;
	size= m_in.m2_l2;

	iommu_proc_e= req_proc_e;

	/* Find slot */
	for (i= 0; i<NR_DMA; i++)
	{
		if (!(dmatab[i].dt_flags & DTF_INUSE))
			continue;
		if (dmatab[i].dt_proc == target_proc_e &&
			dmatab[i].dt_base == base &&
			dmatab[i].dt_size == size)
		{
			break;
		}
	}
	if (i >= NR_DMA)
	{
		printf("pm:do_deldma: slot not found\n");
		return ESRCH;
	}

	if (dmatab[i].dt_flags & DTF_RELEASE_SEG)
	{
		/* Check if we have to release the segment */
		for (j= 0; j<NR_DMA; j++)
		{
			if (j == i)
				continue;
			if (!(dmatab[j].dt_flags & DTF_INUSE))
				continue;
			if (!(dmatab[j].dt_flags & DTF_RELEASE_SEG))
				continue;
			if (dmatab[i].dt_proc == target_proc_e)
				break;
		}
		if (j >= NR_DMA)
		{
			/* Last segment */
			free_mem(dmatab[i].dt_seg_base,
				dmatab[i].dt_seg_size);
		}
	}

	dmatab[i].dt_flags &= ~DTF_INUSE;

	return OK;
}

/*===========================================================================*
 *				do_getdma				     *
 *===========================================================================*/
PUBLIC int do_getdma()
{
	endpoint_t req_proc_e, target_proc_e;
	int i, proc_n;
	phys_bytes base, size;
	struct mproc *rmp;

	if (mp->mp_effuid != SUPER_USER)
		return EPERM;

	req_proc_e= m_in.m_source;
	iommu_proc_e= req_proc_e;

	/* Find slot to report */
	for (i= 0; i<NR_DMA; i++)
	{
		if (!(dmatab[i].dt_flags & DTF_INUSE))
			continue;
		if (!(dmatab[i].dt_flags & DTF_RELEASE_DMA))
			continue;

		printf("do_getdma: setting reply to 0x%x@0x%x proc %d\n",
			dmatab[i].dt_size, dmatab[i].dt_base,
			dmatab[i].dt_proc);
		mp->mp_reply.m2_i1= dmatab[i].dt_proc;
		mp->mp_reply.m2_l1= dmatab[i].dt_base;
		mp->mp_reply.m2_l2= dmatab[i].dt_size;

		return OK;
	}

	/* Nothing */
	return EAGAIN;
}



/*===========================================================================*
 *				release_dma				     *
 *===========================================================================*/
PUBLIC void release_dma(proc_e, base, size)
endpoint_t proc_e;
phys_clicks base;
phys_clicks size;
{
	int i, found_one;;

	found_one= FALSE;
	for (i= 0; i<NR_DMA; i++)
	{
		if (!(dmatab[i].dt_flags & DTF_INUSE))
			continue;
		if (dmatab[i].dt_proc != proc_e)
			continue;
		dmatab[i].dt_flags |= DTF_RELEASE_DMA | DTF_RELEASE_SEG;
		dmatab[i].dt_seg_base= base;
		dmatab[i].dt_seg_size= size;
		found_one= TRUE;
	}
	if (found_one)
		notify(iommu_proc_e);
	else
		free_mem(base, size);
}

#if ENABLE_SWAP
/*===========================================================================*
 *				swap_on					     *
 *===========================================================================*/
PUBLIC int swap_on(file, offset, size)
char *file;				/* file to swap on */
u32_t offset, size;			/* area on swap file to use */
{
/* Turn swapping on. */

  if (swap_fd != -1) return(EBUSY);	/* already have swap? */

  tell_fs(CHDIR, who_e, FALSE, 0);	/* be like the caller for open() */
  if ((swap_fd = open(file, O_RDWR)) < 0) return(-errno);
  swap_offset = offset;
  size >>= CLICK_SHIFT;
  if (size > swap_maxsize) size = swap_maxsize;
  if (size > 0) free_mem(swap_base, (phys_clicks) size);
  return(OK);
}

/*===========================================================================*
 *				swap_off				     *
 *===========================================================================*/
PUBLIC int swap_off()
{
/* Turn swapping off. */
  struct mproc *rmp;
  struct hole *hp, *prev_ptr;

  if (swap_fd == -1) return(OK);	/* can't turn off what isn't on */

  /* Put all swapped out processes on the inswap queue and swap in. */
  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
	if (rmp->mp_flags & ONSWAP) swap_inqueue(rmp);
  }
  swap_in();

  /* All in memory? */
  for (rmp = &mproc[0]; rmp < &mproc[NR_PROCS]; rmp++) {
	if (rmp->mp_flags & ONSWAP) return(ENOMEM);
  }

  /* Yes.  Remove the swap hole and close the swap file descriptor. */
  for (hp = hole_head; hp != NIL_HOLE; prev_ptr = hp, hp = hp->h_next) {
	if (hp->h_base >= swap_base) {
		del_slot(prev_ptr, hp);
		hp = hole_head;
	}
  }
  close(swap_fd);
  swap_fd = -1;
  return(OK);
}

/*===========================================================================*
 *				swap_inqueue				     *
 *===========================================================================*/
PUBLIC void swap_inqueue(rmp)
register struct mproc *rmp;		/* process to add to the queue */
{
/* Put a swapped out process on the queue of processes to be swapped in.  This
 * happens when such a process gets a signal, or if a reply message must be
 * sent, like when a process doing a wait() has a child that exits.
 */
  struct mproc **pmp;

  if (rmp->mp_flags & SWAPIN) return;	/* already queued */

  
  for (pmp = &in_queue; *pmp != NULL; pmp = &(*pmp)->mp_swapq) {}
  *pmp = rmp;
  rmp->mp_swapq = NULL;
  rmp->mp_flags |= SWAPIN;
}

/*===========================================================================*
 *				swap_in					     *
 *===========================================================================*/
PUBLIC void swap_in()
{
/* Try to swap in a process on the inswap queue.  We want to send it a message,
 * interrupt it, or something.
 */
  struct mproc **pmp, *rmp;
  phys_clicks old_base, new_base, size;
  off_t off;
  int proc_nr;

  pmp = &in_queue;
  while ((rmp = *pmp) != NULL) {
	proc_nr = (rmp - mproc);
	size = rmp->mp_seg[S].mem_vir + rmp->mp_seg[S].mem_len
		- rmp->mp_seg[D].mem_vir;

	if (!(rmp->mp_flags & SWAPIN)) {
		/* Guess it got killed.  (Queue is cleaned here.) */
		*pmp = rmp->mp_swapq;
		continue;
	} else
	if ((new_base = alloc_mem(size)) == NO_MEM) {
		/* No memory for this one, try the next. */
		pmp = &rmp->mp_swapq;
	} else {
		/* We've found memory.  Update map and swap in. */
		old_base = rmp->mp_seg[D].mem_phys;
		rmp->mp_seg[D].mem_phys = new_base;
		rmp->mp_seg[S].mem_phys = rmp->mp_seg[D].mem_phys + 
			(rmp->mp_seg[S].mem_vir - rmp->mp_seg[D].mem_vir);
		sys_newmap(rmp->mp_endpoint, rmp->mp_seg);
		off = swap_offset + ((off_t) (old_base-swap_base)<<CLICK_SHIFT);
		lseek(swap_fd, off, SEEK_SET);
		rw_seg(0, swap_fd, rmp->mp_endpoint, D, (phys_bytes)size << CLICK_SHIFT);
		free_mem(old_base, size);
		rmp->mp_flags &= ~(ONSWAP|SWAPIN);
		*pmp = rmp->mp_swapq;
		check_pending(rmp);	/* a signal may have waked this one */
	}
  }
}

/*===========================================================================*
 *				swap_out				     *
 *===========================================================================*/
PRIVATE int swap_out()
{
/* Try to find a process that can be swapped out.  Candidates are those blocked
 * on a system call that PM handles, like wait(), pause() or sigsuspend().
 */
  struct mproc *rmp;
  struct hole *hp, *prev_ptr;
  phys_clicks old_base, new_base, size;
  off_t off;
  int proc_nr;

  rmp = outswap;
  do {
	if (++rmp == &mproc[NR_PROCS]) rmp = &mproc[0];

	/* A candidate? */
	if (!(rmp->mp_flags & (PAUSED | WAITING | SIGSUSPENDED))) continue;

	/* Already on swap or otherwise to be avoided? */
	if (rmp->mp_flags & (DONT_SWAP | TRACED | REPLY | ONSWAP)) continue;

	/* Got one, find a swap hole and swap it out. */
	proc_nr = (rmp - mproc);
	size = rmp->mp_seg[S].mem_vir + rmp->mp_seg[S].mem_len
		- rmp->mp_seg[D].mem_vir;

	prev_ptr = NIL_HOLE;
	for (hp = hole_head; hp != NIL_HOLE; prev_ptr = hp, hp = hp->h_next) {
		if (hp->h_base >= swap_base && hp->h_len >= size) break;
	}
	if (hp == NIL_HOLE) continue;	/* oops, not enough swapspace */
	new_base = hp->h_base;
	hp->h_base += size;
	hp->h_len -= size;
	if (hp->h_len == 0) del_slot(prev_ptr, hp);

	off = swap_offset + ((off_t) (new_base - swap_base) << CLICK_SHIFT);
	lseek(swap_fd, off, SEEK_SET);
	rw_seg(1, swap_fd, rmp->mp_endpoint, D, (phys_bytes)size << CLICK_SHIFT);
	old_base = rmp->mp_seg[D].mem_phys;
	rmp->mp_seg[D].mem_phys = new_base;
	rmp->mp_seg[S].mem_phys = rmp->mp_seg[D].mem_phys + 
		(rmp->mp_seg[S].mem_vir - rmp->mp_seg[D].mem_vir);
	sys_newmap(rmp->mp_endpoint, rmp->mp_seg);
	free_mem(old_base, size);
	rmp->mp_flags |= ONSWAP;

	outswap = rmp;		/* next time start here */
	return(TRUE);
  } while (rmp != outswap);

  return(FALSE);	/* no candidate found */
}
#endif /* SWAP */
