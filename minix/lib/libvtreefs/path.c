/* VTreeFS - path.c - name resolution */

#include "inc.h"

/*
 * Resolve a path string to an inode.
 */
int
fs_lookup(ino_t dir_nr, char * name, struct fsdriver_node * node_details,
	int * is_mountpt)
{
	struct inode *node, *child;
	int r;

	if ((node = find_inode(dir_nr)) == NULL)
		return EINVAL;

	if (!S_ISDIR(node->i_stat.mode))
		return ENOTDIR;

	if (strlen(name) > NAME_MAX)
		return ENAMETOOLONG;

	if (!strcmp(name, ".")) {
		/* Stay in the given directory. */
		child = node;
	} else if (!strcmp(name, "..")) {
		/* Progress into the parent directory. */
		if ((child = get_parent_inode(node)) == NULL)
			return ENOENT;	/* deleted? should not be possible */
	} else {
		/* Progress into a directory entry.  Call the lookup hook, if
		 * present, before doing the actual lookup.
		 */
		if (!is_inode_deleted(node) &&
		    vtreefs_hooks->lookup_hook != NULL) {
			r = vtreefs_hooks->lookup_hook(node, name,
			    get_inode_cbdata(node));
			if (r != OK) return r;
		}

		if ((child = get_inode_by_name(node, name)) == NULL)
			return ENOENT;
	}

	/* On success, open the resulting file and return its details. */
	ref_inode(child);

	node_details->fn_ino_nr = get_inode_number(child);
	node_details->fn_mode = child->i_stat.mode;
	node_details->fn_size = child->i_stat.size;
	node_details->fn_uid = child->i_stat.uid;
	node_details->fn_gid = child->i_stat.gid;
	node_details->fn_dev = child->i_stat.dev;

	*is_mountpt = FALSE;

	return OK;
}
