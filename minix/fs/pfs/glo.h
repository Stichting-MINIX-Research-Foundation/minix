#ifndef __PFS_GLO_H__
#define __PFS_GLO_H__

/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */

extern struct fsdriver pfs_table;

EXTERN int busy;

/* Inode map. */
EXTERN bitchunk_t inodemap[FS_BITMAP_CHUNKS(PFS_NR_INODES)];

#endif
