#include "fs.h"
#include <string.h>
#include <minix/com.h>
#include <assert.h>
#include <minix/u64.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include "const.h"

/*===========================================================================*
 *				count_free_bits				     *
 *===========================================================================*/
bit_t count_free_bits(sp, map)
struct super_block *sp;		/* the filesystem to allocate from */
int map;			/* IMAP (inode map) or ZMAP (zone map) */
{
/* Allocate a bit from a bit map and return its bit number. */
  block_t start_block;		/* first bit block */
  block_t block;
  bit_t map_bits;		/* how many bits are there in the bit map? */
  short bit_blocks;		/* how many blocks are there in the bit map? */
  bit_t origin;			/* number of bit to start searching at */
  unsigned word, bcount;
  struct buf *bp;
  bitchunk_t *wptr, *wlim, k;
  bit_t i, b;
  bit_t free_bits;

  assert(sp != NULL);

  if (map == IMAP) {
    start_block = START_BLOCK;
    map_bits = (bit_t) (sp->s_ninodes + 1);
    bit_blocks = sp->s_imap_blocks;
    origin = sp->s_isearch;
  } else {
    start_block = START_BLOCK + sp->s_imap_blocks;
    map_bits = (bit_t) (sp->s_zones - (sp->s_firstdatazone - 1));
    bit_blocks = sp->s_zmap_blocks;
    origin = sp->s_zsearch;
  }

  /* Figure out where to start the bit search (depends on 'origin'). */
  if (origin >= map_bits) origin = 0;    /* for robustness */
  free_bits = 0;

  /* Locate the starting place. */
  block = (block_t) (origin / FS_BITS_PER_BLOCK(sp->s_block_size));
  word = (origin % FS_BITS_PER_BLOCK(sp->s_block_size)) / FS_BITCHUNK_BITS;

  /* Iterate over all blocks plus one, because we start in the middle. */
  bcount = bit_blocks;
  do {
    bp = get_block(sp->s_dev, start_block + block, NORMAL);
    assert(bp);
    wlim = &b_bitmap(bp)[FS_BITMAP_CHUNKS(sp->s_block_size)];

    /* Iterate over the words in block. */
    for (wptr = &b_bitmap(bp)[word]; wptr < wlim; wptr++) {

      /* Does this word contain a free bit? */
      if (*wptr == (bitchunk_t) ~0) continue;

      k = (bitchunk_t) conv4(sp->s_native, (int) *wptr);

      for (i = 0; i < 8*sizeof(k); ++i) {
        /* Bit number from the start of the bit map. */
        b = ((bit_t) block * FS_BITS_PER_BLOCK(sp->s_block_size))
            + (wptr - &b_bitmap(bp)[0]) * FS_BITCHUNK_BITS
            + i;

        /* Don't count bits beyond the end of the map. */
        if (b >= map_bits) {
          break;
        } 
        if ((k & (1 << i)) == 0) {
          free_bits++;
        }
      }

      if (b >= map_bits) break;
    }
    put_block(bp, MAP_BLOCK);
    ++block;
    word = 0;
  } while (--bcount > 0);
  return free_bits;        /* no bit could be allocated */
}


/*===========================================================================*
 *				blockstats				     *
 *===========================================================================*/
void fs_blockstats(u32_t *blocks, u32_t *free, u32_t *used)
{
  struct super_block *sp;

  sp = get_super(fs_dev);

  assert(sp);
  assert(!sp->s_log_zone_size);

  *blocks = sp->s_zones;
  *used = get_used_blocks(sp);
  *free = *blocks - *used;

  return;
}
