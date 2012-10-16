#ifndef __MFS_BUF_H__
#define __MFS_BUF_H__

#include "clean.h"

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

#include <dirent.h>

union fsdata_u {
    char b__data[_MAX_BLOCK_SIZE];		     /* ordinary user data */
/* directory block */
    struct direct b__dir[NR_DIR_ENTRIES(_MAX_BLOCK_SIZE)];    
/* V1 indirect block */
    zone1_t b__v1_ind[V1_INDIRECTS];	     
/* V2 indirect block */
    zone_t  b__v2_ind[V2_INDIRECTS(_MAX_BLOCK_SIZE)];	     
/* V1 inode block */
    d1_inode b__v1_ino[V1_INODES_PER_BLOCK]; 
/* V2 inode block */
    d2_inode b__v2_ino[V2_INODES_PER_BLOCK(_MAX_BLOCK_SIZE)]; 
/* bit map block */
    bitchunk_t b__bitmap[FS_BITMAP_CHUNKS(_MAX_BLOCK_SIZE)];  
};

/* A block is free if b_dev == NO_DEV. */


/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data(b)   ((union fsdata_u *) b->data)->b__data
#define b_dir(b)    ((union fsdata_u *) b->data)->b__dir
#define b_v1_ind(b) ((union fsdata_u *) b->data)->b__v1_ind
#define b_v2_ind(b) ((union fsdata_u *) b->data)->b__v2_ind
#define b_v1_ino(b) ((union fsdata_u *) b->data)->b__v1_ino
#define b_v2_ino(b) ((union fsdata_u *) b->data)->b__v2_ino
#define b_bitmap(b) ((union fsdata_u *) b->data)->b__bitmap

#endif

