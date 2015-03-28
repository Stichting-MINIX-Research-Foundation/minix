#ifndef EXT2_BUF_H
#define EXT2_BUF_H

union fsdata_u {
    char b__data[1];		/* ordinary user data */
    block_t b__ind[1];		/* indirect block */
    bitchunk_t b__bitmap[1];	/* bit map block */
};

/* These defs make it possible to use to bp->b_data instead of bp->b.b__data */
#define b_data(bp)   ((union fsdata_u *) bp->data)->b__data
#define b_ind(bp) ((union fsdata_u *) bp->data)->b__ind
#define b_bitmap(bp) ((union fsdata_u *) bp->data)->b__bitmap

#endif /* EXT2_BUF_H */
