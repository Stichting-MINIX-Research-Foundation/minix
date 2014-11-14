/* EXTERN should be extern except for the table file */

#ifndef LIBPUFFS_GLO_H
#define LIBPUFFS_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <fs/puffs/puffs_msgif.h>

EXTERN struct puffs_usermount *global_pu;

EXTERN int is_readonly_fs;
EXTERN int buildpath;

/* Sometimes user can call exit. If we received a message,
 * report a failure to VFS before exiting. Especially on mount
 * and unmount.
 */

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;        /* temporary storage for error number */

EXTERN struct puffs_kcred global_kcred;

EXTERN char fs_name[PATH_MAX+1];

EXTERN int mounted;
EXTERN int exitsignaled;

extern struct fsdriver puffs_table;

#endif /* LIBPUFFS_GLO_H */
