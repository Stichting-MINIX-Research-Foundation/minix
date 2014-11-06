/* VTreeFS - link.c - support for symbolic links */

#include "inc.h"

/*
 * Retrieve symbolic link target.
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

	/*
	 * Call the rdlink hook.  The hook must be non-NULL if the file system
	 * adds symlink nodes.  If it doesn't, we will never get here.
	 */
	assert(vtreefs_hooks->rdlink_hook != NULL);
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
