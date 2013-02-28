
#include <minix/vfsif.h>
#include <minix/type.h>
#include <minix/syslib.h>
#include <assert.h>
#include <string.h>

#include "minixfs.h"

int fs_lookup_credentials(vfs_ucred_t *credentials,
	uid_t *caller_uid, gid_t *caller_gid, cp_grant_id_t grant2, size_t cred_size)
{
  int r;

  memset(credentials, 0, sizeof(*credentials));

  r = sys_safecopyfrom(VFS_PROC_NR, grant2, (vir_bytes) 0,
			(vir_bytes) credentials, cred_size);
  if (r != OK) {
	printf("FS: cred copy failed\n");
	return(r);
  }

  assert(credentials->vu_ngroups <= NGROUPS_MAX);

  *caller_uid = credentials->vu_uid;
  *caller_gid = credentials->vu_gid;

  return OK;
}

