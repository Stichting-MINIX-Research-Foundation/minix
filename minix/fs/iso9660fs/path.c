#include "inc.h"

static int search_dir(
	struct inode *ldir_ptr,		/* dir record parent */
	char string[NAME_MAX],			/* component to search for */
	ino_t *numb				/* pointer to new dir record */
) {
	/* The search_dir function performs the operation of searching for the
	 * component ``string" in ldir_ptr. It returns the response and the
	 * number of the inode in numb.
	 */
	struct inode *dir_tmp;
	size_t pos = 0;
	int r;

	/*
	 * This function search a particular element (in string) in a inode and
	 * return its number.
	 */

	if ((ldir_ptr->i_stat.st_mode & S_IFMT) != S_IFDIR)
		return ENOTDIR;

	if (strcmp(string, ".") == 0) {
		*numb = ldir_ptr->i_stat.st_ino;
		return OK;
	}

	/*
	 * Parent directories need special attention to make sure directory
	 * inodes stay consistent.
	*/
	if (strcmp(string, "..") == 0) {
		if (ldir_ptr->i_stat.st_ino ==
		    v_pri.inode_root->i_stat.st_ino) {
			*numb = v_pri.inode_root->i_stat.st_ino;
			return OK;
		}
		else {
			dir_tmp = alloc_inode();
			r = read_inode(dir_tmp, ldir_ptr->extent, pos, &pos);
			if ((r != OK) || (pos >= ldir_ptr->i_stat.st_size)) {
				put_inode(dir_tmp);
				return ENOENT;
			}
			/* Temporary fix for extent spilling */
			put_inode(dir_tmp);
			dir_tmp = alloc_inode();
			/* End of fix */
			r = read_inode(dir_tmp, ldir_ptr->extent, pos, &pos);
			if ((r != OK) || (pos >= ldir_ptr->i_stat.st_size)) {
				put_inode(dir_tmp);
				return ENOENT;
			}
			*numb = dir_tmp->i_stat.st_ino;
			put_inode(dir_tmp);
			return OK;
		}
	}

	/* Read the dir's content */
	while (TRUE) {
		dir_tmp = alloc_inode();
		r = read_inode(dir_tmp, ldir_ptr->extent, pos, &pos);
		if ((r != OK) || (pos >= ldir_ptr->i_stat.st_size)) {
			put_inode(dir_tmp);
			return ENOENT;
		}

		if ((strcmp(dir_tmp->i_name, string) == 0) ||
		    (strcmp(dir_tmp->i_name, "..") &&
		    strcmp(string, "..") == 0)) {
			if (dir_tmp->i_stat.st_ino ==
			    v_pri.inode_root->i_stat.st_ino) {
				*numb = v_pri.inode_root->i_stat.st_ino;
				put_inode(dir_tmp);
				return OK;
			}

			*numb = dir_tmp->i_stat.st_ino;
			put_inode(dir_tmp);
			return OK;
		}

		put_inode(dir_tmp);
	}
}

int fs_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt)
{
	/* Given a directory and a component of a path, look up the component
	 * in the directory, find the inode, open it, and return its details.
	 */
	struct inode *dirp, *rip;
	ino_t ino_nr;
	int r;

	/* Find the starting inode. */
	if ((dirp = find_inode(dir_nr)) == NULL)
		return EINVAL;

	/* Look up the directory entry. */
	if ((r = search_dir(dirp, name, &ino_nr)) != OK)
		return r;

	/* The component has been found in the directory.  Get the inode. */
	if ((rip = get_inode(ino_nr)) == NULL)
		return EIO;	/* FIXME: this could have multiple causes */

	/* Return its details to the caller. */
	node->fn_ino_nr	= rip->i_stat.st_ino;
	node->fn_mode	= rip->i_stat.st_mode;
	node->fn_size	= rip->i_stat.st_size;
	node->fn_uid	= rip->i_stat.st_uid;
	node->fn_gid	= rip->i_stat.st_gid;
	node->fn_dev	= rip->i_stat.st_rdev;

	*is_mountpt = rip->i_mountpoint;

	return OK;
}
