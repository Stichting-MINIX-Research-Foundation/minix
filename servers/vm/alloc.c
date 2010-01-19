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
 */

#define _SYSTEM 1

#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/debug.h>
#include <minix/bitmap.h>

#include <sys/mman.h>

#include <limits.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <memory.h>

#include "vm.h"
#include "proto.h"
#include "util.h"
#include "glo.h"
#include "pagerange.h"
#include "addravl.h"
#include "sanitycheck.h"

/* AVL tree of free pages. */
addr_avl addravl;

/* Used for sanity check. */
PRIVATE phys_bytes mem_low, mem_high;
#define vm_assert_range(addr, len)  			\
	vm_assert((addr) >= mem_low);			\
	vm_assert((addr) + (len) - 1 <= mem_high);

struct hole {
	struct hole *h_next;          /* pointer to next entry on the list */
	phys_clicks h_base;           /* where does the hole begin? */
	phys_clicks h_len;            /* how big is the hole? */
	int freelist;
	int holelist;
};

static int startpages;

#define NIL_HOLE (struct hole *) 0

#define _NR_HOLES (_NR_PROCS*2)  /* No. of memory holes maintained by VM */

PRIVATE struct hole hole[_NR_HOLES];

PRIVATE struct hole *hole_head;	/* pointer to first hole */
PRIVATE struct hole *free_slots;/* ptr to list of unused table slots */

FORWARD _PROTOTYPE( void del_slot, (struct hole *prev_ptr, struct hole *hp) );
FORWARD _PROTOTYPE( void merge, (struct hole *hp)			    );
FORWARD _PROTOTYPE( void free_pages, (phys_bytes addr, int pages)	    );
FORWARD _PROTOTYPE( phys_bytes alloc_pages, (int pages, int flags)	    );

#if SANITYCHECKS
FORWARD _PROTOTYPE( void holes_sanity_f, (char *fn, int line)		    );
#define CHECKHOLES holes_sanity_f(__FILE__, __LINE__)

#define MAXPAGES (1024*1024*1024/VM_PAGE_SIZE) /* 1GB of memory */
#define CHUNKS BITMAP_CHUNKS(MAXPAGES)
PRIVATE bitchunk_t pagemap[CHUNKS];

#else
#define CHECKHOLES 
#endif

/* Sanity check for parameters of node p. */
#define vm_assert_params(p, bytes, next) { \
	vm_assert((p) != NO_MEM);	\
	vm_assert(!((bytes) % VM_PAGE_SIZE)); \
	vm_assert(!((next) % VM_PAGE_SIZE)); \
	vm_assert((bytes) > 0); \
	vm_assert((p) + (bytes) > (p)); \
	vm_assert((next) == NO_MEM || ((p) + (bytes) <= (next))); \
	vm_assert_range((p), (bytes)); \
	vm_assert_range((next), 1); \
}

/* Retrieve size of free block and pointer to next block from physical
 * address (page) p.
 */
#define GET_PARAMS(p, bytes, next) { \
	phys_readaddr((p), &(bytes), &(next));	\
	vm_assert_params((p), (bytes), (next)); \
}

/* Write parameters to physical page p. */
#define SET_PARAMS(p, bytes, next) { \
	vm_assert_params((p), (bytes), (next)); \
	phys_writeaddr((p), (bytes), (next));	\
}


#if SANITYCHECKS

/*===========================================================================*
 *				holes_sanity_f				     *
 *===========================================================================*/
PRIVATE void holes_sanity_f(file, line)
char *file;
int line;
{
#define myassert(c) { \
  if(!(c)) { \
	printf("holes_sanity_f:%s:%d: %s failed\n", file, line, #c); \
	util_stacktrace();	\
	vm_panic("assert failed.", NO_NUM); } \
  }	

	int h, c = 0, n = 0;
	struct hole *hp;

	/* Reset flags */
	for(h = 0; h < _NR_HOLES; h++) {
		hole[h].freelist = 0;
		hole[h].holelist = 0;
	}

	/* Mark all holes on freelist. */
	for(hp = free_slots; hp; hp = hp->h_next) {
		myassert(!hp->freelist);
		myassert(!hp->holelist);
		hp->freelist = 1;
		myassert(c < _NR_HOLES);
		c++;
		n++;
	}

	/* Mark all holes on holelist. */
	c = 0;
	for(hp = hole_head; hp; hp = hp->h_next) {
		myassert(!hp->freelist);
		myassert(!hp->holelist);
		hp->holelist = 1;
		myassert(c < _NR_HOLES);
		c++;
		n++;
	}

	/* Check there are exactly the right number of nodes. */
	myassert(n == _NR_HOLES);

	/* Make sure each slot is on exactly one of the list. */
	c = 0;
	for(h = 0; h < _NR_HOLES; h++) {
		hp = &hole[h];
		myassert(hp->holelist || hp->freelist);
		myassert(!(hp->holelist && hp->freelist));
		myassert(c < _NR_HOLES);
		c++;
	}

	/* Make sure no holes overlap. */
	for(hp = hole_head; hp && hp->h_next; hp = hp->h_next) {
		myassert(hp->holelist);
		hp->holelist = 1;
		/* No holes overlap. */
		myassert(hp->h_base + hp->h_len <= hp->h_next->h_base);

		/* No uncoalesced holes. */
		myassert(hp->h_base + hp->h_len < hp->h_next->h_base);
	}
}
#endif

/*===========================================================================*
 *				alloc_mem_f				     *
 *===========================================================================*/
PUBLIC phys_clicks alloc_mem_f(phys_clicks clicks, u32_t memflags)
{
/* Allocate a block of memory from the free list using first fit. The block
 * consists of a sequence of contiguous bytes, whose length in clicks is
 * given by 'clicks'.  A pointer to the block is returned.  The block is
 * always on a click boundary.  This procedure is called when memory is
 * needed for FORK or EXEC.
 */
  register struct hole *hp, *prev_ptr;
  phys_clicks old_base, mem = NO_MEM, align_clicks = 0;
  int s;

  if(memflags & PAF_ALIGN64K) {
  	align_clicks = (64 * 1024) / CLICK_SIZE;
	clicks += align_clicks;
  }

  if(vm_paged) {
	vm_assert(CLICK_SIZE == VM_PAGE_SIZE);
	mem = alloc_pages(clicks, memflags);
  } else {
CHECKHOLES;
        prev_ptr = NIL_HOLE;
	hp = hole_head;
	while (hp != NIL_HOLE) {
		if (hp->h_len >= clicks) {
			/* We found a hole that is big enough.  Use it. */
			old_base = hp->h_base;	/* remember where it started */
			hp->h_base += clicks;	/* bite a piece off */
			hp->h_len -= clicks;	/* ditto */

			/* Delete the hole if used up completely. */
			if (hp->h_len == 0) del_slot(prev_ptr, hp);

			/* Anything special needs to happen? */
			if(memflags & PAF_CLEAR) {
			  if ((s= sys_memset(0, CLICK_SIZE*old_base,
				CLICK_SIZE*clicks)) != OK)   {
				vm_panic("alloc_mem: sys_memset failed", s);
			  }
			}

			/* Return the start address of the acquired block. */
CHECKHOLES;
			mem = old_base;
			break;
		}

		prev_ptr = hp;
		hp = hp->h_next;
	}
  }

  if(mem == NO_MEM)
  	return mem;

CHECKHOLES;

  if(align_clicks) {
  	phys_clicks o;
  	o = mem % align_clicks;
  	if(o > 0) {
  		phys_clicks e;
  		e = align_clicks - o;
	  	FREE_MEM(mem, e);
	  	mem += e;
	}
  }
CHECKHOLES;

  return mem;
}

/*===========================================================================*
 *				free_mem_f				     *
 *===========================================================================*/
PUBLIC void free_mem_f(phys_clicks base, phys_clicks clicks)
{
/* Return a block of free memory to the hole list.  The parameters tell where
 * the block starts in physical memory and how big it is.  The block is added
 * to the hole list.  If it is contiguous with an existing hole on either end,
 * it is merged with the hole or holes.
 */
  register struct hole *hp, *new_ptr, *prev_ptr;
CHECKHOLES;

  if (clicks == 0) return;

  if(vm_paged) {
	vm_assert(CLICK_SIZE == VM_PAGE_SIZE);
	free_pages(base, clicks);
	return;
  }

  if ( (new_ptr = free_slots) == NIL_HOLE) 
  	vm_panic("hole table full", NO_NUM);
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
CHECKHOLES;
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
CHECKHOLES;
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
PUBLIC void mem_init(chunks)
struct memory *chunks;		/* list of free memory chunks */
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
  int i, first = 0;
  register struct hole *hp;
  int nodes, largest;

  /* Put all holes on the free list. */
  for (hp = &hole[0]; hp < &hole[_NR_HOLES]; hp++) {
	hp->h_next = hp + 1;
	hp->h_base = hp->h_len = 0;
  }
  hole[_NR_HOLES-1].h_next = NIL_HOLE;
  hole_head = NIL_HOLE;
  free_slots = &hole[0];

  addr_init(&addravl);

  total_pages = 0;

  /* Use the chunks of physical memory to allocate holes. */
  for (i=NR_MEMS-1; i>=0; i--) {
  	if (chunks[i].size > 0) {
		phys_bytes from = CLICK2ABS(chunks[i].base),
			to = CLICK2ABS(chunks[i].base+chunks[i].size)-1;
		if(first || from < mem_low) mem_low = from;
		if(first || to > mem_high) mem_high = to;
		FREE_MEM(chunks[i].base, chunks[i].size);
		total_pages += chunks[i].size;
		first = 0;
	}
  }

  CHECKHOLES;
}

#if SANITYCHECKS
PRIVATE void sanitycheck(void)
{
	pagerange_t *p, *prevp = NULL;
	addr_iter iter;
	addr_start_iter_least(&addravl, &iter);
	while((p=addr_get_iter(&iter))) {
		SLABSANE(p);
		vm_assert(p->size > 0);
		if(prevp) {
			vm_assert(prevp->addr < p->addr);
			vm_assert(prevp->addr + p->addr < p->addr);
		}
		addr_incr_iter(&iter);
	}
}
#endif

PUBLIC void memstats(int *nodes, int *pages, int *largest)
{
	pagerange_t *p, *prevp = NULL;
	addr_iter iter;
	addr_start_iter_least(&addravl, &iter);
	*nodes = 0;
	*pages = 0;
	*largest = 0;
#if SANITYCHECKS
	sanitycheck();
#endif
	while((p=addr_get_iter(&iter))) {
		SLABSANE(p);
		(*nodes)++;
		(*pages)+= p->size;
		if(p->size > *largest)
			*largest = p->size;
		addr_incr_iter(&iter);
	}
}

/*===========================================================================*
 *				alloc_pages				     *
 *===========================================================================*/
PRIVATE PUBLIC phys_bytes alloc_pages(int pages, int memflags)
{
	addr_iter iter;
	pagerange_t *pr;
	int incr;
	phys_bytes boundary16 = 16 * 1024 * 1024 / VM_PAGE_SIZE;
	phys_bytes boundary1  =  1 * 1024 * 1024 / VM_PAGE_SIZE;
	phys_bytes mem;
#if SANITYCHECKS
	int firstnodes, firstpages, wantnodes, wantpages;
	int finalnodes, finalpages;
	int largest;

	memstats(&firstnodes, &firstpages, &largest);
	sanitycheck();
	wantnodes = firstnodes;
	wantpages = firstpages - pages;
#endif

	if(memflags & (PAF_LOWER16MB|PAF_LOWER1MB)) {
		addr_start_iter_least(&addravl, &iter);
		incr = 1;
	} else {
		addr_start_iter_greatest(&addravl, &iter);
		incr = 0;
	}

	while((pr = addr_get_iter(&iter))) {
		SLABSANE(pr);
		if(pr->size >= pages) {
			if(memflags & PAF_LOWER16MB) {
				if(pr->addr + pages > boundary16)
					return NO_MEM;
			}

			if(memflags & PAF_LOWER1MB) {
				if(pr->addr + pages > boundary1)
					return NO_MEM;
			}

			/* good block found! */
			break;
		}
		if(incr)
			addr_incr_iter(&iter);
		else
			addr_decr_iter(&iter);
	}

	if(!pr) {
		printf("VM: alloc_pages: alloc failed of %d pages\n", pages);
		util_stacktrace();
		printmemstats();
#if SANITYCHECKS
		if(largest >= pages) {
			vm_panic("no memory but largest was enough", NO_NUM);
		}
#endif
		return NO_MEM;
	}

	SLABSANE(pr);

	/* Allocated chunk is off the end. */
	mem = pr->addr + pr->size - pages;

	vm_assert(pr->size >= pages);
	if(pr->size == pages) {
		pagerange_t *prr;
		prr = addr_remove(&addravl, pr->addr);
		vm_assert(prr);
		vm_assert(prr == pr);
		SLABFREE(pr);
#if SANITYCHECKS
		wantnodes--;
#endif
	} else {
		USE(pr, pr->size -= pages;);
	}

	if(memflags & PAF_CLEAR) {
		int s;
		if ((s= sys_memset(0, CLICK_SIZE*mem,
			VM_PAGE_SIZE*pages)) != OK) 
			vm_panic("alloc_mem: sys_memset failed", s);
	}

#if SANITYCHECKS
	memstats(&finalnodes, &finalpages, &largest);
	sanitycheck();

	vm_assert(finalnodes == wantnodes);
	vm_assert(finalpages == wantpages);
#endif

	return mem;
}

/*===========================================================================*
 *				free_pages				     *
 *===========================================================================*/
PRIVATE void free_pages(phys_bytes pageno, int npages)
{
	pagerange_t *pr, *p;
	addr_iter iter;
#if SANITYCHECKS
	int firstnodes, firstpages, wantnodes, wantpages;
	int finalnodes, finalpages, largest;

	memstats(&firstnodes, &firstpages, &largest);
	sanitycheck();

	wantnodes = firstnodes;
	wantpages = firstpages + npages;
#endif

	vm_assert(!addr_search(&addravl, pageno, AVL_EQUAL));

	/* try to merge with higher neighbour */
	if((pr=addr_search(&addravl, pageno+npages, AVL_EQUAL))) {
		USE(pr, pr->addr -= npages;
			pr->size += npages;);
	} else {
		if(!SLABALLOC(pr))
			vm_panic("alloc_pages: can't alloc", NO_NUM);
#if SANITYCHECKS
		memstats(&firstnodes, &firstpages, &largest);

		wantnodes = firstnodes;
		wantpages = firstpages + npages;

		sanitycheck();
#endif
		vm_assert(npages > 0);
		USE(pr, pr->addr = pageno;
			 pr->size = npages;);
		addr_insert(&addravl, pr);
#if SANITYCHECKS
		wantnodes++;
#endif
	}

	addr_start_iter(&addravl, &iter, pr->addr, AVL_EQUAL);
	p = addr_get_iter(&iter);
	vm_assert(p);
	vm_assert(p == pr);

	addr_decr_iter(&iter);
	if((p = addr_get_iter(&iter))) {
		SLABSANE(p);
		if(p->addr + p->size == pr->addr) {
			USE(p, p->size += pr->size;);
			addr_remove(&addravl, pr->addr);
			SLABFREE(pr);
#if SANITYCHECKS
			wantnodes--;
#endif
		}
	}


#if SANITYCHECKS
	memstats(&finalnodes, &finalpages,  &largest);
	sanitycheck();

	vm_assert(finalnodes == wantnodes);
	vm_assert(finalpages == wantpages);
#endif
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

/*===========================================================================*
 *				do_adddma				     *
 *===========================================================================*/
PUBLIC int do_adddma(message *msg)
{
	endpoint_t req_proc_e, target_proc_e;
	int i, proc_n;
	phys_bytes base, size;
	struct vmproc *vmp;

	req_proc_e= msg->VMAD_REQ;
	target_proc_e= msg->VMAD_EP;
	base= msg->VMAD_START;
	size= msg->VMAD_SIZE;

	/* Find empty slot */
	for (i= 0; i<NR_DMA; i++)
	{
		if (!(dmatab[i].dt_flags & DTF_INUSE))
			break;
	}
	if (i >= NR_DMA)
	{
		printf("vm:do_adddma: dma table full\n");
		for (i= 0; i<NR_DMA; i++)
		{
			printf("%d: flags 0x%x proc %d base 0x%x size 0x%x\n",
				i, dmatab[i].dt_flags,
				dmatab[i].dt_proc,
				dmatab[i].dt_base,
				dmatab[i].dt_size);
		}
		vm_panic("adddma: table full", NO_NUM);
		return ENOSPC;
	}

	/* Find target process */
	if (vm_isokendpt(target_proc_e, &proc_n) != OK)
	{
		printf("vm:do_adddma: endpoint %d not found\n", target_proc_e);
		return EINVAL;
	}
	vmp= &vmproc[proc_n];
	vmp->vm_flags |= VMF_HAS_DMA;

	dmatab[i].dt_flags= DTF_INUSE;
	dmatab[i].dt_proc= target_proc_e;
	dmatab[i].dt_base= base;
	dmatab[i].dt_size= size;

	return OK;
}

/*===========================================================================*
 *				do_deldma				     *
 *===========================================================================*/
PUBLIC int do_deldma(message *msg)
{
	endpoint_t req_proc_e, target_proc_e;
	int i, j, proc_n;
	phys_bytes base, size;
	struct vmproc *vmp;

	req_proc_e= msg->VMDD_REQ;
	target_proc_e= msg->VMDD_EP;
	base= msg->VMDD_START;
	size= msg->VMDD_SIZE;

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
		printf("vm:do_deldma: slot not found\n");
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
			FREE_MEM(dmatab[i].dt_seg_base,
				dmatab[i].dt_seg_size);
		}
	}

	dmatab[i].dt_flags &= ~DTF_INUSE;

	return OK;
}

/*===========================================================================*
 *				do_getdma				     *
 *===========================================================================*/
PUBLIC int do_getdma(message *msg)
{
	endpoint_t target_proc_e;
	int i, proc_n;
	phys_bytes base, size;
	struct vmproc *vmp;

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
		msg->VMGD_PROCP= dmatab[i].dt_proc;
		msg->VMGD_BASEP= dmatab[i].dt_base;
		msg->VMGD_SIZEP= dmatab[i].dt_size;

		return OK;
	}

	/* Nothing */
	return EAGAIN;
}



/*===========================================================================*
 *				release_dma				     *
 *===========================================================================*/
PUBLIC void release_dma(struct vmproc *vmp)
{
	int i, found_one;

	vm_panic("release_dma not done", NO_NUM);
#if 0

	found_one= FALSE;
	for (i= 0; i<NR_DMA; i++)
	{
		if (!(dmatab[i].dt_flags & DTF_INUSE))
			continue;
		if (dmatab[i].dt_proc != vmp->vm_endpoint)
			continue;
		dmatab[i].dt_flags |= DTF_RELEASE_DMA | DTF_RELEASE_SEG;
		dmatab[i].dt_seg_base= base;
		dmatab[i].dt_seg_size= size;
		found_one= TRUE;
	}

	if (!found_one)
		FREE_MEM(base, size);

	msg->VMRD_FOUND = found_one;
#endif

	return;
}

/*===========================================================================*
 *				printmemstats				     *
 *===========================================================================*/
void printmemstats(void)
{
	int nodes, pages, largest;
        memstats(&nodes, &pages, &largest);
        printf("%d blocks, %d pages (%ukB) free, largest %d pages (%ukB)\n",
                nodes, pages, (u32_t) pages * (VM_PAGE_SIZE/1024),
		largest, (u32_t) largest * (VM_PAGE_SIZE/1024));
}


#if SANITYCHECKS

/*===========================================================================*
 *				usedpages_reset				     *
 *===========================================================================*/
void usedpages_reset(void)
{
	memset(pagemap, 0, sizeof(pagemap));
}

/*===========================================================================*
 *				usedpages_add				     *
 *===========================================================================*/
int usedpages_add_f(phys_bytes addr, phys_bytes len, char *file, int line)
{
	pagerange_t *pr;
	u32_t pagestart, pages;

	if(!incheck)
		return OK;

	vm_assert(!(addr % VM_PAGE_SIZE));
	vm_assert(!(len % VM_PAGE_SIZE));
	vm_assert(len > 0);
	vm_assert_range(addr, len);

	pagestart = addr / VM_PAGE_SIZE;
	pages = len / VM_PAGE_SIZE;

	while(pages > 0) {
		phys_bytes thisaddr;
		vm_assert(pagestart > 0);
		vm_assert(pagestart < MAXPAGES);
		thisaddr = pagestart * VM_PAGE_SIZE;
		if(GET_BIT(pagemap, pagestart)) {
			int i;
			printf("%s:%d: usedpages_add: addr 0x%lx reused.\n",
				file, line, thisaddr);
			return EFAULT;
		}
		SET_BIT(pagemap, pagestart);
		pages--;
		pagestart++;
	}

	return OK;
}

#endif
