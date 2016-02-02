#include "inc.h"
#include <sys/stat.h>
#include <sys/statvfs.h>

int fs_stat(ino_t ino_nr, struct stat *statbuf)
{
	struct inode *rip;

	if ((rip = get_inode(ino_nr)) == NULL)
		return EINVAL;

	*statbuf = rip->i_stat;

	return OK;
}

int fs_statvfs(struct statvfs *st)
{
	st->f_flag = ST_NOTRUNC;
	st->f_bsize =  v_pri.logical_block_size_l;
	st->f_frsize = st->f_bsize;
	st->f_iosize = st->f_bsize;
	st->f_blocks = v_pri.volume_space_size_l;
	st->f_namemax = NAME_MAX;

	return OK;
}
