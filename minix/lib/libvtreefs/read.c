/* VTreeFS - read.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"
#include <dirent.h>

#define GETDENTS_BUFSIZ 4096

/*===========================================================================*
 *				fs_read					     *
 *===========================================================================*/
ssize_t fs_read(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int __unused call)
{
	/* Read from a file.
	 */
	struct inode *node;
	size_t len;
	char *ptr;
	int r;

	/* Try to get inode by its inode number. */
	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	/* Check whether the node is a regular file. */
	if (!S_ISREG(node->i_stat.mode))
		return EINVAL;

	/* Call the read hook, if any. */
	if (!is_inode_deleted(node) && vtreefs_hooks->read_hook != NULL) {
		len = bytes;

		/* On success, the read hook provides us with a pointer to the
		 * resulting data. This avoids copying overhead.
		 */
		r = vtreefs_hooks->read_hook(node, pos, &ptr, &len,
			get_inode_cbdata(node));

		assert(len <= bytes);

		/* Copy the resulting data to user space. */
		if (r == OK && len > 0)
			r = fsdriver_copyout(data, 0, ptr, len);
	} else {
		/* Feign an empty file. */
		r = OK;
		len = 0;
	}

	return (r != OK) ? r : len;
}

/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *posp)
{
	/* Retrieve directory entries.
	 */
	struct fsdriver_dentry fsdentry;
	struct inode *node, *child;
	const char *name;
	off_t pos;
	int r, skip, get_next, indexed;
	static char buf[GETDENTS_BUFSIZ];

	if (*posp >= ULONG_MAX)
		return EIO;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	indexed = node->i_indexed;
	get_next = FALSE;
	child = NULL;

	/* Call the getdents hook, if any, to "refresh" the directory. */
	if (!is_inode_deleted(node) && vtreefs_hooks->getdents_hook != NULL) {
		r = vtreefs_hooks->getdents_hook(node, get_inode_cbdata(node));
		if (r != OK) return r;
	}

	fsdriver_dentry_init(&fsdentry, data, bytes, buf, sizeof(buf));

	do {
		/* Determine which inode and name to use for this entry. */
		pos = (*posp)++;

		if (pos == 0) {
			/* The "." entry. */
			child = node;
			name = ".";
		}
		else if (pos == 1) {
			/* The ".." entry. */
			child = get_parent_inode(node);
			if (child == NULL)
				child = node;
			name = "..";
		}
		else if (pos - 2 < indexed) {
			/* All indexed entries. */
			child = get_inode_by_index(node, pos - 2);

			/* If there is no inode with this particular index,
			 * continue with the next index number.
			 */
			if (child == NULL) continue;

			name = child->i_name;
		}
		else {
			/* All non-indexed entries. */

			/* If this is the first loop iteration, first get to
			 * the non-indexed child identified by the current
			 * position.
			 */
			if (get_next == FALSE) {
				skip = pos - indexed - 2;
				child = get_first_inode(node);

				/* Skip indexed children. */
				while (child != NULL &&
						child->i_index != NO_INDEX)
					child = get_next_inode(child);

				/* Skip to the right position. */
				while (child != NULL && skip-- > 0)
					child = get_next_inode(child);

				get_next = TRUE;
			}
			else {
				child = get_next_inode(child);
			}

			/* No more children? Then stop. */
			if (child == NULL)
				break;

			assert(!is_inode_deleted(child));

			name = child->i_name;
		}

		/* Add the directory entry to the output. */
		r = fsdriver_dentry_add(&fsdentry,
			(ino_t) get_inode_number(child), name, strlen(name),
			IFTODT(child->i_stat.mode));
		if (r < 0)
			return r;
	} while (r > 0);

	return fsdriver_dentry_finish(&fsdentry);
}
