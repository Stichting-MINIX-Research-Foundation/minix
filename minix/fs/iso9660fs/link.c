#include "inc.h"

ssize_t fs_rdlink(ino_t ino_nr, struct fsdriver_data *data, size_t bytes)
{
	struct inode *i_node;
	size_t len;
	int r;

	/* Try to get inode according to its index */
	if ((i_node = find_inode(ino_nr)) == NULL)
		return EINVAL; /* no inode found */

	if (!S_ISLNK(i_node->i_stat.st_mode))
		return EACCES;

	len = strlen(i_node->s_link);
	if (len > bytes)
		len = bytes;

	if ((r = fsdriver_copyout(data, 0, i_node->s_link, len)) != OK)
		return r;

	return len;
}
