/* Prototypes for -lminixfs. */

#ifndef _MINIX_FSLIB_H
#define _MINIX_FSLIB_H

#include <minix/safecopies.h>
#include <minix/sef.h>
#include <minix/vfsif.h>

int fs_lookup_credentials(vfs_ucred_t *credentials,
        uid_t *caller_uid, gid_t *caller_gid, cp_grant_id_t grant2, size_t cred_size);
u32_t fs_bufs_heuristic(int minbufs, u32_t btotal, u32_t bfree,
	int blocksize, dev_t majordev);

#endif /* _MINIX_FSLIB_H */

