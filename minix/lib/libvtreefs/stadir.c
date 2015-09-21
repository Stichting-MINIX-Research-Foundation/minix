/* VTreeFS - stadir.c - file and file system status management */

#include "inc.h"

/*
 * Retrieve file status.
 */
int
fs_stat(ino_t ino_nr, struct stat * buf)
{
	char path[PATH_MAX];
	time_t cur_time;
	struct inode *node;
	int r;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	/* Fill in the basic info. */
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

/*
 * Change file mode.
 */
int
fs_chmod(ino_t ino_nr, mode_t * mode)
{
	struct inode *node;
	struct inode_stat istat;
	int r;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	if (vtreefs_hooks->chstat_hook == NULL)
		return ENOSYS;

	get_inode_stat(node, &istat);

	istat.mode = (istat.mode & ~ALL_MODES) | (*mode & ALL_MODES);

	r = vtreefs_hooks->chstat_hook(node, &istat, get_inode_cbdata(node));

	if (r != OK)
		return r;

	get_inode_stat(node, &istat);

	*mode = istat.mode;

	return OK;
}

/*
 * Change file ownership.
 */
int
fs_chown(ino_t ino_nr, uid_t uid, gid_t gid, mode_t * mode)
{
	struct inode *node;
	struct inode_stat istat;
	int r;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	if (vtreefs_hooks->chstat_hook == NULL)
		return ENOSYS;

	get_inode_stat(node, &istat);

	istat.uid = uid;
	istat.gid = gid;
	istat.mode &= ~(S_ISUID | S_ISGID);

	r = vtreefs_hooks->chstat_hook(node, &istat, get_inode_cbdata(node));

	if (r != OK)
		return r;

	get_inode_stat(node, &istat);

	*mode = istat.mode;

	return OK;
}

/*
 * Retrieve file system statistics.
 */
int
fs_statvfs(struct statvfs * buf)
{

	buf->f_flag = ST_NOTRUNC;
	buf->f_namemax = NAME_MAX;

	return OK;
}
