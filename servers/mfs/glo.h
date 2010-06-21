#ifndef __MFS_GLO_H__
#define __MFS_GLO_H__

/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/vfsif.h>

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */
EXTERN int rdwt_err;		/* status of last disk i/o request */

EXTERN int cch[NR_INODES];

extern char dot1[2];   /* dot1 (&dot1[0]) and dot2 (&dot2[0]) have a special */
extern char dot2[3];   /* meaning to search_dir: no access permission check. */

extern _PROTOTYPE (int (*fs_call_vec[]), (void) ); /* fs call table */

EXTERN message fs_m_in;
EXTERN message fs_m_out;
EXTERN vfs_ucred_t credentials;

EXTERN uid_t caller_uid;
EXTERN gid_t caller_gid;

EXTERN int req_nr;

EXTERN endpoint_t SELF_E;

EXTERN char user_path[PATH_MAX+1];  /* pathname to be processed */

EXTERN dev_t fs_dev;    	/* The device that is handled by this FS proc.
				 */
EXTERN char fs_dev_label[16];	/* Name of the device driver that is handled
				 * by this FS proc.
				 */
EXTERN int unmountdone;
EXTERN int exitsignaled;

/* our block size. */
EXTERN unsigned int fs_block_size;

/* Buffer cache. */
EXTERN struct buf *buf;
EXTERN struct buf **buf_hash;   /* the buffer hash table */
EXTERN unsigned int nr_bufs;
EXTERN int may_use_vmcache;

#endif
