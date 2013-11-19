/* Buffer (block) cache.  To acquire a block, a routine calls get_block(),
 * telling which block it wants.  The block is then regarded as "in use"
 * and has its 'b_count' field incremented.  All the blocks that are not
 * in use are chained together in an LRU list, with 'front' pointing
 * to the least recently used block, and 'rear' to the most recently used
 * block.  A reverse chain, using the field b_prev is also maintained.
 * Usage for LRU is measured by the time the put_block() is done.  The second
 * parameter to put_block() can violate the LRU order and put a block on the
 * front of the list, if it will probably not be needed soon.  If a block
 * is modified, the modifying routine must set b_dirt to DIRTY, so the block
 * will eventually be rewritten to the disk.
 */

#ifndef EXT2_BUF_H
#define EXT2_BUF_H

#include <sys/dirent.h>

union fsdata_u {
    char b__data[1];             /* ordinary user data */
/* indirect block */
    block_t b__ind[1];
/* bit map block */
    bitchunk_t b__bitmap[1];
};

/* A block is free if b_dev == NO_DEV. */

/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data(bp)   ((union fsdata_u *) bp->data)->b__data
#define b_ind(bp) ((union fsdata_u *) bp->data)->b__ind
#define b_bitmap(bp) ((union fsdata_u *) bp->data)->b__bitmap

#endif /* EXT2_BUF_H */
