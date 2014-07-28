/* VTreeFS - read.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"
#include <dirent.h>
#include <minix/minlib.h>

#define GETDENTS_BUFSIZ 4096
#define DWORD_ALIGN(len) (((len) + sizeof(long) - 1) & ~(sizeof(long) - 1))

/*===========================================================================*
 *				fs_read					     *
 *===========================================================================*/
int fs_read(void)
{
	/* Read from a file.
	 */
	cp_grant_id_t gid;
	struct inode *node;
	off_t pos;
	size_t len;
	char *ptr;
	int r;

	/* Try to get inode by to its inode number. */
	if ((node = find_inode(fs_m_in.m_vfs_fs_readwrite.inode)) == NULL)
		return EINVAL;

	/* Check whether the node is a regular file. */
	if (!S_ISREG(node->i_stat.mode))
		return EINVAL;

	/* Get the values from the request message. */
	gid = fs_m_in.m_vfs_fs_readwrite.grant;
	pos = fs_m_in.m_vfs_fs_readwrite.seek_pos;

	/* Call the read hook, if any. */
	if (!is_inode_deleted(node) && vtreefs_hooks->read_hook != NULL) {
		len = fs_m_in.m_vfs_fs_readwrite.nbytes;

		/* On success, the read hook provides us with a pointer to the
		 * resulting data. This avoids copying overhead.
		 */
		r = vtreefs_hooks->read_hook(node, pos, &ptr, &len,
			get_inode_cbdata(node));

		assert(len <= fs_m_in.m_vfs_fs_readwrite.nbytes);

		/* Copy the resulting data to user space. */
		if (r == OK && len > 0) {
			r = sys_safecopyto(fs_m_in.m_source, fs_m_in.m_vfs_fs_readwrite.grant,
				0, (vir_bytes) ptr, len);
		}
	} else {
		/* Feign an empty file. */
		r = OK;
		len = 0;
	}

	if (r == OK) {
		fs_m_out.m_fs_vfs_readwrite.seek_pos = pos + len;
		fs_m_out.m_fs_vfs_readwrite.nbytes = len;
	}

	return r;
}

/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
int fs_getdents(void)
{
	/* Retrieve directory entries.
	 */
	struct inode *node, *child = NULL;
	struct dirent *dent;
	char *name;
	size_t len, off, user_off, user_left;
	off_t pos;
	int r, skip, get_next, indexed;
	static char buf[GETDENTS_BUFSIZ];

	if (fs_m_in.m_vfs_fs_getdents.seek_pos >= ULONG_MAX)
		return EIO;

	if ((node = find_inode(fs_m_in.m_vfs_fs_getdents.inode)) == NULL)
		return EINVAL;

	off = 0;
	user_off = 0;
	user_left = fs_m_in.m_vfs_fs_getdents.mem_size;
	indexed = node->i_indexed;
	get_next = FALSE;
	child = NULL;

	/* Call the getdents hook, if any, to "refresh" the directory. */
	if (!is_inode_deleted(node) && vtreefs_hooks->getdents_hook != NULL) {
		r = vtreefs_hooks->getdents_hook(node, get_inode_cbdata(node));
		if (r != OK) return r;
	}

	for (pos = fs_m_in.m_vfs_fs_getdents.seek_pos; ; pos++) {
		/* Determine which inode and name to use for this entry. */
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

		/* record length incl. alignment. */
                len = _DIRENT_RECLEN(dent, strlen(name));

		/* Is the user buffer too small to store another record? */
		if (user_off + off + len > user_left) {
			/* Is the user buffer too small for even a single
			 * record?
			 */
			if (user_off == 0 && off == 0)
				return EINVAL;

			break;
		}

		/* If our own buffer cannot contain the new record, copy out
		 * first.
		 */
		if (off + len > sizeof(buf)) {
			r = sys_safecopyto(fs_m_in.m_source, fs_m_in.m_vfs_fs_getdents.grant,
				user_off, (vir_bytes) buf, off);
			if (r != OK) return r;

			user_off += off;
			user_left -= off;
			off = 0;
		}

		/* Fill in the actual directory entry. */
		dent = (struct dirent *) &buf[off];
		dent->d_ino = (ino_t) get_inode_number(child);
		dent->d_reclen = len;
		dent->d_type = fs_mode_to_type(child->i_stat.mode);
		dent->d_namlen = strlen(name);
		strcpy(dent->d_name, name);

		off += len;
	}

	/* If there is anything left in our own buffer, copy that out now. */
	if (off > 0) {
		r = sys_safecopyto(fs_m_in.m_source, fs_m_in.m_vfs_fs_getdents.grant,
			user_off, (vir_bytes) buf, off);
		if (r != OK)
			return r;

		user_off += off;
	}

	fs_m_out.m_fs_vfs_getdents.seek_pos = pos;
	fs_m_out.m_fs_vfs_getdents.nbytes = user_off;

	return OK;
}
