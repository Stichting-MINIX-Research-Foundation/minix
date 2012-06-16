/* VTreeFS - link.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				fs_rdlink				     *
 *===========================================================================*/
int fs_rdlink(void)
{
	/* Retrieve symbolic link target.
	 */
	char path[PATH_MAX];
	struct inode *node;
	size_t len;
	int r;

	if ((node = find_inode(fs_m_in.REQ_INODE_NR)) == NULL)
		return EINVAL;

	/* Call the rdlink hook. */
	assert(vtreefs_hooks->rdlink_hook != NULL);
	assert(!is_inode_deleted(node));	/* symlinks cannot be opened */

	r = vtreefs_hooks->rdlink_hook(node, path, sizeof(path),
		get_inode_cbdata(node));
	if (r != OK) return r;

	len = strlen(path);
	assert(len > 0 && len < sizeof(path));

	if (len > fs_m_in.REQ_MEM_SIZE)
		len = fs_m_in.REQ_MEM_SIZE;

	/* Copy out the result. */
	r = sys_safecopyto(fs_m_in.m_source, fs_m_in.REQ_GRANT, 0,
		(vir_bytes) path, len);
	if (r != OK) return r;

	fs_m_out.RES_NBYTES = len;
	return OK;
}
