/* EXTERN should be extern except for the table file */

#ifndef EXT2_GLO_H
#define EXT2_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/vfsif.h>

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;        /* temporary storage for error number */
EXTERN int rdwt_err;        /* status of last disk i/o request */

EXTERN int cch[NR_INODES];

extern char dot1[2];   /* dot1 (&dot1[0]) and dot2 (&dot2[0]) have a special */
extern char dot2[3];   /* meaning to search_dir: no access permission check. */

extern int(*fs_call_vec[]) (void);

EXTERN message fs_m_in;
EXTERN message fs_m_out;
EXTERN vfs_ucred_t credentials;

EXTERN uid_t caller_uid;
EXTERN gid_t caller_gid;

EXTERN int req_nr;

EXTERN char user_path[PATH_MAX+1];  /* pathname to be processed */

EXTERN dev_t fs_dev;              /* The device that is handled by this FS proc
                                   */
EXTERN char fs_dev_label[16];    /* Name of the device driver that is handled
                                  * by this FS proc.
                                  */
EXTERN int unmountdone;
EXTERN int exitsignaled;

/* Little hack for syncing group descriptors. */
EXTERN int group_descriptors_dirty;

EXTERN struct opt opt;		/* global options */

/* On ext2 metadata is stored in little endian format, so we shoud take
 * care about byte swapping, when have BE CPU. */
EXTERN int le_CPU;	/* little/big endian, if TRUE do not swap bytes */

#endif /* EXT2_GLO_H */
