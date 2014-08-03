#include "inc.h"
#include <minix/com.h>
#include <minix/vfsif.h>
#include <fcntl.h>
#ifdef __NBSD_LIBC
#include <stddef.h>
#endif
#include "proto.h"

int fs_rdlink()
{
	struct inode *i_node; /* target inode */
	int r;                /* return value */
	size_t copylen;

	/* Try to get inode according to its index */
	i_node = find_inode(fs_m_in.m_vfs_fs_rdlink.inode);
	if (i_node == NULL) 
		return EINVAL; /* no inode found */

	if(!S_ISLNK(i_node->i_stat.st_mode))
		r = EACCES;
	else {
		/* Passed all checks */
		copylen = MIN( (size_t) fs_m_in.m_vfs_fs_rdlink.mem_size, NAME_MAX);

		r = sys_safecopyto(VFS_PROC_NR,
		                   (cp_grant_id_t) fs_m_in.m_vfs_fs_rdlink.grant,
		                   (vir_bytes) 0,
		                   (vir_bytes) i_node->s_link,
		                   copylen);
	}

	return r;
}

