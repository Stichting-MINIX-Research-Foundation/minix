/* VTreeFS - link.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				fs_rdlink				     *
 *===========================================================================*/
ssize_t fs_rdlink(ino_t ino_nr, struct fsdriver_data *data, size_t bytes)
{
	/* Retrieve symbolic link target.
	 */
	char path[PATH_MAX];
	struct inode *node;
	size_t len;
	int r;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	/* Call the rdlink hook. */
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
