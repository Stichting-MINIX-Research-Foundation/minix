/* This file is concerned with allocating and freeing arbitrary-size blocks of
 * physical memory.
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
#include "memlist.h"

/* AVL tree of free pages. */
addr_avl addravl;

/* Used for sanity check. */
static phys_bytes mem_low, mem_high;

static void free_pages(phys_bytes addr, int pages);
static phys_bytes alloc_pages(int pages, int flags, phys_bytes *ret);

#if SANITYCHECKS
#define PAGESPERGB (1024*1024*1024/VM_PAGE_SIZE) /* 1GB of memory */
#define MAXPAGES (2*PAGESPERGB)
#define CHUNKS BITMAP_CHUNKS(MAXPAGES)
static bitchunk_t pagemap[CHUNKS];
#endif

/*===========================================================================*
 *				alloc_mem				     *
 *===========================================================================*/
phys_clicks alloc_mem(phys_clicks clicks, u32_t memflags)
{
/* Allocate a block of memory from the free list using first fit. The block
 * consists of a sequence of contiguous bytes, whose length in clicks is
 * given by 'clicks'.  A pointer to the block is returned.  The block is
 * always on a click boundary.  This procedure is called when memory is
 * needed for FORK or EXEC.
 */
  phys_clicks mem = NO_MEM, align_clicks = 0;

  if(memflags & PAF_ALIGN64K) {
  	align_clicks = (64 * 1024) / CLICK_SIZE;
	clicks += align_clicks;
  } else if(memflags & PAF_ALIGN16K) {
	align_clicks = (16 * 1024) / CLICK_SIZE;
	clicks += align_clicks;
  }

  mem = alloc_pages(clicks, memflags, NULL);
  if(mem == NO_MEM) {
    free_yielded(clicks * CLICK_SIZE);
    mem = alloc_pages(clicks, memflags, NULL);
  }

  if(mem == NO_MEM)
  	return mem;

  if(align_clicks) {
  	phys_clicks o;
  	o = mem % align_clicks;
  	if(o > 0) {
  		phys_clicks e;
  		e = align_clicks - o;
	  	free_mem(mem, e);
	  	mem += e;
	}
  }

  return mem;
}

/*===========================================================================*
 *				free_mem				     *
 *===========================================================================*/
void free_mem(phys_clicks base, phys_clicks clicks)
{
/* Return a block of free memory to the hole list.  The parameters tell where
 * the block starts in physical memory and how big it is.  The block is added
 * to the hole list.  If it is contiguous with an existing hole on either end,
 * it is merged with the hole or holes.
 */
  if (clicks == 0) return;

  assert(CLICK_SIZE == VM_PAGE_SIZE);
  free_pages(base, clicks);
  return;
}

/*===========================================================================*
 *				mem_init				     *
 *===========================================================================*/
void mem_init(chunks)
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

  addr_init(&addravl);

  total_pages = 0;

  /* Use the chunks of physical memory to allocate holes. */
  for (i=NR_MEMS-1; i>=0; i--) {
  	if (chunks[i].size > 0) {
		phys_bytes from = CLICK2ABS(chunks[i].base),
			to = CLICK2ABS(chunks[i].base+chunks[i].size)-1;
		if(first || from < mem_low) mem_low = from;
		if(first || to > mem_high) mem_high = to;
		free_mem(chunks[i].base, chunks[i].size);
		total_pages += chunks[i].size;
		first = 0;
	}
  }
}

#if SANITYCHECKS
void mem_sanitycheck(char *file, int line)
{
	pagerange_t *p, *prevp = NULL;
	addr_iter iter;
	addr_start_iter_least(&addravl, &iter);
	while((p=addr_get_iter(&iter))) {
		SLABSANE(p);
		assert(p->size > 0);
		if(prevp) {
			assert(prevp->addr < p->addr);
			assert(prevp->addr + prevp->size < p->addr);
		}
		usedpages_add(p->addr * VM_PAGE_SIZE, p->size * VM_PAGE_SIZE);
		prevp = p;
		addr_incr_iter(&iter);
	}
}
#endif

void memstats(int *nodes, int *pages, int *largest)
{
	pagerange_t *p;
	addr_iter iter;
	addr_start_iter_least(&addravl, &iter);
	*nodes = 0;
	*pages = 0;
	*largest = 0;

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
static phys_bytes alloc_pages(int pages, int memflags, phys_bytes *len)
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

#if NONCONTIGUOUS
	/* If NONCONTIGUOUS is on, allocate physical pages single
	 * pages at a time, accomplished by returning single pages
	 * if the caller can handle that (indicated by PAF_FIRSTBLOCK).
	 */
	if(memflags & PAF_FIRSTBLOCK) {
		assert(!(memflags & PAF_CONTIG));
		pages = 1;
	}
#endif

	memstats(&firstnodes, &firstpages, &largest);
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
		assert(pr->size > 0);
		if(pr->size >= pages || (memflags & PAF_FIRSTBLOCK)) {
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
		if(len)
			*len = 0;
#if SANITYCHECKS
		assert(largest < pages);
#endif
	   return NO_MEM;
	}

	SLABSANE(pr);

	if(memflags & PAF_FIRSTBLOCK) {
		assert(len);
		/* block doesn't have to as big as requested;
		 * return its size though.
		 */
		if(pr->size < pages) {
			pages = pr->size;
#if SANITYCHECKS
			wantpages = firstpages - pages;
#endif
		}
	}

	if(len)
		*len = pages;

	/* Allocated chunk is off the end. */
	mem = pr->addr + pr->size - pages;

	assert(pr->size >= pages);
	if(pr->size == pages) {
		pagerange_t *prr;
		prr = addr_remove(&addravl, pr->addr);
		assert(prr);
		assert(prr == pr);
		SLABFREE(pr);
#if SANITYCHECKS
		wantnodes--;
#endif
	} else {
		USE(pr, pr->size -= pages;);
	}

	if(memflags & PAF_CLEAR) {
		int s;
		if ((s= sys_memset(NONE, 0, CLICK_SIZE*mem,
			VM_PAGE_SIZE*pages)) != OK) 
			panic("alloc_mem: sys_memset failed: %d", s);
	}

#if SANITYCHECKS
	memstats(&finalnodes, &finalpages, &largest);

	assert(finalnodes == wantnodes);
	assert(finalpages == wantpages);
#endif

	return mem;
}

/*===========================================================================*
 *				free_pages				     *
 *===========================================================================*/
static void free_pages(phys_bytes pageno, int npages)
{
	pagerange_t *pr, *p;
	addr_iter iter;
#if SANITYCHECKS
	int firstnodes, firstpages, wantnodes, wantpages;
	int finalnodes, finalpages, largest;

	memstats(&firstnodes, &firstpages, &largest);

	wantnodes = firstnodes;
	wantpages = firstpages + npages;
#endif

	assert(!addr_search(&addravl, pageno, AVL_EQUAL));

#if JUNKFREE
       if(sys_memset(NONE, 0xa5a5a5a5, VM_PAGE_SIZE * pageno,
               VM_PAGE_SIZE * npages) != OK)
                       panic("free_pages: sys_memset failed");
#endif

	/* try to merge with higher neighbour */
	if((pr=addr_search(&addravl, pageno+npages, AVL_EQUAL))) {
		USE(pr, pr->addr -= npages;
			pr->size += npages;);
	} else {
		if(!SLABALLOC(pr))
			panic("alloc_pages: can't alloc");
#if SANITYCHECKS
		memstats(&firstnodes, &firstpages, &largest);

		wantnodes = firstnodes;
		wantpages = firstpages + npages;

#endif
		assert(npages > 0);
		USE(pr, pr->addr = pageno;
			 pr->size = npages;);
		addr_insert(&addravl, pr);
#if SANITYCHECKS
		wantnodes++;
#endif
	}

	addr_start_iter(&addravl, &iter, pr->addr, AVL_EQUAL);
	p = addr_get_iter(&iter);
	assert(p);
	assert(p == pr);

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

	assert(finalnodes == wantnodes);
	assert(finalpages == wantpages);
#endif
}

#define NR_DMA	16

static struct dmatab
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
int do_adddma(message *msg)
{
	endpoint_t target_proc_e;
	int i, proc_n;
	phys_bytes base, size;
	struct vmproc *vmp;

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
			printf("%d: flags 0x%x proc %d base 0x%lx size 0x%lx\n",
				i, dmatab[i].dt_flags,
				dmatab[i].dt_proc,
				dmatab[i].dt_base,
				dmatab[i].dt_size);
		}
		panic("adddma: table full");
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
int do_deldma(message *msg)
{
	endpoint_t target_proc_e;
	int i, j;
	phys_bytes base, size;

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
int do_getdma(message *msg)
{
	int i;

	/* Find slot to report */
	for (i= 0; i<NR_DMA; i++)
	{
		if (!(dmatab[i].dt_flags & DTF_INUSE))
			continue;
		if (!(dmatab[i].dt_flags & DTF_RELEASE_DMA))
			continue;

		printf("do_getdma: setting reply to 0x%lx@0x%lx proc %d\n",
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
void release_dma(struct vmproc *vmp)
{
#if 0
	int i, found_one;

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
		free_mem(base, size);

	msg->VMRD_FOUND = found_one;
#else
	panic("release_dma not done");
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
        printf("%d blocks, %d pages (%lukB) free, largest %d pages (%lukB)\n",
                nodes, pages, (unsigned long) pages * (VM_PAGE_SIZE/1024),
		largest, (unsigned long) largest * (VM_PAGE_SIZE/1024));
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
	u32_t pagestart, pages;

	if(!incheck)
		return OK;

	assert(!(addr % VM_PAGE_SIZE));
	assert(!(len % VM_PAGE_SIZE));
	assert(len > 0);

	pagestart = addr / VM_PAGE_SIZE;
	pages = len / VM_PAGE_SIZE;

	while(pages > 0) {
		phys_bytes thisaddr;
		assert(pagestart > 0);
		assert(pagestart < MAXPAGES);
		thisaddr = pagestart * VM_PAGE_SIZE;
		if(GET_BIT(pagemap, pagestart)) {
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

/*===========================================================================*
 *				alloc_mem_in_list			     *
 *===========================================================================*/
struct memlist *alloc_mem_in_list(phys_bytes bytes, u32_t flags)
{
	phys_bytes rempages;
	struct memlist *head = NULL, *tail = NULL;

	assert(bytes > 0);
	assert(!(bytes % VM_PAGE_SIZE));

	rempages = bytes / VM_PAGE_SIZE;

	/* unless we are told to allocate all memory
	 * contiguously, tell alloc function to grab whatever
	 * block it can find.
	 */
	if(!(flags & PAF_CONTIG))
		flags |= PAF_FIRSTBLOCK;

	do {
		struct memlist *ml;
		phys_bytes mem, gotpages;
		vir_bytes freed = 0;

		do {
			mem = alloc_pages(rempages, flags, &gotpages);

			if(mem == NO_MEM) {
				freed = free_yielded(rempages * VM_PAGE_SIZE);
			}
		} while(mem == NO_MEM && freed > 0);

		if(mem == NO_MEM) {
			printf("alloc_mem_in_list: giving up, %lukB missing\n",
				rempages * VM_PAGE_SIZE/1024);
			printmemstats();
			free_mem_list(head, 1);
			return NULL;
		}

		assert(gotpages <= rempages);
		assert(gotpages > 0);

		if(!(SLABALLOC(ml))) {
			free_mem_list(head, 1);
			free_pages(mem, gotpages);
			return NULL;
		}

		USE(ml,
			ml->phys = CLICK2ABS(mem);
			ml->length = CLICK2ABS(gotpages);
			ml->next = NULL;);
		if(tail) {
			USE(tail,
				tail->next = ml;);
		}
		tail = ml;
		if(!head)
			head = ml;
		rempages -= gotpages;
	} while(rempages > 0);

    {
	struct memlist *ml;
	for(ml = head; ml; ml = ml->next) {
		assert(ml->phys);
		assert(ml->length);
#if NONCONTIGUOUS
		if(!(flags & PAF_CONTIG)) {
			assert(ml->length == VM_PAGE_SIZE);
			if(ml->next)
				assert(ml->phys + ml->length != ml->next->phys);
		}
#endif
	}
    }

	return head;
}

/*===========================================================================*
 *				free_mem_list			     	     *
 *===========================================================================*/
void free_mem_list(struct memlist *list, int all)
{
	while(list) {
		struct memlist *next;
		next = list->next;
		assert(!(list->phys % VM_PAGE_SIZE));
		assert(!(list->length % VM_PAGE_SIZE));
		if(all)
			free_pages(list->phys / VM_PAGE_SIZE,
			list->length / VM_PAGE_SIZE);
		SLABFREE(list);
		list = next;
	}
}

/*===========================================================================*
 *				print_mem_list			     	     *
 *===========================================================================*/
void print_mem_list(struct memlist *list)
{
	while(list) {
		assert(list->length > 0);
		printf("0x%lx-0x%lx", list->phys, list->phys+list->length-1);
		printf(" ");
		list = list->next;
	}
	printf("\n");
}

