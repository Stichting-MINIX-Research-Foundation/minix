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

#include <sys/dir.h>            /* need struct direct */
#include <dirent.h>

union fsdata_u {
    char b__data[_MAX_BLOCK_SIZE];             /* ordinary user data */
/* indirect block */
    block_t b__ind[_MAX_BLOCK_SIZE/sizeof(block_t)];
/* bit map block */
    bitchunk_t b__bitmap[FS_BITMAP_CHUNKS(_MAX_BLOCK_SIZE)];
};

/* A block is free if b_dev == NO_DEV. */

/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data   bp->b__data
#define b_ind bp->b__ind
#define b_ino bp->b__ino
#define b_bitmap bp->b__bitmap

#define BUFHASH(b) ((b) % nr_bufs)

EXTERN struct buf *front;    /* points to least recently used free block */
EXTERN struct buf *rear;    /* points to most recently used free block */
EXTERN unsigned int bufs_in_use; /* # bufs currently in use (not on free list)*/

/* When a block is released, the type of usage is passed to put_block(). */
#define WRITE_IMMED   0100 /* block should be written to disk now */
#define ONE_SHOT      0200 /* set if block not likely to be needed soon */

#define INODE_BLOCK        0                 /* inode block */
#define DIRECTORY_BLOCK    1                 /* directory block */
#define INDIRECT_BLOCK     2                 /* pointer block */
#define MAP_BLOCK          3                 /* bit map */
#define FULL_DATA_BLOCK    5                 /* data, fully used */
#define PARTIAL_DATA_BLOCK 6                 /* data, partly used*/

#endif /* EXT2_BUF_H */
