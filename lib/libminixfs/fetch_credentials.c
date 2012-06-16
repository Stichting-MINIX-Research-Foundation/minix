
#include <minix/vfsif.h>
#include <minix/type.h>
#include <minix/syslib.h>
#include <assert.h>
#include <string.h>

#include "minixfs.h"

int fs_lookup_credentials(vfs_ucred_t *credentials,
	uid_t *caller_uid, gid_t *caller_gid, cp_grant_id_t grant2, size_t cred_size)
{
  vfs_ucred_old_t old_cred;
  int r;

  memset(credentials, 0, sizeof(*credentials));

  if(cred_size == sizeof(*credentials)) {
           r = sys_safecopyfrom(VFS_PROC_NR, grant2, (vir_bytes) 0,
                                (vir_bytes) credentials, cred_size);
           if (r != OK) {
	   	printf("FS: cred copy (regular) failed.\n");
	   	return(r);
	   }
  } else if(cred_size == sizeof(old_cred)) {
           int g;
           r = sys_safecopyfrom(VFS_PROC_NR, grant2, (vir_bytes) 0,
                                (vir_bytes) &old_cred, sizeof(old_cred));
           if (r != OK) {
	   	printf("FS: cred copy (fallback) failed.\n");
	   	return(r);
	   }
           credentials->vu_ngroups = old_cred.vu_ngroups;
           credentials->vu_uid = old_cred.vu_uid;
           credentials->vu_gid = old_cred.vu_gid;
           for(g = 0; g < NGROUPS_MAX_OLD; g++) {
		   assert(g < NGROUPS_MAX);
                   credentials->vu_sgroups[g] = old_cred.vu_sgroups[g];
           }
  } else {
	   static int w = 0;
	   if(!w) { printf("FS: cred size incompatible with VFS.\n"); w = 1; }
           return(EINVAL); /* Wrong size. */
  }

   assert(credentials->vu_ngroups <= NGROUPS_MAX);

   *caller_uid = credentials->vu_uid;
   *caller_gid = credentials->vu_gid;

   return OK;
}

