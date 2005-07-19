/*	malloc(), realloc(), free() - simple memory allocation routines
 *
 * This is a very small and simple minded malloc	Author: Kees J. Bot
 * implementation.  Ideal for things like a			29 Jan 1994
 * bootstrap program, or for debugging.  Six times
 * slower than any good malloc.
 */
#define nil 0

#define sbrk _sbrk
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#if !DEBUG
#define NDEBUG	1
#define debug(expr)	((void) 0)
#else
#define debug(expr)	expr
#endif
#include <assert.h>

typedef struct cell {
	size_t		size;		/* Size of a malloc()'ed object. */
#if DEBUG
	unsigned	magic;		/* To recognize a cell. */
#endif
	struct cell	*next;		/* Next cell on the free list. */
#if DEBUG
	unsigned	sacred;		/* Don't touch while unallocated. */
#endif
} cell_t;

#if UINT_MAX <= 0xFFFF
#define MAGIC	0x537B
#else
#define MAGIC	0x537BC0D8
#endif

/* Size of the header of an object. */
#define HDR_SIZE	offsetof(cell_t, next)

/* An offset from a cell pointer to a next cell. */
#define offset(cp, size)	((cell_t *) ((char *) (cp) + (size)))

/* Address of the object in a cell and back. */
#define cell2obj(cp)		((void *) ((char *) (cp) + HDR_SIZE))
#define obj2cell(op)		((cell_t *) ((char *) (op) - HDR_SIZE))

/* The free list. */
static cell_t *freelist;

void *malloc(size_t size)
/* Allocate an object of at least the given size. */
{
	cell_t **pcp, *cp;

	size += HDR_SIZE;
	if (size < sizeof(cell_t)) size= sizeof(cell_t);

	/* Align to a word.  Use a real malloc if you need better alignment. */
	size= (size + sizeof(int) - 1) & ~(sizeof(int) - 1);

	/* Space for a magic number at the end of the chunk. */
	debug(size += sizeof(unsigned));

	for (;;) {
		/* Do a first fit search. */
		pcp= &freelist;
		while ((cp= *pcp) != nil) {
			cell_t *next= cp->next;

			assert(cp->magic == MAGIC);
			assert(cp->sacred == MAGIC);

			if (offset(cp, cp->size) == next) {
				/* Join adjacent free cells. */
				assert(next->magic == MAGIC);
				assert(next->sacred == MAGIC);

				cp->size+= next->size;
				cp->next= next->next;

				continue;		/* Try again. */
			}
			if (size <= cp->size) break;	/* Big enough. */

			/* Next cell. */
			pcp= &cp->next;
		}

		if (cp != nil) break;	/* Found a big enough chunk. */

		/* Allocate a new chunk at the break. */
		if ((cp= (cell_t *) sbrk(size)) == (cell_t *) -1) {
			return nil;
		}

		cp->size= size;
		cp->next= nil;
		debug(cp->magic= MAGIC);
		debug(cp->sacred= MAGIC);
		*pcp= cp;
	}

	/* We've got a cell that is big enough.  Can we break it up? */
	if (cp->size >= size + sizeof(cell_t)) {
		cell_t *next= offset(cp, size);

		next->size= cp->size - size;
		next->next= cp->next;
		debug(next->magic= MAGIC);
		debug(next->sacred= MAGIC);
		cp->size= size;
		cp->next= next;
	}

	/* Unchain the cell we've found and return an address in it. */
	*pcp= cp->next;
	debug(memset(cell2obj(cp), 0xAA, cp->size - HDR_SIZE));
	debug(((unsigned *) offset(cp, cp->size))[-1]= MAGIC);

	return cell2obj(cp);
}

void free(void *op)
/* Deallocate an object. */
{
	cell_t **prev, *next, *cp;

	if (op == nil) return;		/* Aaargh. */

	cp= obj2cell(op);
	assert(cp->magic == MAGIC);
	assert(((unsigned *) offset(cp, cp->size))[-1] == MAGIC);
	debug(cp->sacred= MAGIC);

	/* Find the spot where the object belongs. */
	prev= &freelist;
	while ((next= *prev) != nil && next < cp) {
		assert(next->magic == MAGIC);
		assert(next->sacred == MAGIC);
		prev= &next->next;
	}

	/* Put the new free cell in the list. */
	*prev= cp;
	cp->next= next;

#if DEBUG
	/* Check the rest of the list. */
	while (next != nil) {
		assert(next->magic == MAGIC);
		assert(next->sacred == MAGIC);
		next= next->next;
	}
#endif
}

void *realloc(void *op, size_t size)
/* Change the size of an object.  Don't bother being smart, just copy it. */
{
	size_t oldsize;
	void *new;

	oldsize= op == nil ? 0 : obj2cell(op)->size - HDR_SIZE;

	new= malloc(size);
	memcpy(new, op, oldsize > size ? size : oldsize);
	free(op);
	return new;
}

/*
 * $PchId: malloc.c,v 1.4 1996/02/22 09:15:56 philip Exp $
 */
