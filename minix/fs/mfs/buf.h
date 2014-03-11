#ifndef __MFS_BUF_H__
#define __MFS_BUF_H__

#include "clean.h"

union ixfer_fsdata_u {
    char b__data[1];			/* ordinary user data */
    struct direct b__dir[1];		/* directory block */
    zone_t  b__v2_ind[1];		/* V2 indirect block */
    d2_inode b__v2_ino[1];		/* V2 inode block */
    bitchunk_t b__bitmap[1];		/* bit map block */
};

/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data(b)   ((union ixfer_fsdata_u *) b->data)->b__data
#define b_dir(b)    ((union ixfer_fsdata_u *) b->data)->b__dir
#define b_v2_ind(b) ((union ixfer_fsdata_u *) b->data)->b__v2_ind
#define b_v2_ino(b) ((union ixfer_fsdata_u *) b->data)->b__v2_ino
#define b_bitmap(b) ((union ixfer_fsdata_u *) b->data)->b__bitmap

#endif
