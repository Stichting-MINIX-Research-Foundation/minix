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
#include "sanitycheck.h"
#include "memlist.h"

/* Maximum amount of physical memory the VM allocator can track.  The free
 * bitmap is statically sized for this cap; mem_init() records how much RAM the
 * machine actually has in number_physical_pages (<= MAX_PHYSICAL_PAGES), and
 * every bitmap scan is bounded by that runtime value (so a machine with little
 * RAM does not scan the whole range).  On x86_64 the kernel identity-maps up to
 * 64 GB (PG_IDENT_PD_MAX in arch/x86_64/pg_utils.c), so VM can manage that
 * much; on i386 the physical address space is 4 GB. */
#if defined(__x86_64__)
#define MAX_PHYSICAL_BYTES	(64ULL * 1024 * 1024 * 1024)	/* 64 GB */
#else
#define MAX_PHYSICAL_BYTES	(0x100000000ULL)		/* 4 GB */
#endif
#define MAX_PHYSICAL_PAGES	((int)(MAX_PHYSICAL_BYTES / VM_PAGE_SIZE))
#define PAGE_BITMAP_CHUNKS BITMAP_CHUNKS(MAX_PHYSICAL_PAGES)
static bitchunk_t free_pages_bitmap[PAGE_BITMAP_CHUNKS];

/* Highest physical page actually present (+1), determined in mem_init() and
 * bounded by MAX_PHYSICAL_PAGES.  Upper bound for all free-bitmap scans. */
static int number_physical_pages = MAX_PHYSICAL_PAGES;
#define PAGE_CACHE_MAX 10000
static int free_page_cache[PAGE_CACHE_MAX];
static int free_page_cache_size = 0;

/* Number of physical pages currently free (tracks free_pages_bitmap). */
static int free_pages_cnt = 0;

/* Low-memory reserve: pages that ordinary user processes (PAF_USERMEM) may not
 * allocate.  They stay available for system services so that a user process
 * exhausting memory cannot make a service (e.g. VFS) fail a page fault and die,
 * which would be unrecoverable and panic the kernel. */
#define PAGES_RESERVED 1024	/* 4 MB */

/* Used for sanity check. */
static phys_bytes mem_low, mem_high;

static void free_pages(phys_bytes addr, int pages);
static phys_bytes alloc_pages(int pages, int flags);

#if SANITYCHECKS
/* The page map is a debug-only aid and would be enormous if sized for
 * MAX_PHYSICAL_PAGES on x86_64 (64 GB worth of entries -> hundreds of MB).
 * Keep it sized for the low 4 GB; pages above that are simply not tracked. */
#define SANITYCHECK_PHYSICAL_PAGES	((int)(0x100000000ULL / VM_PAGE_SIZE))
struct {
	int used;
	const char *file;
	int line;
} pagemap[SANITYCHECK_PHYSICAL_PAGES];
#endif

#define page_isfree(i) GET_BIT(free_pages_bitmap, i)

#define RESERVEDMAGIC		0x6e4c74d5
#define MAXRESERVEDPAGES	300
#define MAXRESERVEDQUEUES	 15

static struct reserved_pages {
	struct reserved_pages *next;	/* next in use */
	int max_available;	/* queue depth use, 0 if not in use at all */
	int npages;		/* number of consecutive pages */
	int mappedin;		/* must reserved pages also be mapped? */
	int n_available;	/* number of queue entries */
	int allocflags;		/* allocflags for alloc_mem */
	struct reserved_pageslot {
		phys_bytes	phys;
		void		*vir;
	} slots[MAXRESERVEDPAGES];
	u32_t magic;
} reservedqueues[MAXRESERVEDQUEUES], *first_reserved_inuse = NULL;

int missing_spares = 0;

static void sanitycheck_queues(void)
{
	struct reserved_pages *mrq;
	int m = 0;

	for(mrq = first_reserved_inuse; mrq; mrq = mrq->next) {
		assert(mrq->max_available > 0);
		assert(mrq->max_available >= mrq->n_available);
		m += mrq->max_available - mrq->n_available;
	}

	assert(m == missing_spares);
}

static void sanitycheck_rq(struct reserved_pages *rq)
{
	assert(rq->magic == RESERVEDMAGIC);
	assert(rq->n_available >= 0);
	assert(rq->n_available <= MAXRESERVEDPAGES);
	assert(rq->n_available <= rq->max_available);

	sanitycheck_queues();
}

void *reservedqueue_new(int max_available, int npages, int mapped, int allocflags)
{
	int r;
	struct reserved_pages *rq;

	assert(max_available > 0);
	assert(max_available < MAXRESERVEDPAGES);
	assert(npages > 0);
	assert(npages < 10);

	for(r = 0; r < MAXRESERVEDQUEUES; r++)
		if(!reservedqueues[r].max_available)
			break;

	if(r >= MAXRESERVEDQUEUES) {
		printf("VM: %d reserved queues in use\n", MAXRESERVEDQUEUES);
		return NULL;
	}

	rq = &reservedqueues[r];

	memset(rq, 0, sizeof(*rq));
	rq->next = first_reserved_inuse;
	first_reserved_inuse = rq;

	rq->max_available = max_available;
	rq->npages = npages;
	rq->mappedin = mapped;
	rq->allocflags = allocflags;
	rq->magic = RESERVEDMAGIC;

	missing_spares += max_available;

	return rq;
}

static void reservedqueue_fillslot(struct reserved_pages *rq,
	struct reserved_pageslot *rps, phys_bytes ph, void *vir)
{
	rps->phys = ph;
	rps->vir = vir;
	assert(missing_spares > 0);
	if(rq->mappedin) assert(vir);
	missing_spares--;
	rq->n_available++;
}

static int reservedqueue_addslot(struct reserved_pages *rq)
{
	phys_bytes cl, cl_addr;
	void *vir;
	struct reserved_pageslot *rps;

	sanitycheck_rq(rq);

	if((cl = alloc_mem(rq->npages, rq->allocflags)) == NO_MEM)
		return ENOMEM;

	cl_addr = CLICK2ABS(cl);

	vir = NULL;

	if(rq->mappedin) {
		if(!(vir = vm_mappages(cl_addr, rq->npages))) {
			free_mem(cl, rq->npages);
			printf("reservedqueue_addslot: vm_mappages failed\n");
			return ENOMEM;
		}
	}

	rps = &rq->slots[rq->n_available];

	reservedqueue_fillslot(rq, rps, cl_addr, vir);

	return OK;
}

void reservedqueue_add(void *rq_v, void *vir, phys_bytes ph)
{
	struct reserved_pages *rq = rq_v;
	struct reserved_pageslot *rps;

	sanitycheck_rq(rq);

	rps = &rq->slots[rq->n_available];

	reservedqueue_fillslot(rq, rps, ph, vir);
}

static int reservedqueue_fill(void *rq_v)
{
	struct reserved_pages *rq = rq_v;
	int r;

	sanitycheck_rq(rq);

	while(rq->n_available < rq->max_available)
		if((r=reservedqueue_addslot(rq)) != OK)
			return r;

	return OK;
}

int reservedqueue_alloc(void *rq_v, phys_bytes *ph, void **vir)
{
	struct reserved_pages *rq = rq_v;
	struct reserved_pageslot *rps;

	sanitycheck_rq(rq);

	if(rq->n_available < 1) return ENOMEM;

	rq->n_available--;
	missing_spares++;
	rps = &rq->slots[rq->n_available];

	*ph = rps->phys;
	*vir = rps->vir;

	sanitycheck_rq(rq);

	return OK;
}

void alloc_cycle(void)
{
	struct reserved_pages *rq;
	sanitycheck_queues();
	for(rq = first_reserved_inuse; rq && missing_spares > 0; rq = rq->next) {
		sanitycheck_rq(rq);
		reservedqueue_fill(rq);
		sanitycheck_rq(rq);
	}
	sanitycheck_queues();
}

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
  phys_clicks mem = NO_MEM, align_clicks = 0, wanted = clicks;

  if(memflags & PAF_ALIGN64K) {
  	align_clicks = (64 * 1024) / CLICK_SIZE;
	clicks += align_clicks;
  } else if(memflags & PAF_ALIGN16K) {
	align_clicks = (16 * 1024) / CLICK_SIZE;
	clicks += align_clicks;
  }

  do {
	mem = alloc_pages(clicks, memflags);
  } while(mem == NO_MEM && cache_freepages(clicks) > 0);

  if(mem == NO_MEM)
  	return mem;

  if(align_clicks) {
	phys_clicks head, tail;

	/*
	 * An aligned request is served by taking align_clicks more than was
	 * asked for and placing the result inside it.  Every page of that
	 * headroom which the alignment did not need has to be given back
	 * here, at both ends: the caller is told only where its own pages
	 * begin and how many it asked for, so anything else is memory
	 * nothing can ever free again.  Returning the pages below the
	 * aligned start but not the ones above the end leaked the rest of
	 * the 64 kB on every such allocation.
	 */
	head = (align_clicks - (mem % align_clicks)) % align_clicks;
	if(head > 0) {
		free_mem(mem, head);
		mem += head;
	}
	tail = align_clicks - head;
	if(tail > 0)
		free_mem(mem + wanted, tail);
  }

  return mem;
}

void mem_add_total_pages(int pages)
{
	total_pages += pages;
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
void mem_init(struct memory *chunks)
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

  total_pages = 0;
  free_pages_cnt = 0;	/* free_mem() below accounts the actual free pages */
  number_physical_pages = 0;

  memset(free_pages_bitmap, 0, sizeof(free_pages_bitmap));

  /* Use the chunks of physical memory to allocate holes. */
  for (i=NR_MEMS-1; i>=0; i--) {
  	if (chunks[i].size > 0) {
		phys_clicks base = chunks[i].base;
		phys_clicks size = chunks[i].size;
		phys_bytes from, to;

		/*
		 * The free_pages_bitmap[] array is statically sized for
		 * MAX_PHYSICAL_PAGES; indexing a page beyond that would overrun
		 * it and corrupt VM's own data.  Skip or clamp any chunk that
		 * reaches past the cap (on x86_64 this is the 64 GB the kernel
		 * identity-maps; on i386 it is the 4 GB address space).
		 */
		if (base >= MAX_PHYSICAL_PAGES)
			continue;
		/* overflow-safe: phys_clicks is 32-bit, so base+size could
		 * wrap; compare against the headroom instead. */
		if (size > (phys_clicks)MAX_PHYSICAL_PAGES - base)
			size = (phys_clicks)MAX_PHYSICAL_PAGES - base;

		/* Remember the highest page present so scans stop there. */
		if ((int)(base + size) > number_physical_pages)
			number_physical_pages = (int)(base + size);

		from = CLICK2ABS(base);
		to = CLICK2ABS(base+size)-1;
		if(first || from < mem_low) mem_low = from;
		if(first || to > mem_high) mem_high = to;
		free_mem(base, size);
		total_pages += size;
		first = 0;
	}
  }

  /* Defensive: never leave the scan bound at 0 (would make maxpage -1). */
  if (number_physical_pages <= 0)
	number_physical_pages = MAX_PHYSICAL_PAGES;
}

#if SANITYCHECKS
void mem_sanitycheck(const char *file, int line)
{
	int i;
	for(i = 0; i < number_physical_pages; i++) {
		if(!page_isfree(i)) continue;
		MYASSERT(usedpages_add(i * VM_PAGE_SIZE, VM_PAGE_SIZE) == OK);
	}
}
#endif

void memstats(int *nodes, int *pages, int *largest)
{
	int i;
	*nodes = 0;
	*pages = 0;
	*largest = 0;

	for(i = 0; i < number_physical_pages; i++) {
		int size = 0;
		while(i < number_physical_pages && page_isfree(i)) {
			size++;
			i++;
		}
		if(size == 0) continue;
		(*nodes)++;
		(*pages)+= size;
		if(size > *largest)
			*largest = size;
	}
}

static phys_clicks findbit(int low, int startscan, int pages, int memflags,
	int *len)
{
	/* NB: return type is phys_clicks, NOT int.  NO_MEM is
	 * (phys_clicks)MAP_NONE; returning it through an int and assigning to a
	 * 64-bit phys_bytes sign-extended it (0xFFFFFFFF_FFFFFFFE), so the
	 * caller's `== NO_MEM` test (zero-extended 32-bit) missed it and used a
	 * garbage page number.  Returning phys_clicks keeps it 32-bit/unsigned. */
	int run_length = 0, i;
	int freerange_start = startscan;

	for(i = startscan; i >= low; i--) {
		if(!page_isfree(i)) {
			int pi;
			int chunk = i/BITCHUNK_BITS, moved = 0;
			run_length = 0;
			pi = i;
			while(chunk > 0 &&
			   !MAP_CHUNK(free_pages_bitmap, chunk*BITCHUNK_BITS)) {
				chunk--;
				moved = 1;
			}
			if(moved) { i = chunk * BITCHUNK_BITS + BITCHUNK_BITS; }
			continue;
		}
		if(!run_length) { freerange_start = i; run_length = 1; }
		else { freerange_start--; run_length++; }
		assert(run_length <= pages);
		if(run_length == pages) {
			/* good block found! */
			*len = run_length;
			return freerange_start;
		}
	}

	return NO_MEM;
}

/*===========================================================================*
 *				alloc_pages				     *
 *===========================================================================*/
static phys_bytes alloc_pages(int pages, int memflags)
{
	phys_bytes boundary16 = 16 * 1024 * 1024 / VM_PAGE_SIZE;
	phys_bytes boundary1  =  1 * 1024 * 1024 / VM_PAGE_SIZE;
	phys_bytes mem = NO_MEM, i;	/* page number */
	int maxpage = number_physical_pages - 1;
	static int lastscan = -1;
	int startscan, run_length;

	/* Deny ordinary user allocations that would eat into the reserve kept
	 * for system services.  The process gets ENOMEM/SIGSEGV (acceptable),
	 * but services can still fault in pages and survive. */
	if((memflags & PAF_USERMEM) && free_pages_cnt - pages < PAGES_RESERVED)
		return NO_MEM;

	if(memflags & PAF_LOWER16MB)
		maxpage = boundary16 - 1;
	else if(memflags & PAF_LOWER1MB)
		maxpage = boundary1 - 1;
	else {
		/* no position restrictions: check page cache */
		if(pages == 1) {
			while(free_page_cache_size > 0) {
				i = free_page_cache[free_page_cache_size-1];
				if(page_isfree(i)) {
					free_page_cache_size--;
					mem = i;
					assert(mem != NO_MEM);
					run_length = 1;
					break;
				}
				free_page_cache_size--;
			}
		}
	}

	if(lastscan < maxpage && lastscan >= 0)
		startscan = lastscan;
	else	startscan = maxpage;

	if(mem == NO_MEM)
		mem = findbit(0, startscan, pages, memflags, &run_length);
	if(mem == NO_MEM)
		mem = findbit(0, maxpage, pages, memflags, &run_length);
	if(mem == NO_MEM)
		return NO_MEM;

	/* remember for next time */
	lastscan = mem;

	for(i = mem; i < mem + pages; i++) {
		UNSET_BIT(free_pages_bitmap, i);
	}
	free_pages_cnt -= pages;

	if(memflags & PAF_CLEAR) {
		int s;
		if ((s= sys_memset(NONE, 0, CLICK_SIZE*mem,
			VM_PAGE_SIZE*pages)) != OK) 
			panic("alloc_mem: sys_memset failed: %d", s);
	}

	return mem;
}

/*===========================================================================*
 *				free_pages				     *
 *===========================================================================*/
static void free_pages(phys_bytes pageno, int npages)
{
	int i, lim = pageno + npages - 1;

#if JUNKFREE
       if(sys_memset(NONE, 0xa5a5a5a5, VM_PAGE_SIZE * pageno,
               VM_PAGE_SIZE * npages) != OK)
                       panic("free_pages: sys_memset failed");
#endif

	for(i = pageno; i <= lim; i++) {
		SET_BIT(free_pages_bitmap, i);
		if(free_page_cache_size < PAGE_CACHE_MAX) {
			free_page_cache[free_page_cache_size++] = i;
		}
	}
	free_pages_cnt += npages;
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
int usedpages_add_f(phys_bytes addr, phys_bytes len, const char *file, int line)
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
		/* pagemap[] only covers the low 4 GB (SANITYCHECK_PHYSICAL_PAGES);
		 * pages above that are not tracked, so skip them. */
		if(pagestart >= SANITYCHECK_PHYSICAL_PAGES) {
			pages--;
			pagestart++;
			continue;
		}
		thisaddr = pagestart * VM_PAGE_SIZE;
		if(pagemap[pagestart].used) {
			static int warnings = 0;
			if(warnings++ < 100)
				printf("%s:%d: usedpages_add: addr 0x%lx reused, first %s:%d\n",
					file, line, thisaddr, pagemap[pagestart].file, pagemap[pagestart].line);
			util_stacktrace();
			return EFAULT;
		}
		pagemap[pagestart].used = 1;
		pagemap[pagestart].file = file;
		pagemap[pagestart].line = line;
		pages--;
		pagestart++;
	}

	return OK;
}

#endif

