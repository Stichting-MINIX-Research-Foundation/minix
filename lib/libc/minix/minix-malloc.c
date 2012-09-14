/* $Header$ */

/* replace undef by define */
#define  ALIGN_EIGHT_BYTES /* Use 8-byte alignment. */
#define	 DEBUG		   /* check assertions */
#undef	 SLOWDEBUG	   /* some extra test loops (requires DEBUG) */

#ifndef DEBUG
#define NDEBUG
#endif

#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<assert.h>

#include 	"malloc-debug.h"

static int no_debug = -1;
#define CHECK_DBG(statement)						\
  if (no_debug <= 0) {							\
  	if (no_debug < 0) no_debug = getenv("MALLOC_DEBUG") ? 0 : 1;	\
	if (no_debug == 0) { statement; }				\
  }

#define	ptrint		int

#define BRKSIZE		4096
#ifdef ALIGN_EIGHT_BYTES
#define PTRSIZE		8
#else
#define	PTRSIZE		((int) sizeof(void *))
#endif
#define Align(x,a)	(((x) + (a - 1)) & ~(a - 1))
#define NextSlot(p)	(* (void **) ((p) - PTRSIZE))
#define NextFree(p)	(* (void **) (p))

/*
 * A short explanation of the data structure and algorithms.
 * An area returned by malloc() is called a slot. Each slot
 * contains the number of bytes requested, but preceeded by
 * an extra pointer to the next the slot in memory.
 * '_bottom' and '_top' point to the first/last slot.
 * More memory is asked for using brk() and appended to top.
 * The list of free slots is maintained to keep malloc() fast.
 * '_empty' points the the first free slot. Free slots are
 * linked together by a pointer at the start of the
 * user visable part, so just after the next-slot pointer.
 * Free slots are merged together by free().
 *
 * Since modern processors prefer 8-byte alignment, we now pretend
 * our pointers are 8 bytes wide.
 */

extern void *_sbrk(int);
extern int _brk(void *);
static void *_bottom, *_top, *_empty;

static int grow(size_t len)
{
  register char *p;

  assert(NextSlot((char *)_top) == 0);
  if ((char *) _top + len < (char *) _top
      || (p = (char *)Align((ptrint)_top + len, BRKSIZE)) < (char *) _top ) {
	errno = ENOMEM;
	return(0);
  }
  if (_brk(p) != 0)
	return(0);
  NextSlot((char *)_top) = p;
  NextSlot(p) = 0;
  free(_top);
  _top = p;
  return 1;
}

void *
malloc(const size_t size)
{
  register char *prev, *p, *next, *new;
  unsigned ntries;

  if (size == 0)
	return NULL;

  CHECK_DBG(return _dbg_malloc(size));

  for (ntries = 0; ntries < 2; ntries++) {
	unsigned len = Align(size, PTRSIZE) + PTRSIZE;
	if (len < 2 * PTRSIZE) {
		errno = ENOMEM;
		return NULL;
	}
	if (_bottom == 0) {
		if ((p = _sbrk(2 * PTRSIZE)) == (char *) -1)
			return NULL;
		p = (char *) Align((ptrint)p, PTRSIZE);
		p += PTRSIZE;
		_top = _bottom = p;
		NextSlot(p) = 0;
	}
#ifdef SLOWDEBUG
	for (p = _bottom; (next = NextSlot(p)) != 0; p = next)
		assert(next > p);
	assert(p == _top);
#endif
	for (prev = 0, p = _empty; p != 0; prev = p, p = NextFree(p)) {
		next = NextSlot(p);
		new = p + len;	/* easily overflows!! */
		if (new > next || new <= p)
			continue;		/* too small */
		if (new + PTRSIZE < next) {	/* too big, so split */
			/* + PTRSIZE avoids tiny slots on free list */
			NextSlot(new) = next;
			NextSlot(p) = new;
			NextFree(new) = NextFree(p);
			NextFree(p) = new;
		}
		if (prev)
			NextFree(prev) = NextFree(p);
		else
			_empty = NextFree(p);
		return p;
	}
	if (grow(len) == 0)
		break;
  }
  assert(ntries != 2);
  return NULL;
}

void *
realloc(void *oldp, size_t size)
{
  register char *prev, *p, *next, *new;
  char *old = oldp;
  register size_t len, n;

  if (old == 0)
	return malloc(size);
  if (size == 0) {
	free(old);
	return NULL;
  }

  CHECK_DBG(return _dbg_realloc(oldp, size));

  len = Align(size, PTRSIZE) + PTRSIZE;
  next = NextSlot(old);
  n = (int)(next - old);			/* old length */
  /*
   * extend old if there is any free space just behind it
   */
  for (prev = 0, p = _empty; p != 0; prev = p, p = NextFree(p)) {
	if (p > next)
		break;
	if (p == next) {	/* 'next' is a free slot: merge */
		NextSlot(old) = NextSlot(p);
		if (prev)
			NextFree(prev) = NextFree(p);
		else
			_empty = NextFree(p);
		next = NextSlot(old);
		break;
	}
  }
  new = old + len;
  /*
   * Can we use the old, possibly extended slot?
   */
  if (new <= next && new >= old) {		/* it does fit */
	if (new + PTRSIZE < next) {		/* too big, so split */
		/* + PTRSIZE avoids tiny slots on free list */
		NextSlot(new) = next;
		NextSlot(old) = new;
		free(new);
	}
	return old;
  }
  if ((new = malloc(size)) == NULL)		/* it didn't fit */
	return NULL;
  memcpy(new, old, n);				/* n < size */
  free(old);
  return new;
}

void
free(void *ptr)
{
  register char *prev, *next;
  char *p = ptr;

  if (p == 0)
	return;

  CHECK_DBG(_dbg_free(ptr); return);

#ifdef SLOWDEBUG
  {
  	int found;
	char *curr;

	/* block must be in block list */
	assert(_bottom);
	found = 0;
	for (curr = _bottom; (next = NextSlot(curr)) != 0; curr = next) {
		assert(next > curr);
		if (curr == p) found = 1;
	}
	if (curr == p) found = 1;
	assert(found);

	/* block must not be in free list */
	if (_empty) {
		found = 0;
		for (curr = _empty; (next = NextFree(curr)) != 0; curr = next) {
			assert(next > curr);
			if (curr == p) found = 1;
		}
		if (curr == p) found = 1;
		assert(!found);
	}
  }
#endif

  assert((char *) NextSlot(p) > p);
  for (prev = 0, next = _empty; next != 0; prev = next, next = NextFree(next))
	if (p < next)
		break;
  NextFree(p) = next;
  if (prev)
	NextFree(prev) = p;
  else
	_empty = p;
  if (next) {
	assert((char *) NextSlot(p) <= next);
	if (NextSlot(p) == next) {		/* merge p and next */
		NextSlot(p) = NextSlot(next);
		NextFree(p) = NextFree(next);
	}
  }
  if (prev) {
	assert((char *) NextSlot(prev) <= p);
	if (NextSlot(prev) == p) {		/* merge prev and p */
		NextSlot(prev) = NextSlot(p);
		NextFree(prev) = NextFree(p);
	}
  }
}
