/* This file manages the super block table and the related data structures,
 * namely, the bit maps that keep track of which zones and which inodes are
 * allocated and which are free.  When a new inode or zone is needed, the
 * appropriate bit map is searched for a free entry.
 *
 * The entry points into this file are
 *   alloc_bit:       somebody wants to allocate a zone or inode; find one
 *   free_bit:        indicate that a zone or inode is available for allocation
 */

#include "fs.h"
#include "buf.h"
#include "inode.h"
#include "const.h"


/*===========================================================================*
 *				alloc_bit				     *
 *===========================================================================*/
bit_t alloc_bit(void)
{
/* Allocate a bit from a bit map and return its bit number. */
  bitchunk_t *wptr, *wlim;
  bit_t b;
  unsigned int i, bcount;

  bcount = FS_BITMAP_CHUNKS(NR_INODES); /* Inode map has this many chunks. */
  wlim = &inodemap[bcount]; /* Point to last chunk in inodemap. */

  for (wptr = &inodemap[0]; wptr < wlim; wptr++) {
	/* Does this word contain a free bit? */
	if (*wptr == (bitchunk_t) ~0) continue; /* No. Go to next word */

	/* Find and allocate the free bit. */
	for (i = 0; (*wptr & (1 << i)) != 0; ++i) {}

	/* Get inode number */
	b = (bit_t) ((wptr - &inodemap[0]) * FS_BITCHUNK_BITS + i);

	/* Don't allocate bits beyond end of map. */
	if (b >= NR_INODES) break;

	/* Allocate and return bit number. */
	*wptr |= 1 << i;

	/* Mark server 'busy' */
	busy++;
	return(b);
  }

  return(NO_BIT);			/* no bit could be allocated */
}


/*===========================================================================*
 *				free_bit				     *
 *===========================================================================*/
void free_bit(bit_returned)
bit_t bit_returned;		/* number of bit to insert into the inode map*/
{
  bitchunk_t *k, mask;
  bit_t bit;
  unsigned word;

  /* Get word offset and bit within offset */
  word = (unsigned) (bit_returned / (bit_t) FS_BITCHUNK_BITS);
  bit = bit_returned % (bit_t) FS_BITCHUNK_BITS;

  /* Unset bit */
  k = &inodemap[word];
  mask = (unsigned) 1 << bit;
  *k &= ~mask;

  busy--; /* One inode less in use. */
}
