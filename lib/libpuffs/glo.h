/* EXTERN should be extern except for the table file */

#ifndef LIBPUFFS_GLO_H
#define LIBPUFFS_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/vfsif.h>

#include "puffs_msgif.h"

EXTERN struct puffs_usermount *global_pu;

EXTERN int is_readonly_fs;
EXTERN int is_root_fs;
EXTERN int buildpath;

/* Sometimes user can call exit. If we received a message,
 * report a failure to VFS before exiting. Especially on mount
 * and unmount.
 *
 * Either transid of last request or 0.
 */
EXTERN int last_request_transid;

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;        /* temporary storage for error number */

/* TODO: it duplicates caller_uid and caller_gid */
EXTERN struct puffs_kcred global_kcred;

extern int(*fs_call_vec[]) (void);

EXTERN message fs_m_in;
EXTERN message fs_m_out;
EXTERN vfs_ucred_t credentials;

EXTERN uid_t caller_uid;
EXTERN gid_t caller_gid;

EXTERN int req_nr;

EXTERN endpoint_t SELF_E;

EXTERN char user_path[PATH_MAX+1];  /* pathname to be processed */

EXTERN dev_t fs_dev;              /* The device that is handled by this FS proc
                                   */
EXTERN char fs_name[PATH_MAX+1];

EXTERN int unmountdone;
EXTERN int exitsignaled;

#endif /* LIBPUFFS_GLO_H */
