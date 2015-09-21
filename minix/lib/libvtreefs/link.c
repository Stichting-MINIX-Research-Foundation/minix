/* VTreeFS - link.c - support for symbolic links and device nodes */

#include "inc.h"

/*
 * Retrieve a symbolic link target.
 */
ssize_t
fs_rdlink(ino_t ino_nr, struct fsdriver_data * data, size_t bytes)
{
	char path[PATH_MAX];
	struct inode *node;
	size_t len;
	int r;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	/* The hook should be provided for any FS that adds symlink inodes.. */
	if (vtreefs_hooks->rdlink_hook == NULL)
		return ENOSYS;

	assert(!is_inode_deleted(node));	/* symlinks cannot be opened */

	r = vtreefs_hooks->rdlink_hook(node, path, sizeof(path),
	    get_inode_cbdata(node));
	if (r != OK) return r;

	len = strlen(path);
	assert(len > 0 && len < sizeof(path));

	if (len > bytes)
		len = bytes;

	/* Copy out the result. */
	if ((r = fsdriver_copyout(data, 0, path, len)) != OK)
		return r;

	return len;
}

/*
 * Create a symbolic link.
 */
int
fs_slink(ino_t dir_nr, char * name, uid_t uid, gid_t gid,
	struct fsdriver_data * data, size_t bytes)
{
	char path[PATH_MAX];
	struct inode *node;
	struct inode_stat istat;
	int r;

	if ((node = find_inode(dir_nr)) == NULL)
		return EINVAL;

	if (vtreefs_hooks->slink_hook == NULL)
		return ENOSYS;

	if (get_inode_by_name(node, name) != NULL)
		return EEXIST;

	if (bytes >= sizeof(path))
		return ENAMETOOLONG;

	if ((r = fsdriver_copyin(data, 0, path, bytes)) != OK)
		return r;
	path[bytes] = 0;

	memset(&istat, 0, sizeof(istat));
	istat.mode = S_IFLNK | RWX_MODES;
	istat.uid = uid;
	istat.gid = gid;
	istat.size = strlen(path);
	istat.dev = 0;

	return vtreefs_hooks->slink_hook(node, name, &istat, path,
	    get_inode_cbdata(node));
}

/*
 * Create a device node.
 */
int
fs_mknod(ino_t dir_nr, char * name, mode_t mode, uid_t uid, gid_t gid,
	dev_t rdev)
{
	struct inode *node;
	struct inode_stat istat;

	if ((node = find_inode(dir_nr)) == NULL)
		return EINVAL;

	if (get_inode_by_name(node, name) != NULL)
		return EEXIST;

	if (vtreefs_hooks->mknod_hook == NULL)
		return ENOSYS;

	memset(&istat, 0, sizeof(istat));
	istat.mode = mode;
	istat.uid = uid;
	istat.gid = gid;
	istat.size = 0;
	istat.dev = rdev;

	return vtreefs_hooks->mknod_hook(node, name, &istat,
	    get_inode_cbdata(node));
}

/*
 * Unlink a node.
 */
int
fs_unlink(ino_t dir_nr, char * name, int __unused call)
{
	struct inode *dir_node, *node;

	if ((dir_node = find_inode(dir_nr)) == NULL)
		return EINVAL;

	if ((node = get_inode_by_name(dir_node, name)) == NULL)
		return ENOENT;

	if (vtreefs_hooks->unlink_hook == NULL)
		return ENOSYS;

	return vtreefs_hooks->unlink_hook(node, get_inode_cbdata(node));
}
