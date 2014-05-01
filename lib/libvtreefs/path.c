/* VTreeFS - path.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				access_as_dir				     *
 *===========================================================================*/
static int access_as_dir(struct inode *node, vfs_ucred_t *ucred)
{
	/* Check whether the given inode may be accessed as directory.
	 * Return OK or an appropriate error code.
	 */
	mode_t mask;
	int i;

	/* The inode must be a directory to begin with. */
	if (!S_ISDIR(node->i_stat.mode)) return ENOTDIR;

	/* The caller must have search access to the directory.
	 * Root always does.
	 */
	if (ucred->vu_uid == SUPER_USER) return OK;

	if (ucred->vu_uid == node->i_stat.uid) mask = S_IXUSR;
	else if (ucred->vu_gid == node->i_stat.gid) mask = S_IXGRP;
	else {
		mask = S_IXOTH;

		for (i = 0; i < ucred->vu_ngroups; i++) {
			if (ucred->vu_sgroups[i] == node->i_stat.gid) {
				mask = S_IXGRP;

				break;
			}
		}
	}

	return (node->i_stat.mode & mask) ? OK : EACCES;
}

/*===========================================================================*
 *				next_name				     *
 *===========================================================================*/
static int next_name(char **ptr, char **start, char name[PNAME_MAX+1])
{
	/* Get the next path component from a path.
	 */
	char *p;
	int i;

	for (p = *ptr; *p == '/'; p++);

	*start = p;

	if (*p) {
		for (i = 0; *p && *p != '/' && i <= PNAME_MAX; p++, i++)
			name[i] = *p;

		if (i > PNAME_MAX)
			return ENAMETOOLONG;

		name[i] = 0;
	} else {
		strcpy(name, ".");
	}

	*ptr = p;
	return OK;
}

/*===========================================================================*
 *				go_up					     *
 *===========================================================================*/
static int go_up(struct inode *node, struct inode **parent)
{
	/* Given a directory inode, progress into the parent directory.
	 */

	*parent = get_parent_inode(node);

	/* Trapped in a deleted directory? Should not be possible. */
	if (*parent == NULL)
		return ENOENT;

	ref_inode(*parent);

	return OK;
}

/*===========================================================================*
 *				go_down					     *
 *===========================================================================*/
static int go_down(struct inode *parent, char *name, struct inode **child)
{
	/* Given a directory inode and a name, progress into a directory entry.
	 */
	int r;

	/* Call the lookup hook, if present, before doing the actual lookup. */
	if (!is_inode_deleted(parent) && vtreefs_hooks->lookup_hook != NULL) {
		r = vtreefs_hooks->lookup_hook(parent, name,
			get_inode_cbdata(parent));
		if (r != OK) return r;
	}

	if ((*child = get_inode_by_name(parent, name)) == NULL)
		return ENOENT;

	ref_inode(*child);

	return OK;
}

/*===========================================================================*
 *				resolve_link				     *
 *===========================================================================*/
static int resolve_link(struct inode *node, char pptr[PATH_MAX], char *tail)
{
	/* Given a symbolic link, resolve and return the contents of the link.
	 */
	char path[PATH_MAX];
	size_t len;
	int r;

	assert(vtreefs_hooks->rdlink_hook != NULL);
	assert(!is_inode_deleted(node));

	r = vtreefs_hooks->rdlink_hook(node, path, sizeof(path),
		get_inode_cbdata(node));
	if (r != OK) return r;

	len = strlen(path);
	assert(len > 0 && len < sizeof(path));

	if (len + strlen(tail) >= sizeof(path))
		return ENAMETOOLONG;

	strlcat(path, tail, sizeof(path));

	strlcpy(pptr, path, PATH_MAX);

	return OK;
}

/*===========================================================================*
 *				fs_lookup				     *
 *===========================================================================*/
int fs_lookup(void)
{
	/* Resolve a path string to an inode.
	 */
	ino_t dir_ino_nr, root_ino_nr;
	struct inode *cur_ino, *next_ino, *root_ino;
	char path[PATH_MAX], name[PNAME_MAX+1];
	char *ptr, *last;
	vfs_ucred_t ucred;
	size_t len;
	int r, r2, symloop;

	dir_ino_nr = fs_m_in.m_vfs_fs_lookup.dir_ino;
	root_ino_nr = fs_m_in.m_vfs_fs_lookup.root_ino;
	len = fs_m_in.m_vfs_fs_lookup.path_len;

	/* Fetch the path name. */
	if (len < 1 || len > PATH_MAX)
		return EINVAL;

	r = sys_safecopyfrom(fs_m_in.m_source,
		fs_m_in.m_vfs_fs_lookup.grant_path, 0, (vir_bytes) path,
		(phys_bytes) len);
	if (r != OK) return r;

	if (path[len-1] != 0) return EINVAL;

	/* Fetch the caller's credentials. */
	if (fs_m_in.m_vfs_fs_lookup.flags & PATH_GET_UCRED) {
		assert(fs_m_in.m_vfs_fs_lookup.ucred_size == sizeof(ucred));

		r = sys_safecopyfrom(fs_m_in.m_source,
			fs_m_in.m_vfs_fs_lookup.grant_ucred, 0,
			(vir_bytes) &ucred, fs_m_in.m_vfs_fs_lookup.ucred_size);

		if (r != OK)
			return r;
	}
	else {
		ucred.vu_uid = fs_m_in.m_vfs_fs_lookup.uid;
		ucred.vu_gid = fs_m_in.m_vfs_fs_lookup.gid;
		ucred.vu_ngroups = 0;
	}

	/* Start the actual lookup. */
	if ((cur_ino = get_inode(dir_ino_nr)) == NULL)
		return EINVAL;

	/* Chroot'ed environment? */
	if (root_ino_nr > 0)
		root_ino = find_inode(root_ino_nr);
	else
		root_ino = NULL;

	symloop = 0;

	for (ptr = last = path; ptr[0] != 0; ) {
		/* There is more path to process. That means that the current
		 * file is now being accessed as a directory. Check type and
		 * permissions.
		 */
		if ((r = access_as_dir(cur_ino, &ucred)) != OK)
			break;

		/* Get the next path component. The result is a non-empty
		 * string.
		 */
		if ((r = next_name(&ptr, &last, name)) != OK)
			break;

		if (!strcmp(name, ".") ||
				(cur_ino == root_ino && !strcmp(name, "..")))
			continue;

		if (!strcmp(name, "..")) {
			if (cur_ino == get_root_inode())
				r = ELEAVEMOUNT;
			else
				r = go_up(cur_ino, &next_ino);
		} else {
			r = go_down(cur_ino, name, &next_ino);

			/* Perform symlink resolution if we have to. */
			if (r == OK && S_ISLNK(next_ino->i_stat.mode) &&
				(ptr[0] != '\0' ||
				!(fs_m_in.m_vfs_fs_lookup.flags & PATH_RET_SYMLINK))) {

				if (++symloop == _POSIX_SYMLOOP_MAX) {
					put_inode(next_ino);

					r = ELOOP;

					break;
				}

				/* Resolve the symlink, and append the
				 * remaining unresolved part of the path.
				 */
				r = resolve_link(next_ino, path, ptr);

				put_inode(next_ino);

				if (r != OK)
					break;

				/* If the symlink is absolute, return it to
				 * VFS.
				 */
				if (path[0] == '/') {
					r = ESYMLINK;
					last = path;

					break;
				}

				ptr = path;
				continue;
			}
		}

		if (r != OK)
			break;

		/* We have found a new file. Continue from this file. */
		assert(next_ino != NULL);

		put_inode(cur_ino);

		cur_ino = next_ino;
	}

	/* If an error occurred, close the file and return error information.
	 */
	if (r != OK) {
		put_inode(cur_ino);

		/* We'd need support for this here. */
		assert(r != EENTERMOUNT);

		/* Copy back the path if we resolved at least one symlink. */
		if (symloop > 0 && (r == ELEAVEMOUNT || r == ESYMLINK)) {
			r2 = sys_safecopyto(fs_m_in.m_source,
				fs_m_in.m_vfs_fs_lookup.grant_path, 0,
				(vir_bytes) path, strlen(path) + 1);

			if (r2 != OK)
				r = r2;
		}

		if (r == ELEAVEMOUNT || r == ESYMLINK) {
			fs_m_out.m_fs_vfs_lookup.offset = (int) (last - path);
			fs_m_out.m_fs_vfs_lookup.symloop = symloop;
		}

		return r;
	}

	/* On success, leave the resulting file open and return its details. */
	fs_m_out.m_fs_vfs_lookup.inode = get_inode_number(cur_ino);
	fs_m_out.m_fs_vfs_lookup.mode = cur_ino->i_stat.mode;
	fs_m_out.m_fs_vfs_lookup.file_size = cur_ino->i_stat.size;
	fs_m_out.m_fs_vfs_lookup.uid = cur_ino->i_stat.uid;
	fs_m_out.m_fs_vfs_lookup.gid = cur_ino->i_stat.gid;
	fs_m_out.m_fs_vfs_lookup.device = cur_ino->i_stat.dev;

	return OK;
}
