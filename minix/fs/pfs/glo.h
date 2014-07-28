#ifndef __PFS_GLO_H__
#define __PFS_GLO_H__

/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/vfsif.h>

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */

EXTERN int(*fs_call_vec[]) (message *fs_m_in, message *fs_m_out);

EXTERN int exitsignaled;
EXTERN int busy;
EXTERN int unmountdone;

/* Inode map. */
EXTERN bitchunk_t inodemap[FS_BITMAP_CHUNKS(PFS_NR_INODES)];

#endif
