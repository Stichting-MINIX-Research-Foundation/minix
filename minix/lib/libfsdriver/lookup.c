
#include "fsdriver.h"

/*
 * Check whether the given node may be accessed as directory.
 * Return OK or an appropriate error code.
 */
static int
access_as_dir(struct fsdriver_node * __restrict node,
	vfs_ucred_t * __restrict ucred)
{
	mode_t mask;
	int i;

	/* The file must be a directory to begin with. */
	if (!S_ISDIR(node->fn_mode)) return ENOTDIR;

	/* The root user may access anything at all. */
	if (ucred->vu_uid == ROOT_UID) return OK;

	/* Otherwise, the caller must have search access to the directory. */
	if (ucred->vu_uid == node->fn_uid) mask = S_IXUSR;
	else if (ucred->vu_gid == node->fn_gid) mask = S_IXGRP;
	else {
		mask = S_IXOTH;

		for (i = 0; i < ucred->vu_ngroups; i++) {
			if (ucred->vu_sgroups[i] == node->fn_gid) {
				mask = S_IXGRP;

				break;
			}
		}
	}

	return (node->fn_mode & mask) ? OK : EACCES;
}

/*
 * Get the next path component from a path.  Return the start and end of the
 * component into the path, and store its name in a null-terminated buffer.
 */
static int
next_name(char ** ptr, char ** start, char * __restrict name, size_t namesize)
{
	char *p;
	unsigned int i;

	/* Skip one or more path separator characters; they have no effect. */
	for (p = *ptr; *p == '/'; p++);

	*start = p;

	if (*p) {
		/*
		 * Copy as much of the name as possible, up to the next path
		 * separator.  Return an error if the name does not fit.
		 */
		for (i = 0; *p && *p != '/' && i < namesize; p++, i++)
			name[i] = *p;

		if (i >= namesize)
			return ENAMETOOLONG;

		name[i] = 0;
	} else
		/* An empty path component implies the current directory. */
		strlcpy(name, ".", namesize);

	/*
	 * Return a pointer to the first character not part of this component.
	 * This would typically be either the path separator or a null.
	 */
	*ptr = p;
	return OK;
}

/*
 * Given a symbolic link, resolve and return the contents of the link, followed
 * by the remaining part of the path that has not yet been resolved (the tail).
 * Note that the tail points into the given destination buffer.
 */
static int
resolve_link(const struct fsdriver * __restrict fdp, ino_t ino_nr, char * pptr,
	size_t size, char * tail)
{
	struct fsdriver_data data;
	char path[PATH_MAX];
	ssize_t r;

	data.endpt = SELF;
	data.ptr = path;
	data.size = sizeof(path) - 1;

	/*
	 * Let the file system the symbolic link.  Note that the resulting path
	 * is not null-terminated.
	 */
	if ((r = fdp->fdr_rdlink(ino_nr, &data, data.size)) < 0)
		return r;

	/* Append the remaining part of the original path to be resolved. */
	if (r + strlen(tail) >= sizeof(path))
		return ENAMETOOLONG;

	strlcpy(&path[r], tail, sizeof(path) - r);

	/* Copy back the result to the original buffer. */
	strlcpy(pptr, path, size);

	return OK;
}

/*
 * Process a LOOKUP request from VFS.
 */
int
fsdriver_lookup(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{
	ino_t dir_ino_nr, root_ino_nr;
	struct fsdriver_node cur_node, next_node;
	char path[PATH_MAX], name[NAME_MAX+1];
	char *ptr, *last;
	cp_grant_id_t path_grant;
	vfs_ucred_t ucred;
	unsigned int flags;
	size_t path_len, path_size;
	int r, r2, going_up, is_mountpt, symloop;

	if (fdp->fdr_lookup == NULL)
		return ENOSYS;

	dir_ino_nr = m_in->m_vfs_fs_lookup.dir_ino;
	root_ino_nr = m_in->m_vfs_fs_lookup.root_ino;
	path_grant = m_in->m_vfs_fs_lookup.grant_path;
	path_size = m_in->m_vfs_fs_lookup.path_size;
	path_len = m_in->m_vfs_fs_lookup.path_len;
	flags = m_in->m_vfs_fs_lookup.flags;

	/* Fetch the path name. */
	if ((r = fsdriver_getname(m_in->m_source, path_grant, path_len, path,
	    sizeof(path), FALSE /*not_empty*/)) != OK)
		return r;

	/* Fetch the caller's credentials. */
	if (flags & PATH_GET_UCRED) {
		if (m_in->m_vfs_fs_lookup.ucred_size != sizeof(ucred)) {
			printf("fsdriver: bad credential structure\n");

			return EINVAL;
		}

		if ((r = sys_safecopyfrom(m_in->m_source,
		    m_in->m_vfs_fs_lookup.grant_ucred, 0, (vir_bytes)&ucred,
		    (phys_bytes)m_in->m_vfs_fs_lookup.ucred_size)) != OK)
			return r;
	} else {
		ucred.vu_uid = m_in->m_vfs_fs_lookup.uid;
		ucred.vu_gid = m_in->m_vfs_fs_lookup.gid;
		ucred.vu_ngroups = 0;
	}

	/* Start the actual lookup by referencing the starting inode. */
	strlcpy(name, ".", sizeof(name)); /* allow a non-const argument */

	r = fdp->fdr_lookup(dir_ino_nr, name, &cur_node, &is_mountpt);
	if (r != OK)
		return r;

	symloop = 0;

	/* Whenever we leave this loop, 'cur_node' holds a referenced inode. */
	for (ptr = last = path; *ptr != 0; ) {
		/*
		 * Get the next path component. The result is a non-empty
		 * string.
		 */
		if ((r = next_name(&ptr, &last, name, sizeof(name))) != OK)
			break;

		if (is_mountpt) {
			/*
			 * If we start off from a mount point, the next path
			 * component *must* cause us to go up.  Anything else
			 * is a protocol violation.
			 */
			if (strcmp(name, "..")) {
				r = EINVAL;
				break;
			}
		} else {
			/*
			 * There is more path to process.  That means that the
			 * current file is now being accessed as a directory.
			 * Check type and permissions.
			 */
			if ((r = access_as_dir(&cur_node, &ucred)) != OK)
				break;
		}

		/* A single-dot component resolves to the current directory. */
		if (!strcmp(name, "."))
			continue;

		/* A dot-dot component resolves to the parent directory. */
		going_up = !strcmp(name, "..");

		if (going_up) {
			/*
			 * The parent of the process's root directory is the
			 * same root directory.  All processes have a root
			 * directory, so this check also covers the case of
			 * going up from the global system root directory.
			 */
			if (cur_node.fn_ino_nr == root_ino_nr)
				continue;

			/*
			 * Going up from the file system's root directory means
			 * crossing mount points.  As indicated, the root file
			 * system is already covered by the check above.
			 */
			if (cur_node.fn_ino_nr == fsdriver_root) {
				ptr = last;

				r = ELEAVEMOUNT;
				break;
			}
		}

		/*
		 * Descend into a child node or go up to a parent node, by
		 * asking the actual file system to perform a one-step
		 * resolution.  The result, if successful, is an open
		 * (referenced) inode.
		 */
		if ((r = fdp->fdr_lookup(cur_node.fn_ino_nr, name, &next_node,
		    &is_mountpt)) != OK)
			break;

		/* Sanity check: a parent node must always be a directory. */
		if (going_up && !S_ISDIR(next_node.fn_mode))
			panic("fsdriver: ascending into nondirectory");

		/*
		 * Perform symlink resolution, unless the symlink is the last
		 * path component and VFS is asking us not to resolve it.
		 */
		if (S_ISLNK(next_node.fn_mode) &&
		    (*ptr || !(flags & PATH_RET_SYMLINK))) {
			/*
			 * Resolve the symlink, and append the remaining
			 * unresolved part of the path.
			 */
			if (++symloop < _POSIX_SYMLOOP_MAX)
				r = resolve_link(fdp, next_node.fn_ino_nr,
				    path, sizeof(path), ptr);
			else
				r = ELOOP;

			if (fdp->fdr_putnode != NULL)
				fdp->fdr_putnode(next_node.fn_ino_nr, 1);

			if (r != OK)
				break;

			ptr = path;

			/* If the symlink is absolute, return it to VFS. */
			if (path[0] == '/') {
				r = ESYMLINK;
				break;
			}

			continue;
		}

		/* We have found a new node.  Continue from this node. */
		if (fdp->fdr_putnode != NULL)
			fdp->fdr_putnode(cur_node.fn_ino_nr, 1);

		cur_node = next_node;

		/*
		 * If the new node is a mount point, yield to another file
		 * system.
		 */
		if (is_mountpt) {
			r = EENTERMOUNT;
			break;
		}
	}

	/* For special redirection errors, we need to return extra details. */
	if (r == EENTERMOUNT || r == ELEAVEMOUNT || r == ESYMLINK) {
		/* Copy back the path if we resolved at least one symlink. */
		if (symloop > 0) {
			if ((path_len = strlen(path) + 1) > path_size)
				return ENAMETOOLONG;

			r2 = sys_safecopyto(m_in->m_source, path_grant, 0,
			    (vir_bytes)path, (phys_bytes)path_len);
		} else
			r2 = OK;

		if (r2 == OK) {
			m_out->m_fs_vfs_lookup.offset = (int)(ptr - path);
			m_out->m_fs_vfs_lookup.symloop = symloop;

			if (r == EENTERMOUNT)
				m_out->m_fs_vfs_lookup.inode =
				    cur_node.fn_ino_nr;
		} else
			r = r2;
	}

	/*
	 * On success, leave the resulting file open and return its details.
	 * If an error occurred, close the file and return error information.
	 */
	if (r == OK) {
		m_out->m_fs_vfs_lookup.inode = cur_node.fn_ino_nr;
		m_out->m_fs_vfs_lookup.mode = cur_node.fn_mode;
		m_out->m_fs_vfs_lookup.file_size = cur_node.fn_size;
		m_out->m_fs_vfs_lookup.uid = cur_node.fn_uid;
		m_out->m_fs_vfs_lookup.gid = cur_node.fn_gid;
		m_out->m_fs_vfs_lookup.device = cur_node.fn_dev;
	} else if (fdp->fdr_putnode != NULL)
		fdp->fdr_putnode(cur_node.fn_ino_nr, 1);

	return r;
}
