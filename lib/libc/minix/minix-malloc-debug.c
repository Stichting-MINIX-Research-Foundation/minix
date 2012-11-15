/* pointless without assertions */
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <machine/vm.h>
#include <machine/vmparam.h>
#include <minix/minlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define mmap minix_mmap
#define munmap minix_munmap

#include "malloc-debug.h"

#if 0
#include <stdio.h>
static int reenter; 
#define LOG(args) if (!reenter) { reenter++; printf args; reenter--; }
#else
#define LOG(args)
#endif

struct block {
	size_t size;
	unsigned magic;
};

static u8_t *ptr_min, *ptr_max;

static unsigned long page_round_down(unsigned long x)
{
	return x - x % PAGE_SIZE;
}

static unsigned long page_round_up(unsigned long x)
{
	unsigned long rem;
	
	rem = x % PAGE_SIZE;
	if (rem)
		x += PAGE_SIZE - rem;
		
	return x;
}

#define page_round_down_ptr(x) ((u8_t *) page_round_down((unsigned long) (x)))
#define page_round_up_ptr(x) ((u8_t *) page_round_up((unsigned long) (x)))

static unsigned long block_compute_magic(struct block *block)
{
	return (unsigned long) block + block->size + 0xDEADBEEFUL;
}

static size_t block_get_totalsize(size_t size)
{
	return page_round_up(sizeof(struct block) + size);
}

static u8_t *block_get_endptr(struct block *block)
{
	return (u8_t *) block + block_get_totalsize(block->size);
}

static u8_t *block_get_dataptr(struct block *block)
{
	return block_get_endptr(block) - block->size;
}

static void block_check(struct block *block) 
{
	u8_t *dataptr, *p;

	/* check location */
	assert(block);
	assert(!((unsigned long) block % PAGE_SIZE));
	assert((u8_t *) block >= ptr_min);
	assert((u8_t *) block <= ptr_max);
	
	/* check size */
	assert(block->size > 0);
	
	/* check fillers */
	assert(block->magic == block_compute_magic(block));
	dataptr = block_get_dataptr(block);
	for (p = (u8_t *) (block + 1); p < dataptr; p++)
		assert(*p == ((unsigned long) p & 0xff));
}

static struct block *block_alloc(size_t size) 
{
	struct block *block;
	u8_t *dataptr, *p, *ptr;
	unsigned page_index, page_index_max;
	size_t sizerem, totalsize;
	u64_t tsc;

	LOG(("block_alloc; size=0x%x\n", size));
	assert(size > 0);
	
	/* round size up to machine word size */
	sizerem = size % sizeof(long);
	if (sizerem)
		size += sizeof(long) - sizerem;

	/* initialize address range */
	if (!ptr_min && !ptr_max) {
		/* keep a safe distance from areas that are in use:
		 * - 4MB from the break (should not change if traditional
		 *   malloc is not used so a small margin is sufficient
		 * - 256MB from the stack (big margin because memory beyond
		 *   this may be allocated by mmap when the address space 
		 *   starts to fill up)
		 */
		ptr_min = page_round_up_ptr((u8_t *) sbrk(0) + 0x400000);
		ptr_max = page_round_down_ptr((u8_t *) &size - 0x10000000);
	}
	assert(ptr_min);
	assert(ptr_max);
	assert(ptr_min < ptr_max);

	/* select address at random */
#ifdef __NBSD_LIBC
	tsc = 0;
#else
	tsc = 0;
  /* LSC Broken for now... */
  /* read_tsc_64(&tsc); */
#endif
	totalsize = block_get_totalsize(size);
	page_index_max = (ptr_max - ptr_min - totalsize) / PAGE_SIZE;
	page_index = (page_index_max > 0) ? (ex64lo(tsc) % page_index_max) : 0;
	ptr = ptr_min + page_index * PAGE_SIZE;
	
	/* allocate block */
	block = (struct block *) mmap(
		ptr, 				/* addr */
		totalsize,			/* len */ 
		PROT_READ|PROT_WRITE, 		/* prot */
		MAP_PREALLOC, 			/* flags */
		-1, 				/* fd */
		0);				/* offset */
	if (block == MAP_FAILED) {
		/* mmap call failed */
		abort();
	}

	/* block may not be at the requested location if that is in use */
	if (ptr_min > (u8_t *) block)
		ptr_min = (u8_t *) block;

	if (ptr_max < (u8_t *) block)
		ptr_max = (u8_t *) block;

	/* initialize block, including fillers */
	block->size = size;
	block->magic = block_compute_magic(block);
	dataptr = block_get_dataptr(block);
	for (p = (u8_t *) (block + 1); p < dataptr; p++)
		*p = ((unsigned long) p & 0xff);
		
	LOG(("block_alloc; block=0x%x\n", block));
	return block;
}

static struct block *block_find(const void *ptr) 
{
	struct block *block;

	LOG(("block_find; ptr=0x%x\n", ptr));
	assert(ptr);

	/* locate block based on pointer, then check whether it is valid */
	block = (struct block *) page_round_down(
		(unsigned long) ((struct block *) __UNCONST(ptr) - 1));
	block_check(block);
	LOG(("block_find; block=0x%x\n", block));
	return block;
}

static void block_free(struct block *block) 
{
	LOG(("block_free; block=0x%x\n", block));
	assert(block);

	/* simply unmap the block */
	if (munmap(block, block_get_totalsize(block->size)) < 0) {
		/* munmap call failed */
		abort();
	}
}

void *_dbg_malloc(size_t size)
{
	struct block *newblock;
	u8_t *ptr;
	
	LOG(("_dbg_malloc; size=0x%x\n", size));
	assert(size > 0); /* enforced by regular malloc */

	newblock = block_alloc(size);
	if (!newblock)
		return NULL;
		
	ptr = block_get_dataptr(newblock);
	LOG(("_dbg_malloc; ptr=0x%x\n", ptr));
	return ptr;
}

void *_dbg_realloc(void *oldp, size_t size)
{
	u8_t *newp;
	struct block *oldblock, *newblock;
	
	LOG(("_dbg_realloc; oldp=0x%x; size=0x%x\n", oldp, size));
	assert(oldp); /* enforced by regular realloc */
	assert(size > 0); /* enforced by regular realloc */

	/* always allocate new block */
	newblock = block_alloc(size);
	if (!newblock)
		return NULL;

	/* copy the data */
	oldblock = block_find(oldp);
	memcpy(block_get_dataptr(newblock), 
		block_get_dataptr(oldblock), 
		MIN(newblock->size, oldblock->size));
		
	/* deallocate old block */
	block_free(oldblock);
	
	newp = block_get_dataptr(newblock);
	LOG(("_dbg_realloc; newp=0x%x\n", newp));
	return newp;
}

void _dbg_free(void *ptr)
{
	LOG(("_dbg_free; ptr=0x%x\n", ptr));
	assert(ptr); /* enforced by regular free */

	/* find the block and free it */
	block_free(block_find(ptr));

	LOG(("_dbg_free done\n"));
}

