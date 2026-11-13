#ifndef FFS_BUF_H
#define FFS_BUF_H

union fsdata_u {
    char b__data[1];		/* ordinary user data */
    int64_t b__ind[1];		/* UFS2 indirect block (64-bit pointers) */
};

/* These defs make it possible to use bp->b_data instead of bp->b.b__data. */
#define b_data(bp)  ((union fsdata_u *) bp->data)->b__data
#define b_ind(bp)   ((union fsdata_u *) bp->data)->b__ind

#endif /* FFS_BUF_H */
