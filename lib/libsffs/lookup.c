/* This file provides path-to-inode lookup functionality.
 *
 * The entry points into this file are:
 *   do_lookup		perform the LOOKUP file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

static int get_mask(vfs_ucred_t *ucred);
static int access_as_dir(struct inode *ino, struct sffs_attr *attr, int
	uid, int mask);
static int next_name(char **ptr, char **start, char name[NAME_MAX+1]);
static int go_up(char path[PATH_MAX], struct inode *ino, struct inode
	**res_ino, struct sffs_attr *attr);
static int go_down(char path[PATH_MAX], struct inode *ino, char *name,
	struct inode **res_ino, struct sffs_attr *attr);

/*===========================================================================*
 *				get_mask				     *
 *===========================================================================*/
static int get_mask(ucred)
vfs_ucred_t *ucred;		/* credentials of the caller */
{
  /* Given the caller's credentials, precompute a search access mask to test
   * against directory modes.
   */
  int i;

  if (ucred->vu_uid == sffs_params->p_uid) return S_IXUSR;

  if (ucred->vu_gid == sffs_params->p_gid) return S_IXGRP;

  for (i = 0; i < ucred->vu_ngroups; i++)
	if (ucred->vu_sgroups[i] == sffs_params->p_gid) return S_IXGRP;

  return S_IXOTH;
}

/*===========================================================================*
 *				access_as_dir				     *
 *===========================================================================*/
static int access_as_dir(ino, attr, uid, mask)
struct inode *ino;		/* the inode to test */
struct sffs_attr *attr;		/* attributes of the inode */
int uid;			/* UID of the caller */
int mask;			/* search access mask of the caller */
{
/* Check whether the given inode may be accessed as directory.
 * Return OK or an appropriate error code.
 */
  mode_t mode;

  assert(attr->a_mask & SFFS_ATTR_MODE);

  /* The inode must be a directory to begin with. */
  if (!IS_DIR(ino)) return ENOTDIR;

  /* The caller must have search access to the directory. Root always does. */
  if (uid == 0) return OK;

  mode = get_mode(ino, attr->a_mode);

  return (mode & mask) ? OK : EACCES;
}

/*===========================================================================*
 *				next_name				     *
 *===========================================================================*/
static int next_name(ptr, start, name)
char **ptr;			/* cursor pointer into path (in, out) */
char **start;			/* place to store start of name */
char name[NAME_MAX+1];		/* place to store name */
{
/* Get the next path component from a path.
 */
  char *p;
  int i;

  for (p = *ptr; *p == '/'; p++);

  *start = p;

  if (*p) {
	for (i = 0; *p && *p != '/' && i <= NAME_MAX; p++, i++)
		name[i] = *p;

	if (i > NAME_MAX)
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
static int go_up(path, ino, res_ino, attr)
char path[PATH_MAX];		/* path to take the last part from */
struct inode *ino;		/* inode of the current directory */
struct inode **res_ino;		/* place to store resulting inode */
struct sffs_attr *attr;		/* place to store inode attributes */
{
/* Given an inode, progress into the parent directory.
 */
  struct inode *parent;
  int r;

  pop_path(path);

  parent = ino->i_parent;
  assert(parent != NULL);

  if ((r = verify_path(path, parent, attr, NULL)) != OK)
	return r;

  get_inode(parent);

  *res_ino = parent;

  return r;
}

/*===========================================================================*
 *				go_down					     *
 *===========================================================================*/
static int go_down(path, parent, name, res_ino, attr)
char path[PATH_MAX];		/* path to add the name to */
struct inode *parent;		/* inode of the current directory */
char *name;			/* name of the directory entry */
struct inode **res_ino;		/* place to store resulting inode */
struct sffs_attr *attr;		/* place to store inode attributes */
{
/* Given a directory inode and a name, progress into a directory entry.
 */
  struct inode *ino;
  int r, stale = 0;

  if ((r = push_path(path, name)) != OK)
	return r;

  dprintf(("%s: go_down: name '%s', path now '%s'\n", sffs_name, name, path));

  ino = lookup_dentry(parent, name);

  dprintf(("%s: lookup_dentry('%s') returned %p\n", sffs_name, name, ino));

  if (ino != NULL)
	r = verify_path(path, ino, attr, &stale);
  else
	r = sffs_table->t_getattr(path, attr);

  dprintf(("%s: path query returned %d\n", sffs_name, r));

  if (r != OK) {
	if (ino != NULL) {
		put_inode(ino);

		ino = NULL;
	}

	if (!stale)
		return r;
  }

  dprintf(("%s: name '%s'\n", sffs_name, name));

  if (ino == NULL) {
	if ((ino = get_free_inode()) == NULL)
		return ENFILE;

	dprintf(("%s: inode %p ref %d\n", sffs_name, ino, ino->i_ref));

	ino->i_flags = MODE_TO_DIRFLAG(attr->a_mode);

	add_dentry(parent, name, ino);
  }

  *res_ino = ino;
  return OK;
}

/*===========================================================================*
 *				do_lookup				     *
 *===========================================================================*/
int do_lookup()
{
/* Resolve a path string to an inode.
 */
  ino_t dir_ino_nr, root_ino_nr;
  struct inode *cur_ino, *root_ino;
  struct inode *next_ino = NULL;
  struct sffs_attr attr;
  char buf[PATH_MAX], path[PATH_MAX];
  char name[NAME_MAX+1];
  char *ptr, *last;
  vfs_ucred_t ucred;
  mode_t mask;
  size_t len;
  int r;

  dir_ino_nr = m_in.REQ_DIR_INO;
  root_ino_nr = m_in.REQ_ROOT_INO;
  len = m_in.REQ_PATH_LEN;

  /* Fetch the path name. */
  if (len < 1 || len > PATH_MAX)
	return EINVAL;

  r = sys_safecopyfrom(m_in.m_source, m_in.REQ_GRANT, 0,
	(vir_bytes) buf, len);

  if (r != OK)
	return r;

  if (buf[len-1] != 0) {
	printf("%s: VFS did not zero-terminate path!\n", sffs_name);

	return EINVAL;
  }

  /* Fetch the credentials, and generate a search access mask to test against
   * directory modes.
   */
  if (m_in.REQ_FLAGS & PATH_GET_UCRED) {
	if (m_in.REQ_UCRED_SIZE != sizeof(ucred)) {
		printf("%s: bad credential structure size\n", sffs_name);

		return EINVAL;
	}

	r = sys_safecopyfrom(m_in.m_source, m_in.REQ_GRANT2, 0,
		(vir_bytes) &ucred, m_in.REQ_UCRED_SIZE);

	if (r != OK)
		return r;
  }
  else {
	ucred.vu_uid = m_in.REQ_UID;
	ucred.vu_gid = m_in.REQ_GID;
	ucred.vu_ngroups = 0;
  }

  mask = get_mask(&ucred);

  /* Start the actual lookup. */
  dprintf(("%s: lookup: got query '%s'\n", sffs_name, buf));

  if ((cur_ino = find_inode(dir_ino_nr)) == NULL)
	return EINVAL;

  attr.a_mask = SFFS_ATTR_MODE | SFFS_ATTR_SIZE;

  if ((r = verify_inode(cur_ino, path, &attr)) != OK)
	return r;

  get_inode(cur_ino);

  if (root_ino_nr > 0)
	root_ino = find_inode(root_ino_nr);
  else
	root_ino = NULL;

  /* One possible optimization would be to check a path only right before the
   * first ".." in a row, and at the very end (if still necessary). This would
   * have consequences for inode validation, though.
   */
  for (ptr = last = buf; *ptr != 0; ) {
	if ((r = access_as_dir(cur_ino, &attr, ucred.vu_uid, mask)) != OK)
		break;

	if ((r = next_name(&ptr, &last, name)) != OK)
		break;

	dprintf(("%s: lookup: next name '%s'\n", sffs_name, name));

	if (!strcmp(name, ".") ||
			(cur_ino == root_ino && !strcmp(name, "..")))
		continue;

	if (!strcmp(name, "..")) {
		if (IS_ROOT(cur_ino))
			r = ELEAVEMOUNT;
		else
			r = go_up(path, cur_ino, &next_ino, &attr);
	} else {
		r = go_down(path, cur_ino, name, &next_ino, &attr);
	}

	if (r != OK)
		break;

	assert(next_ino != NULL);

	put_inode(cur_ino);

	cur_ino = next_ino;
  }

  dprintf(("%s: lookup: result %d\n", sffs_name, r));

  if (r != OK) {
	put_inode(cur_ino);

	/* We'd need support for these here. We don't have such support. */
	assert(r != EENTERMOUNT && r != ESYMLINK);

	if (r == ELEAVEMOUNT) {
		m_out.RES_OFFSET = (int) (last - buf);
		m_out.RES_SYMLOOP = 0;
	}

	return r;
  }

  m_out.RES_INODE_NR = INODE_NR(cur_ino);
  m_out.RES_MODE = get_mode(cur_ino, attr.a_mode);
  m_out.RES_FILE_SIZE_HI = ex64hi(attr.a_size);
  m_out.RES_FILE_SIZE_LO = ex64lo(attr.a_size);
  m_out.RES_UID = sffs_params->p_uid;
  m_out.RES_GID = sffs_params->p_gid;
  m_out.RES_DEV = NO_DEV;

  return OK;
}
