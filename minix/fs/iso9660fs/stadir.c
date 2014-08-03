#include "inc.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <minix/com.h>
#include <string.h>
#include <time.h>

#include <minix/vfsif.h>

int fs_stat()
{
	int r = EINVAL; /* return value */
	struct inode *dir;

	if ((dir = get_inode(fs_m_in.m_vfs_fs_stat.inode)) != NULL) {
		r = sys_safecopyto(fs_m_in.m_source, fs_m_in.m_vfs_fs_stat.grant,
		                   0, (vir_bytes) &dir->i_stat,
		                   (phys_bytes) sizeof(dir->i_stat));
		put_inode(dir);
	}

	return r;
}

int fs_statvfs()
{
	int r;
	struct statvfs st;

	memset(&st, 0, sizeof(st));

	st.f_bsize =  v_pri.logical_block_size_l;
	st.f_frsize = st.f_bsize;
	st.f_iosize = st.f_bsize;
	st.f_blocks = v_pri.volume_space_size_l;
	st.f_namemax = NAME_MAX;

	/* Copy the struct to user space. */
	r = sys_safecopyto(fs_m_in.m_source, fs_m_in.m_vfs_fs_statvfs.grant, 0,
	                   (vir_bytes) &st, (phys_bytes) sizeof(st));

	return r;
}

void fs_blockstats(u64_t *blocks, u64_t *free, u64_t *used)
{
	*used = *blocks = v_pri.volume_space_size_l;
	*free = 0;
}

