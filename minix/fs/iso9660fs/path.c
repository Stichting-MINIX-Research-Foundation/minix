#include "inc.h"
#include <string.h>
#include <minix/com.h>
#include <minix/vfsif.h>
#include <sys/stat.h>

static char *get_name(char *name, char string[NAME_MAX+1]);
static int ltraverse(struct inode *rip, char *suffix);
static int parse_path(ino_t dir_ino, ino_t root_ino, int flags, struct
	inode **res_inop, size_t *offsetp, int *symlinkp);


/*===========================================================================*
 *                             fs_lookup				     *
 *===========================================================================*/
int fs_lookup() {
	cp_grant_id_t grant;
	int r, len, flags, symlinks = 0;
	size_t offset = 0;
	ino_t dir_ino, root_ino;
	struct inode *dir = 0;

	grant		= fs_m_in.m_vfs_fs_lookup.grant_path;
	len		= fs_m_in.m_vfs_fs_lookup.path_len;	/* including terminating nul */
	dir_ino		= fs_m_in.m_vfs_fs_lookup.dir_ino;
	root_ino	= fs_m_in.m_vfs_fs_lookup.root_ino;
	flags		= fs_m_in.m_vfs_fs_lookup.flags;
	caller_uid	= fs_m_in.m_vfs_fs_lookup.uid;
	caller_gid	= fs_m_in.m_vfs_fs_lookup.gid;

	/* Check length. */
	if(len > sizeof(user_path))
		return E2BIG;	/* too big for buffer */
	if(len < 1)
		return EINVAL;			/* too small */

	/* Copy the pathname and set up caller's user and group id */
	r = sys_safecopyfrom(VFS_PROC_NR, grant, 0, (vir_bytes) user_path, 
		             (phys_bytes) len);
	if (r != OK) {
		printf("ISOFS %s:%d sys_safecopyfrom failed: %d\n",
		       __FILE__, __LINE__, r);
		return r;
	}

	/* Verify this is a null-terminated path. */
	if(user_path[len-1] != '\0')
		return EINVAL;

	/* Lookup inode */
	r = parse_path(dir_ino, root_ino, flags, &dir, &offset, &symlinks);

	if(r == ELEAVEMOUNT || r == ESYMLINK) {
		/* Report offset and the error */
		fs_m_out.m_fs_vfs_lookup.offset = offset;
		fs_m_out.m_fs_vfs_lookup.symloop = symlinks;
		return r;
	}

	if (r != OK && r != EENTERMOUNT)
		return r;

	fs_m_out.m_fs_vfs_lookup.inode		= dir->i_stat.st_ino;
	fs_m_out.m_fs_vfs_lookup.mode		= dir->i_stat.st_mode;
	fs_m_out.m_fs_vfs_lookup.file_size	= dir->i_stat.st_size;
	fs_m_out.m_fs_vfs_lookup.device		= dir->i_stat.st_rdev;
	fs_m_out.m_fs_vfs_lookup.symloop	= 0;
	fs_m_out.m_fs_vfs_lookup.uid		= dir->i_stat.st_uid;
	fs_m_out.m_fs_vfs_lookup.gid		= dir->i_stat.st_gid;

	if (r == EENTERMOUNT) {
		fs_m_out.m_fs_vfs_lookup.offset = offset;
		put_inode(dir);
	}

	return r;
}

/* The search dir actually performs the operation of searching for the
 * compoent ``string" in ldir_ptr. It returns the response and the number of
 * the inode in numb. */
/*===========================================================================*
 *				search_dir				     *
 *===========================================================================*/
int search_dir(
	struct inode *ldir_ptr,		/* dir record parent */
	char string[NAME_MAX],			/* component to search for */
	ino_t *numb				/* pointer to new dir record */
) {
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
		if (ldir_ptr->i_stat.st_ino == v_pri.inode_root->i_stat.st_ino) {
			*numb = v_pri.inode_root->i_stat.st_ino;
			return OK;
		}
		else {
			dir_tmp = alloc_inode();
			r = read_inode(dir_tmp, ldir_ptr->extent, pos, &pos);
			if ((r != OK) || (pos >= ldir_ptr->i_stat.st_size)) {
				put_inode(dir_tmp);
				return EINVAL;
			}
			r = read_inode(dir_tmp, ldir_ptr->extent, pos, &pos);
			if ((r != OK) || (pos >= ldir_ptr->i_stat.st_size)) {
				put_inode(dir_tmp);
				return EINVAL;
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
			return EINVAL;
		}

		if ((strcmp(dir_tmp->i_name, string) == 0) ||
		    (strcmp(dir_tmp->i_name, "..") && strcmp(string, "..") == 0)) {
			if (dir_tmp->i_stat.st_ino == v_pri.inode_root->i_stat.st_ino) {
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


/*===========================================================================*
 *                             parse_path				     *
 *===========================================================================*/
static int parse_path(
ino_t dir_ino,
ino_t root_ino,
int flags,
struct inode **res_inop,
size_t *offsetp,
int *symlinkp
) {
	int r;
	char string[NAME_MAX+1];
	char *cp, *ncp;
	struct inode *start_dir = 0, *old_dir = 0;

	/* Find starting inode inode according to the request message */
	if ((start_dir = find_inode(dir_ino)) == NULL) {
		printf("ISOFS: couldn't find starting inode %llu\n", dir_ino);
		return ENOENT;
	}

	cp = user_path;
	dup_inode(start_dir);

	/* Scan the path component by component. */
	while (TRUE) {
		if (cp[0] == '\0') {
			/* Empty path */
			*res_inop = start_dir;
			*offsetp += cp-user_path;

			/* Return EENTERMOUNT if we are at a mount point */
			if (start_dir->i_mountpoint)
				return EENTERMOUNT;

			return OK;
		}

		if (cp[0] == '/') {
			/*
			* Special case code. If the remaining path consists of
			* just slashes, we need to look up '.'
			*/
			while(cp[0] == '/')
				cp++;
			if (cp[0] == '\0') {
				strlcpy(string, ".", NAME_MAX + 1);
				ncp = cp;
			}
			else
				ncp = get_name(cp, string);
		}
		else
			/* Just get the first component */
			ncp = get_name(cp, string);

		/* Special code for '..'. A process is not allowed to leave a chrooted
		* environment. A lookup of '..' at the root of a mounted filesystem
		* has to return ELEAVEMOUNT.
		*/
		if (strcmp(string, "..") == 0) {

			/* This condition is not necessary since it will never be the root filesystem */
			/*       if (start_dir == v_pri.inode_root) { */
			/* 	cp = ncp; */
			/* 	continue;	/\* Just ignore the '..' at a process' */
			/* 			 * root. */
			/* 			 *\/ */
			/*       } */

			if (start_dir == v_pri.inode_root) {
				/* Climbing up mountpoint. */
				put_inode(start_dir);
				*res_inop = NULL;
				*offsetp += cp-user_path;
				return ELEAVEMOUNT;
			}
		}
		else {
			/* Only check for a mount point if we are not looking for '..'. */
			if (start_dir->i_mountpoint) {
				*res_inop = start_dir;
				*offsetp += cp-user_path;
				return EENTERMOUNT;
			}
		}

		/* There is more path.  Keep parsing. */
		old_dir = start_dir;

		r = advance(old_dir, string, &start_dir);

		if (r != OK) {
			put_inode(old_dir);
			return r;
		}

		/* The call to advance() succeeded.  Fetch next component. */
		if (S_ISLNK(start_dir->i_stat.st_mode)) {

			if (ncp[0] == '\0' && (flags & PATH_RET_SYMLINK)) {
				put_inode(old_dir);
				*res_inop = start_dir;
				*offsetp += ncp - user_path;
				return OK;
			}

			/* Extract path name from the symlink file */
			r = ltraverse(start_dir, ncp);
			ncp = user_path;
			*offsetp = 0;

			/* Symloop limit reached? */
			if (++(*symlinkp) > _POSIX_SYMLOOP_MAX)
				r = ELOOP;

			if (r != OK) {
				put_inode(old_dir);
				put_inode(start_dir);
				return r;
			}

			if (ncp[0] == '/') {
				put_inode(old_dir);
				put_inode(start_dir);
				return ESYMLINK;
			}

			put_inode(start_dir);
			dup_inode(old_dir);
			start_dir = old_dir;
		}

		put_inode(old_dir);
		cp = ncp;
	}
}

/*===========================================================================*
 *                             ltraverse				     *
 *===========================================================================*/
static int ltraverse(
struct inode *rip,		/* symbolic link */
char *suffix)			/* current remaining path. Has to point in the
				 * user_path buffer
				 */
{
	/* Traverse a symbolic link. Copy the link text from the inode and insert
	 * the text into the path. Return error code or report success. Base
	 * directory has to be determined according to the first character of the
	 * new pathname.
	 */

	size_t llen;		/* length of link */
	size_t slen;		/* length of suffix */
	char *sp;		/* start of link text */

	llen = strlen(rip->s_link);
	sp = rip->s_link;
	slen = strlen(suffix);

	/* The path we're parsing looks like this:
	 * /already/processed/path/<link> or
	 * /already/processed/path/<link>/not/yet/processed/path
	 * After expanding the <link>, the path will look like
	 * <expandedlink> or
	 * <expandedlink>/not/yet/processed
	 * In both cases user_path must have enough room to hold <expandedlink>.
	 * However, in the latter case we have to move /not/yet/processed to the
	 * right place first, before we expand <link>. When strlen(<expandedlink>) is
	 * smaller than strlen(/already/processes/path), we move the suffix to the
	 * left. Is strlen(<expandedlink>) greater then we move it to the right. Else
	 * we do nothing.
	 */

	if (slen > 0) { /* Do we have path after the link? */
		/* For simplicity we require that suffix starts with a slash */
		if (suffix[0] != '/') {
			panic("ltraverse: suffix does not start with a slash");
		}

		/* To be able to expand the <link>, we have to move the 'suffix'
		 * to the right place.
		 */
		if (slen + llen + 1 > sizeof(user_path))
			return ENAMETOOLONG;/* <expandedlink>+suffix+\0 does not fit*/
		if ((unsigned) (suffix - user_path) != llen) {
			/* Move suffix left or right */
			memmove(&user_path[llen], suffix, slen+1);
		}
	}
	else {
		if (llen + 1 > sizeof(user_path))
			return ENAMETOOLONG; /* <expandedlink> + \0 does not fix */

		/* Set terminating nul */
		user_path[llen]= '\0';
	}

	/* Everything is set, now copy the expanded link to user_path */
	memmove(user_path, sp, llen);

	return OK;
}

/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
int advance(
struct inode *dirp,		/* inode for directory to be searched */
char string[NAME_MAX],			/* component name to look for */
struct inode **resp)		/* resulting inode */
{
	/* Given a directory and a component of a path, look up the component in
	 * the directory, find the inode, open it, and return a pointer to its inode
	 * slot.
	 */

	struct inode *rip = NULL;
	int r;
	ino_t numb;

	/* If 'string' is empty, yield same inode straight away. */
	if (string[0] == '\0')
		return ENOENT;

	/* Check for NULL. */
	if (dirp == NULL)
		return EINVAL;

	/* If 'string' is not present in the directory, signal error. */
	if ( (r = search_dir(dirp, string, &numb)) != OK)
		return r;

	/* The component has been found in the directory.  Get inode. */
	if ( (rip = get_inode((int) numb)) == NULL)
		return err_code;

	*resp= rip;
	return OK;
}

/*===========================================================================*
 *				get_name				     *
 *===========================================================================*/
static char *get_name(
char *path_name,		/* path name to parse */
char string[NAME_MAX+1])	/* component extracted from 'old_name' */
{
	/* Given a pointer to a path name in fs space, 'path_name', copy the first
	 * component to 'string' (truncated if necessary, always nul terminated).
	 * A pointer to the string after the first component of the name as yet
	 * unparsed is returned.  Roughly speaking,
	 * 'get_name' = 'path_name' - 'string'.
	 *
	 * This routine follows the standard convention that /usr/ast, /usr//ast,
	 * //usr///ast and /usr/ast/ are all equivalent.
	 */
	size_t len;
	char *cp, *ep;

	cp = path_name;

	/* Skip leading slashes */
	while (cp[0] == '/')
		cp++;

	/* Find the end of the first component */
	ep = cp;
	while(ep[0] != '\0' && ep[0] != '/')
		ep++;

	len = ep-cp;

	/* Truncate the amount to be copied if it exceeds NAME_MAX */
	if (len > NAME_MAX)
		len = NAME_MAX;

	/* Special case of the string at cp is empty */
	if (len == 0)
		/* Return "." */
		strlcpy(string, ".", NAME_MAX + 1);
	else {
		memcpy(string, cp, len);
		string[len]= '\0';
	}

	return ep;
}

