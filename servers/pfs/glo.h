/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/vfsif.h>

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */

EXTERN int cch[NR_INODES];

extern _PROTOTYPE (int (*fs_call_vec[]), (void) ); /* fs call table */

EXTERN message fs_m_in;
EXTERN message fs_m_out;

EXTERN uid_t caller_uid;
EXTERN gid_t caller_gid;
EXTERN int req_nr;
EXTERN int SELF_E;
EXTERN int exitsignaled;
EXTERN int busy;

/* Inode map. */
EXTERN bitchunk_t inodemap[FS_BITMAP_CHUNKS(NR_INODES)]; 
