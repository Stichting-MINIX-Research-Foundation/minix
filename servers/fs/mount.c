/* This file performs the MOUNT and UMOUNT system calls.
 *
 * The entry points into this file are
 *   do_mount:	perform the MOUNT system call
 *   do_umount:	perform the UMOUNT system call
 */

#include "fs.h"
#include <fcntl.h>
#include <minix/com.h>
#include <sys/stat.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"
#include "super.h"

FORWARD _PROTOTYPE( dev_t name_to_dev, (char *path)			);

/*===========================================================================*
 *				do_mount				     *
 *===========================================================================*/
PUBLIC int do_mount()
{
/* Perform the mount(name, mfile, rd_only) system call. */

  register struct inode *rip, *root_ip;
  struct super_block *xp, *sp;
  dev_t dev;
  mode_t bits;
  int rdir, mdir;		/* TRUE iff {root|mount} file is dir */
  int r, found;

  /* Only the super-user may do MOUNT. */
  if (!super_user) return(EPERM);

  /* If 'name' is not for a block special file, return error. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ( (dev = name_to_dev(user_path)) == NO_DEV) return(err_code);

  /* Scan super block table to see if dev already mounted & find a free slot.*/
  sp = NIL_SUPER;
  found = FALSE;
  for (xp = &super_block[0]; xp < &super_block[NR_SUPERS]; xp++) {
	if (xp->s_dev == dev) found = TRUE;	/* is it mounted already? */
	if (xp->s_dev == NO_DEV) sp = xp;	/* record free slot */
  }
  if (found) return(EBUSY);	/* already mounted */
  if (sp == NIL_SUPER) return(ENFILE);	/* no super block available */

  /* Open the device the file system lives on. */
  if (dev_open(dev, who, m_in.rd_only ? R_BIT : (R_BIT|W_BIT)) != OK) 
  	return(EINVAL);

  /* Make the cache forget about blocks it has open on the filesystem */
  (void) do_sync();
  invalidate(dev);

  /* Fill in the super block. */
  sp->s_dev = dev;		/* read_super() needs to know which dev */
  r = read_super(sp);

  /* Is it recognized as a Minix filesystem? */
  if (r != OK) {
	dev_close(dev);
	sp->s_dev = NO_DEV;
	return(r);
  }

  /* Now get the inode of the file to be mounted on. */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) {
	dev_close(dev);
	sp->s_dev = NO_DEV;
	return(err_code);
  }
  if ( (rip = eat_path(user_path)) == NIL_INODE) {
	dev_close(dev);
	sp->s_dev = NO_DEV;
	return(err_code);
  }

  /* It may not be busy. */
  r = OK;
  if (rip->i_count > 1) r = EBUSY;

  /* It may not be special. */
  bits = rip->i_mode & I_TYPE;
  if (bits == I_BLOCK_SPECIAL || bits == I_CHAR_SPECIAL) r = ENOTDIR;

  /* Get the root inode of the mounted file system. */
  root_ip = NIL_INODE;		/* if 'r' not OK, make sure this is defined */
  if (r == OK) {
	if ( (root_ip = get_inode(dev, ROOT_INODE)) == NIL_INODE) r = err_code;
  }
  if (root_ip != NIL_INODE && root_ip->i_mode == 0) {
  	r = EINVAL;
  }

  /* File types of 'rip' and 'root_ip' may not conflict. */
  if (r == OK) {
	mdir = ((rip->i_mode & I_TYPE) == I_DIRECTORY);  /* TRUE iff dir */
	rdir = ((root_ip->i_mode & I_TYPE) == I_DIRECTORY);
	if (!mdir && rdir) r = EISDIR;
  }

  /* If error, return the super block and both inodes; release the maps. */
  if (r != OK) {
	put_inode(rip);
	put_inode(root_ip);
	(void) do_sync();
	invalidate(dev);
	dev_close(dev);
	sp->s_dev = NO_DEV;
	return(r);
  }

  /* Nothing else can go wrong.  Perform the mount. */
  rip->i_mount = I_MOUNT;	/* this bit says the inode is mounted on */
  sp->s_imount = rip;
  sp->s_isup = root_ip;
  sp->s_rd_only = m_in.rd_only;
  return(OK);
}

/*===========================================================================*
 *				do_umount				     *
 *===========================================================================*/
PUBLIC int do_umount()
{
/* Perform the umount(name) system call. */
  dev_t dev;

  /* Only the super-user may do UMOUNT. */
  if (!super_user) return(EPERM);

  /* If 'name' is not for a block special file, return error. */
  if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
  if ( (dev = name_to_dev(user_path)) == NO_DEV) return(err_code);

  return(unmount(dev));
}

/*===========================================================================*
 *				unmount					     *
 *===========================================================================*/
PUBLIC int unmount(dev)
Dev_t dev;
{
/* Unmount a file system by device number. */
  register struct inode *rip;
  struct super_block *sp, *sp1;
  int count;

  /* See if the mounted device is busy.  Only 1 inode using it should be
   * open -- the root inode -- and that inode only 1 time.
   */
  count = 0;
  for (rip = &inode[0]; rip< &inode[NR_INODES]; rip++)
	if (rip->i_count > 0 && rip->i_dev == dev) count += rip->i_count;
  if (count > 1) return(EBUSY);	/* can't umount a busy file system */

  /* Find the super block. */
  sp = NIL_SUPER;
  for (sp1 = &super_block[0]; sp1 < &super_block[NR_SUPERS]; sp1++) {
	if (sp1->s_dev == dev) {
		sp = sp1;
		break;
	}
  }

  /* Sync the disk, and invalidate cache. */
  (void) do_sync();		/* force any cached blocks out of memory */
  invalidate(dev);		/* invalidate cache entries for this dev */
  if (sp == NIL_SUPER) {
  	return(EINVAL);
  }

  /* Close the device the file system lives on. */
  dev_close(dev);

  /* Finish off the unmount. */
  sp->s_imount->i_mount = NO_MOUNT;	/* inode returns to normal */
  put_inode(sp->s_imount);	/* release the inode mounted on */
  put_inode(sp->s_isup);	/* release the root inode of the mounted fs */
  sp->s_imount = NIL_INODE;
  sp->s_dev = NO_DEV;
  return(OK);
}

/*===========================================================================*
 *				name_to_dev				     *
 *===========================================================================*/
PRIVATE dev_t name_to_dev(path)
char *path;			/* pointer to path name */
{
/* Convert the block special file 'path' to a device number.  If 'path'
 * is not a block special file, return error code in 'err_code'.
 */

  register struct inode *rip;
  register dev_t dev;

  /* If 'path' can't be opened, give up immediately. */
  if ( (rip = eat_path(path)) == NIL_INODE) return(NO_DEV);

  /* If 'path' is not a block special file, return error. */
  if ( (rip->i_mode & I_TYPE) != I_BLOCK_SPECIAL) {
	err_code = ENOTBLK;
	put_inode(rip);
	return(NO_DEV);
  }

  /* Extract the device number. */
  dev = (dev_t) rip->i_zone[0];
  put_inode(rip);
  return(dev);
}
