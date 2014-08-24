/* VTreeFS - stadir.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				fs_stat					     *
 *===========================================================================*/
int fs_stat(ino_t ino_nr, struct stat *buf)
{
	/* Retrieve file status.
	 */
	char path[PATH_MAX];
	time_t cur_time;
	struct inode *node;
	int r;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	/* Fill in the basic info. */
	buf->st_dev = fs_dev;
	buf->st_ino = get_inode_number(node);
	buf->st_mode = node->i_stat.mode;
	buf->st_nlink = !is_inode_deleted(node);
	buf->st_uid = node->i_stat.uid;
	buf->st_gid = node->i_stat.gid;
	buf->st_rdev = (dev_t) node->i_stat.dev;
	buf->st_size = node->i_stat.size;

	/* If it is a symbolic link, return the size of the link target. */
	if (S_ISLNK(node->i_stat.mode) && vtreefs_hooks->rdlink_hook != NULL) {
		r = vtreefs_hooks->rdlink_hook(node, path, sizeof(path),
			get_inode_cbdata(node));

		if (r == OK)
			buf->st_size = strlen(path);
	}

	/* Take the current time as file time for all files. */
	cur_time = clock_time(NULL);
	buf->st_atime = cur_time;
	buf->st_mtime = cur_time;
	buf->st_ctime = cur_time;

	return OK;
}

/*===========================================================================*
 *				fs_statvfs				     *
 *===========================================================================*/
int fs_statvfs(struct statvfs *buf)
{
	/* Retrieve file system statistics.
	 */

	buf->f_flag = ST_NOTRUNC;
	buf->f_namemax = PNAME_MAX;

	return OK;
}
