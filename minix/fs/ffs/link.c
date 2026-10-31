#include "fs.h"
#include <sys/stat.h>
#include <string.h>
#include <minix/com.h>
#include "buf.h"
#include "inode.h"
#include "super.h"
#include <minix/vfsif.h>
#include <sys/param.h>

#define SAME 1000

static int remove_dir(struct inode *rldirp, struct inode *rip,
	const char *dir_name);
static int unlink_file(struct inode *dirp, struct inode *rip,
	const char *file_name);

/*===========================================================================*
 *				fs_link 				     *
 *===========================================================================*/
int fs_link(ino_t dir_nr, char *name, ino_t ino_nr)
{
/* Perform the link(name1, name2) system call. */
  struct inode *ip, *rip, *new_ip;
  int r;

  if ((rip = get_inode(fs_dev, ino_nr)) == NULL)
	return(EINVAL);

  r = OK;
  if (rip->i_din.di_nlink >= LINK_MAX)
	r = EMLINK;

  /* Linking to directories is too dangerous to allow. */
  if (r == OK && (rip->i_din.di_mode & I_TYPE) == I_DIRECTORY)
	r = EPERM;

  if (r != OK) {
	put_inode(rip);
	return(r);
  }

  if ((ip = get_inode(fs_dev, dir_nr)) == NULL) {
	put_inode(rip);
	return(EINVAL);
  }

  if (ip->i_din.di_nlink == NO_LINK) {	/* parent does not exist */
	put_inode(rip);
	put_inode(ip);
	return(ENOENT);
  }

  /* If 'name' already exists, that is an error. */
  if ((new_ip = advance(ip, name)) == NULL) {
	r = err_code;
	if (r == ENOENT)
		r = OK;
  } else {
	put_inode(new_ip);
	r = EEXIST;
  }

  if (r == OK)
	r = search_dir(ip, name, &rip->i_num, ENTER,
		rip->i_din.di_mode & I_TYPE);

  if (r == OK) {
	rip->i_din.di_nlink++;
	rip->i_update |= CTIME;
	rip->i_dirt = IN_DIRTY;
  }

  put_inode(rip);
  put_inode(ip);
  return(r);
}

/*===========================================================================*
 *				fs_unlink				     *
 *===========================================================================*/
int fs_unlink(ino_t dir_nr, char *name, int call)
{
/* Perform the unlink(name) or rmdir(name) system call. */
  struct inode *rip, *rldirp;
  int r;

  if ((rldirp = get_inode(fs_dev, dir_nr)) == NULL)
	return(EINVAL);

  rip = advance(rldirp, name);
  r = err_code;
  if (r != OK) {
	put_inode(rldirp);
	return(r);
  }

  if (rip->i_mountpoint) {
	put_inode(rip);
	put_inode(rldirp);
	return(EBUSY);
  }

  if (call == FSC_UNLINK) {
	if ((rip->i_din.di_mode & I_TYPE) == I_DIRECTORY)
		r = EPERM;
	if (r == OK)
		r = unlink_file(rldirp, rip, name);
  } else {
	r = remove_dir(rldirp, rip, name);	/* call is FSC_RMDIR */
  }

  put_inode(rip);
  put_inode(rldirp);
  return(r);
}

/*===========================================================================*
 *				remove_dir				     *
 *===========================================================================*/
static int remove_dir(struct inode *rldirp, struct inode *rip,
	const char *dir_name)
{
/* A directory is removed only if it is empty (except for . and ..), is not the
 * root, and is not a mount point. */
  int r;

  /* search_dir checks that rip is a directory too. */
  if ((r = search_dir(rip, "", NULL, IS_EMPTY, 0)) != OK)
	return(r);

  if (rip->i_num == ROOT_INODE)
	return(EBUSY);

  if ((r = unlink_file(rldirp, rip, dir_name)) != OK)
	return(r);

  /* Remove "." and ".." from the now-disconnected directory.  Unlinking ".."
   * already drops the parent's backlink (it decrements the inode that ".."
   * resolves to, i.e. rldirp), so no extra decrement is done here. */
  (void) unlink_file(rip, NULL, ".");
  (void) unlink_file(rip, NULL, "..");

  return(OK);
}

/*===========================================================================*
 *				unlink_file				     *
 *===========================================================================*/
static int unlink_file(struct inode *dirp, struct inode *rip,
	const char *file_name)
{
/* Unlink 'file_name'; 'rip' must be the inode of 'file_name' or NULL. */
  ino_t numb;
  int r;

  if (rip == NULL) {
	err_code = search_dir(dirp, file_name, &numb, LOOK_UP, 0);
	if (err_code == OK)
		rip = get_inode(dirp->i_dev, numb);
	if (err_code != OK || rip == NULL)
		return(err_code);
  } else {
	dup_inode(rip);		/* returned via put_inode below */
  }

  r = search_dir(dirp, file_name, NULL, DELETE, 0);

  if (r == OK) {
	rip->i_din.di_nlink--;
	rip->i_update |= CTIME;
	rip->i_dirt = IN_DIRTY;
  }

  put_inode(rip);
  return(r);
}

/*===========================================================================*
 *				fs_rename				     *
 *===========================================================================*/
int fs_rename(ino_t old_dir_nr, char *old_name, ino_t new_dir_nr,
	char *new_name)
{
/* Perform the rename(name1, name2) system call. */
  struct inode *old_dirp, *old_ip;
  struct inode *new_dirp, *new_ip;
  struct inode *new_superdirp, *next_new_superdirp;
  int r = OK;
  int odir, ndir;
  int same_pdir = 0;
  ino_t numb;

  if ((old_dirp = get_inode(fs_dev, old_dir_nr)) == NULL)
	return(err_code);

  old_ip = advance(old_dirp, old_name);
  r = err_code;
  if (old_ip == NULL) {
	put_inode(old_dirp);
	return(r);
  }

  if (old_ip->i_mountpoint) {
	put_inode(old_ip);
	put_inode(old_dirp);
	return(EBUSY);
  }

  if ((new_dirp = get_inode(fs_dev, new_dir_nr)) == NULL) {
	put_inode(old_ip);
	put_inode(old_dirp);
	return(err_code);
  }
  if (new_dirp->i_din.di_nlink == NO_LINK) {	/* parent does not exist */
	put_inode(old_ip);
	put_inode(old_dirp);
	put_inode(new_dirp);
	return(ENOENT);
  }

  new_ip = advance(new_dirp, new_name);		/* may legitimately be NULL */

  if (new_ip != NULL && new_ip->i_mountpoint) {
	put_inode(new_ip);
	new_ip = NULL;
	r = EBUSY;
  }

  odir = ((old_ip->i_din.di_mode & I_TYPE) == I_DIRECTORY);

  if (r == OK) {
	same_pdir = (old_dirp == new_dirp);

	/* The old inode must not be a super-directory of the new parent. */
	if (odir && !same_pdir) {
		dup_inode(new_superdirp = new_dirp);
		while (TRUE) {
			if (new_superdirp == old_ip) {
				put_inode(new_superdirp);
				r = EINVAL;
				break;
			}
			next_new_superdirp = advance(new_superdirp, "..");
			put_inode(new_superdirp);
			if (next_new_superdirp == new_superdirp) {
				put_inode(new_superdirp);
				break;
			}
			if (next_new_superdirp == NULL) {
				r = EINVAL;
				break;
			}
			if (next_new_superdirp->i_num == ROOT_INODE) {
				put_inode(next_new_superdirp);
				break;
			}
			new_superdirp = next_new_superdirp;
		}
	}

	if (new_ip == NULL) {
		if (odir && new_dirp->i_din.di_nlink >= LINK_MAX &&
		    !same_pdir && r == OK)
			r = EMLINK;
	} else {
		if (old_ip == new_ip)
			r = SAME;
		ndir = ((new_ip->i_din.di_mode & I_TYPE) == I_DIRECTORY);
		if (odir && !ndir)
			r = ENOTDIR;
		if (!odir && ndir)
			r = EISDIR;
	}
  }

  if (r == OK) {
	if (new_ip != NULL) {
		/* The destination exists; remove it first. */
		if (odir)
			r = remove_dir(new_dirp, new_ip, new_name);
		else
			r = unlink_file(new_dirp, new_ip, new_name);
	}
  }

  if (r == OK) {
	numb = old_ip->i_num;
	if (same_pdir) {
		r = search_dir(old_dirp, old_name, NULL, DELETE, 0);
		if (r == OK)
			(void) search_dir(old_dirp, new_name, &numb, ENTER,
				old_ip->i_din.di_mode & I_TYPE);
	} else {
		r = search_dir(new_dirp, new_name, &numb, ENTER,
			old_ip->i_din.di_mode & I_TYPE);
		if (r == OK)
			(void) search_dir(old_dirp, old_name, NULL, DELETE, 0);
	}
  }

  if (r == OK && odir && !same_pdir) {
	/* Repoint the moved directory's ".." at its new parent.  Unlinking the
	 * old ".." already dropped old_dirp's backlink, so only the new parent
	 * gains a link here. */
	numb = new_dirp->i_num;
	(void) unlink_file(old_ip, NULL, "..");
	if (search_dir(old_ip, "..", &numb, ENTER, I_DIRECTORY) == OK) {
		new_dirp->i_din.di_nlink++;
		new_dirp->i_dirt = IN_DIRTY;
	}
  }

  put_inode(old_dirp);
  put_inode(old_ip);
  put_inode(new_dirp);
  put_inode(new_ip);
  return(r == SAME ? OK : r);
}
