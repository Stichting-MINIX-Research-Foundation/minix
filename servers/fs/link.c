/* This file handles the LINK and UNLINK system calls.  It also deals with
 * deallocating the storage used by a file when the last UNLINK is done to a
 * file and the blocks must be returned to the free block pool.
 *
 * The entry points into this file are
 *   do_link:         perform the LINK system call
 *   do_unlink:	      perform the UNLINK and RMDIR system calls
 *   do_rename:	      perform the RENAME system call
 *   do_truncate:     perform the TRUNCATE system call
 *   do_ftruncate:    perform the FTRUNCATE system call
 *   truncate_inode:  release the blocks associated with an inode up to a size
 *   freesp_inode:    release a range of blocks without setting the size
 */

#include "fs.h"
#include <sys/stat.h>
#include <string.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"
#include "super.h"

#define SAME 1000

FORWARD _PROTOTYPE( int remove_dir, (struct inode *rldirp, struct inode *rip,
			char dir_name[NAME_MAX])			);
FORWARD _PROTOTYPE( int unlink_file, (struct inode *dirp, struct inode *rip,
			char file_name[NAME_MAX])			);
FORWARD _PROTOTYPE( off_t nextblock, (off_t pos, int zonesize)		);
FORWARD _PROTOTYPE( void zeroblock_half, (struct inode *i, off_t p, int l));
FORWARD _PROTOTYPE( void zeroblock_range, (struct inode *i, off_t p, off_t h));

/* Args to zeroblock_half() */
#define FIRST_HALF	0
#define LAST_HALF	1

/*===========================================================================*
 *				do_link					     *
 *===========================================================================*/
PUBLIC int do_link()
{
/* Perform the link(name1, name2) system call. */

  struct inode *ip, *rip;
  register int r;
  char string[NAME_MAX];
  struct inode *new_ip;

  /* See if 'name' (file to be linked) exists. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ( (rip = eat_path(user_path)) == NIL_INODE) return(err_code);

  /* Check to see if the file has maximum number of links already. */
  r = OK;
  if (rip->i_nlinks >= (rip->i_sp->s_version == V1 ? CHAR_MAX : SHRT_MAX))
	r = EMLINK;

  /* Only super_user may link to directories. */
  if (r == OK)
	if ( (rip->i_mode & I_TYPE) == I_DIRECTORY && !super_user) r = EPERM;

  /* If error with 'name', return the inode. */
  if (r != OK) {
	put_inode(rip);
	return(r);
  }

  /* Does the final directory of 'name2' exist? */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) {
	put_inode(rip);
	return(err_code);
  }
  if ( (ip = last_dir(user_path, string)) == NIL_INODE) r = err_code;

  /* If 'name2' exists in full (even if no space) set 'r' to error. */
  if (r == OK) {
	if ( (new_ip = advance(&ip, string)) == NIL_INODE) {
		r = err_code;
		if (r == ENOENT) r = OK;
	} else {
		put_inode(new_ip);
		r = EEXIST;
	}
  }

  /* Check for links across devices. */
  if (r == OK)
	if (rip->i_dev != ip->i_dev) r = EXDEV;

  /* Try to link. */
  if (r == OK)
	r = search_dir(ip, string, &rip->i_num, ENTER);

  /* If success, register the linking. */
  if (r == OK) {
	rip->i_nlinks++;
	rip->i_update |= CTIME;
	rip->i_dirt = DIRTY;
  }

  /* Done.  Release both inodes. */
  put_inode(rip);
  put_inode(ip);
  return(r);
}

/*===========================================================================*
 *				do_unlink				     *
 *===========================================================================*/
PUBLIC int do_unlink()
{
/* Perform the unlink(name) or rmdir(name) system call. The code for these two
 * is almost the same.  They differ only in some condition testing.  Unlink()
 * may be used by the superuser to do dangerous things; rmdir() may not.
 */

  register struct inode *rip;
  struct inode *rldirp;
  int r;
  char string[NAME_MAX];

  /* Get the last directory in the path. */
  if (fetch_name(m_in.name, m_in.name_length, M3) != OK) return(err_code);
  if ( (rldirp = last_dir(user_path, string)) == NIL_INODE)
	return(err_code);

  /* The last directory exists.  Does the file also exist? */
  r = OK;
  if ( (rip = advance(&rldirp, string)) == NIL_INODE) r = err_code;

  /* If error, return inode. */
  if (r != OK) {
	put_inode(rldirp);
	return(r);
  }

  /* Do not remove a mount point. */
  if (rip->i_num == ROOT_INODE) {
	put_inode(rldirp);
	put_inode(rip);
	return(EBUSY);
  }

  /* Now test if the call is allowed, separately for unlink() and rmdir(). */
  if (call_nr == UNLINK) {
	/* Only the su may unlink directories, but the su can unlink any dir.*/
	if ( (rip->i_mode & I_TYPE) == I_DIRECTORY && !super_user) r = EPERM;

	/* Don't unlink a file if it is the root of a mounted file system. */
	if (rip->i_num == ROOT_INODE) r = EBUSY;

	/* Actually try to unlink the file; fails if parent is mode 0 etc. */
	if (r == OK) r = unlink_file(rldirp, rip, string);

  } else {
	r = remove_dir(rldirp, rip, string); /* call is RMDIR */
  }

  /* If unlink was possible, it has been done, otherwise it has not. */
  put_inode(rip);
  put_inode(rldirp);
  return(r);
}

/*===========================================================================*
 *				do_rename				     *
 *===========================================================================*/
PUBLIC int do_rename()
{
/* Perform the rename(name1, name2) system call. */

  struct inode *old_dirp, *old_ip;	/* ptrs to old dir, file inodes */
  struct inode *new_dirp, *new_ip;	/* ptrs to new dir, file inodes */
  struct inode *new_superdirp, *next_new_superdirp;
  int r = OK;				/* error flag; initially no error */
  int odir, ndir;			/* TRUE iff {old|new} file is dir */
  int same_pdir;			/* TRUE iff parent dirs are the same */
  char old_name[NAME_MAX], new_name[NAME_MAX];
  ino_t numb;
  int r1;
  
  /* See if 'name1' (existing file) exists.  Get dir and file inodes. */
  if (fetch_name(m_in.name1, m_in.name1_length, M1) != OK) return(err_code);
  if ( (old_dirp = last_dir(user_path, old_name))==NIL_INODE) return(err_code);

  if ( (old_ip = advance(&old_dirp, old_name)) == NIL_INODE) r = err_code;

  /* See if 'name2' (new name) exists.  Get dir and file inodes. */
  if (fetch_name(m_in.name2, m_in.name2_length, M1) != OK) r = err_code;
  if ( (new_dirp = last_dir(user_path, new_name)) == NIL_INODE) r = err_code;
  new_ip = advance(&new_dirp, new_name);	/* not required to exist */

  if (old_ip != NIL_INODE)
	odir = ((old_ip->i_mode & I_TYPE) == I_DIRECTORY);  /* TRUE iff dir */

  /* If it is ok, check for a variety of possible errors. */
  if (r == OK) {
	same_pdir = (old_dirp == new_dirp);

	/* The old inode must not be a superdirectory of the new last dir. */
	if (odir && !same_pdir) {
		dup_inode(new_superdirp = new_dirp);
		while (TRUE) {		/* may hang in a file system loop */
			if (new_superdirp == old_ip) {
				r = EINVAL;
				break;
			}
			next_new_superdirp = advance(&new_superdirp, dot2);
			put_inode(new_superdirp);
			if (next_new_superdirp == new_superdirp)
				break;	/* back at system root directory */
			new_superdirp = next_new_superdirp;
			if (new_superdirp == NIL_INODE) {
				/* Missing ".." entry.  Assume the worst. */
				r = EINVAL;
				break;
			}
		} 	
		put_inode(new_superdirp);
	}	

	/* The old or new name must not be . or .. */
	if (strcmp(old_name, ".")==0 || strcmp(old_name, "..")==0 ||
	    strcmp(new_name, ".")==0 || strcmp(new_name, "..")==0) r = EINVAL;

	/* Both parent directories must be on the same device. */
	if (old_dirp->i_dev != new_dirp->i_dev) r = EXDEV;

	/* Parent dirs must be writable, searchable and on a writable device */
	if ((r1 = forbidden(old_dirp, W_BIT | X_BIT)) != OK ||
	    (r1 = forbidden(new_dirp, W_BIT | X_BIT)) != OK) r = r1;

	/* Some tests apply only if the new path exists. */
	if (new_ip == NIL_INODE) {
		/* don't rename a file with a file system mounted on it. */
		if (old_ip->i_dev != old_dirp->i_dev) r = EXDEV;
		if (odir && new_dirp->i_nlinks >=
		    (new_dirp->i_sp->s_version == V1 ? CHAR_MAX : SHRT_MAX) &&
		    !same_pdir && r == OK) r = EMLINK;
	} else {
		if (old_ip == new_ip) r = SAME; /* old=new */

		/* has the old file or new file a file system mounted on it? */
		if (old_ip->i_dev != new_ip->i_dev) r = EXDEV;

		ndir = ((new_ip->i_mode & I_TYPE) == I_DIRECTORY); /* dir ? */
		if (odir == TRUE && ndir == FALSE) r = ENOTDIR;
		if (odir == FALSE && ndir == TRUE) r = EISDIR;
	}
  }

  /* If a process has another root directory than the system root, we might
   * "accidently" be moving it's working directory to a place where it's
   * root directory isn't a super directory of it anymore. This can make
   * the function chroot useless. If chroot will be used often we should
   * probably check for it here.
   */

  /* The rename will probably work. Only two things can go wrong now:
   * 1. being unable to remove the new file. (when new file already exists)
   * 2. being unable to make the new directory entry. (new file doesn't exists)
   *     [directory has to grow by one block and cannot because the disk
   *      is completely full].
   */
  if (r == OK) {
	if (new_ip != NIL_INODE) {
		  /* There is already an entry for 'new'. Try to remove it. */
		if (odir) 
			r = remove_dir(new_dirp, new_ip, new_name);
		else 
			r = unlink_file(new_dirp, new_ip, new_name);
	}
	/* if r is OK, the rename will succeed, while there is now an
	 * unused entry in the new parent directory.
	 */
  }

  if (r == OK) {
	/* If the new name will be in the same parent directory as the old one,
	 * first remove the old name to free an entry for the new name,
	 * otherwise first try to create the new name entry to make sure
	 * the rename will succeed.
	 */
	numb = old_ip->i_num;		/* inode number of old file */

  	if (same_pdir) {
		r = search_dir(old_dirp, old_name, (ino_t *) 0, DELETE);
						/* shouldn't go wrong. */
		if (r==OK) (void) search_dir(old_dirp, new_name, &numb, ENTER);
	} else {
		r = search_dir(new_dirp, new_name, &numb, ENTER);
		if (r == OK)
		    (void) search_dir(old_dirp, old_name, (ino_t *) 0, DELETE);
	}
  }
  /* If r is OK, the ctime and mtime of old_dirp and new_dirp have been marked
   * for update in search_dir.
   */

  if (r == OK && odir && !same_pdir) {
	/* Update the .. entry in the directory (still points to old_dirp). */
	numb = new_dirp->i_num;
	(void) unlink_file(old_ip, NIL_INODE, dot2);
	if (search_dir(old_ip, dot2, &numb, ENTER) == OK) {
		/* New link created. */
		new_dirp->i_nlinks++;
		new_dirp->i_dirt = DIRTY;
	}
  }
	
  /* Release the inodes. */
  put_inode(old_dirp);
  put_inode(old_ip);
  put_inode(new_dirp);
  put_inode(new_ip);
  return(r == SAME ? OK : r);
}

/*===========================================================================*
 *				do_truncate				     *
 *===========================================================================*/
PUBLIC int do_truncate()
{
/* truncate_inode() does the actual work of do_truncate() and do_ftruncate().
 * do_truncate() and do_ftruncate() have to get hold of the inode, either
 * by name or fd, do checks on it, and call truncate_inode() to do the
 * work.
 */
	int r;
	struct inode *rip;	/* pointer to inode to be truncated */

	if (fetch_name(m_in.m2_p1, m_in.m2_i1, M1) != OK)
		return err_code;
	if( (rip = eat_path(user_path)) == NIL_INODE)
		return err_code;
	if ( (rip->i_mode & I_TYPE) != I_REGULAR)
		r = EINVAL;
	else
		r = truncate_inode(rip, m_in.m2_l1); 
	put_inode(rip);

	return r;
}

/*===========================================================================*
 *				do_ftruncate				     *
 *===========================================================================*/
PUBLIC int do_ftruncate()
{
/* As with do_truncate(), truncate_inode() does the actual work. */
	struct filp *rfilp;
	if ( (rfilp = get_filp(m_in.m2_i1)) == NIL_FILP)
		return err_code;
	if ( (rfilp->filp_ino->i_mode & I_TYPE) != I_REGULAR)
		return EINVAL;
	return truncate_inode(rfilp->filp_ino, m_in.m2_l1);
}

/*===========================================================================*
 *				truncate_inode				     *
 *===========================================================================*/
PUBLIC int truncate_inode(rip, newsize)
register struct inode *rip;	/* pointer to inode to be truncated */
off_t newsize;			/* inode must become this size */
{
/* Set inode to a certain size, freeing any zones no longer referenced
 * and updating the size in the inode. If the inode is extended, the
 * extra space is a hole that reads as zeroes.
 *
 * Nothing special has to happen to file pointers if inode is opened in
 * O_APPEND mode, as this is different per fd and is checked when 
 * writing is done.
 */
  zone_t zone_size;
  int scale, file_type, waspipe;
  dev_t dev;

  file_type = rip->i_mode & I_TYPE;	/* check to see if file is special */
  if (file_type == I_CHAR_SPECIAL || file_type == I_BLOCK_SPECIAL)
	return EINVAL;
  if(newsize > rip->i_sp->s_max_size)	/* don't let inode grow too big */
	return EFBIG;

  dev = rip->i_dev;		/* device on which inode resides */
  scale = rip->i_sp->s_log_zone_size;
  zone_size = (zone_t) rip->i_sp->s_block_size << scale;

  /* Pipes can shrink, so adjust size to make sure all zones are removed. */
  waspipe = rip->i_pipe == I_PIPE;	/* TRUE if this was a pipe */
  if (waspipe) {
	if(newsize != 0)
		return EINVAL;	/* Only truncate pipes to 0. */
	rip->i_size = PIPE_SIZE(rip->i_sp->s_block_size);
  }

  /* Free the actual space if relevant. */
  if(newsize < rip->i_size)
	  freesp_inode(rip, newsize, rip->i_size);

  /* Next correct the inode size. */
  if(!waspipe) rip->i_size = newsize;
  else wipe_inode(rip);	/* Pipes can only be truncated to 0. */
  rip->i_dirt = DIRTY;

  return OK;
}

/*===========================================================================*
 *				freesp_inode				     *
 *===========================================================================*/
PUBLIC int freesp_inode(rip, start, end)
register struct inode *rip;	/* pointer to inode to be partly freed */
off_t start, end;		/* range of bytes to free (end uninclusive) */
{
/* Cut an arbitrary hole in an inode. The caller is responsible for checking
 * the reasonableness of the inode type of rip. The reason is this is that
 * this function can be called for different reasons, for which different
 * sets of inode types are reasonable. Adjusting the final size of the inode
 * is to be done by the caller too, if wished.
 *
 * Consumers of this function currently are truncate_inode() (used to
 * free indirect and data blocks for any type of inode, but also to
 * implement the ftruncate() and truncate() system calls) and the F_FREESP
 * fcntl().
 */
	off_t p, e;
	int zone_size, dev;

	if(end > rip->i_size)		/* freeing beyond end makes no sense */
		end = rip->i_size;
	if(end <= start)		/* end is uninclusive, so start<end */
		return EINVAL;
        zone_size = rip->i_sp->s_block_size << rip->i_sp->s_log_zone_size;
	dev = rip->i_dev;             /* device on which inode resides */

	/* If freeing doesn't cross a zone boundary, then we may only zero
	 * a range of the block.
	 */
	if(start/zone_size == (end-1)/zone_size) {
		zeroblock_range(rip, start, end-start);
	} else { 
		/* First zero unused part of partly used blocks. */
		if(start%zone_size)
			zeroblock_half(rip, start, LAST_HALF);
		if(end%zone_size && end < rip->i_size)
			zeroblock_half(rip, end, FIRST_HALF);
	}

	/* Now completely free the completely unused blocks.
	 * write_map() will free unused (double) indirect
	 * blocks too. Converting the range to zone numbers avoids
	 * overflow on p when doing e.g. 'p += zone_size'.
	 */
	e = end/zone_size;
	if(end == rip->i_size && (end % zone_size)) e++;
	for(p = nextblock(start, zone_size)/zone_size; p < e; p ++)
		write_map(rip, p*zone_size, NO_ZONE, WMAP_FREE);

	return OK;
}

/*===========================================================================*
 *				nextblock				     *
 *===========================================================================*/
PRIVATE off_t nextblock(pos, zone_size)
off_t pos;
int zone_size;
{
/* Return the first position in the next block after position 'pos'
 * (unless this is the first position in the current block).
 * This can be done in one expression, but that can overflow pos.
 */
	off_t p;
	p = (pos/zone_size)*zone_size;
	if((pos % zone_size)) p += zone_size;	/* Round up. */
	return p;
}

/*===========================================================================*
 *				zeroblock_half				     *
 *===========================================================================*/
PRIVATE void zeroblock_half(rip, pos, half)
struct inode *rip;
off_t pos;
int half;
{
/* Zero the upper or lower 'half' of a block that holds position 'pos'.
 * half can be FIRST_HALF or LAST_HALF.
 *
 * FIRST_HALF: 0..pos-1 will be zeroed
 * LAST_HALF:  pos..blocksize-1 will be zeroed
 */
	int offset, len;

	 /* Offset of zeroing boundary. */
	 offset = pos % rip->i_sp->s_block_size;

	 if(half == LAST_HALF)  {
	   	len = rip->i_sp->s_block_size - offset;
	 } else {
		len = offset;
		pos -= offset;
		offset = 0;
	 }

	zeroblock_range(rip, pos, len);
}

/*===========================================================================*
 *				zeroblock_range				     *
 *===========================================================================*/
PRIVATE void zeroblock_range(rip, pos, len)
struct inode *rip;
off_t pos;
off_t len;
{
/* Zero a range in a block.
 * This function is used to zero a segment of a block, either 
 * FIRST_HALF of LAST_HALF.
 * 
 */
	block_t b;
	struct buf *bp;
	off_t offset;

	if(!len) return; /* no zeroing to be done. */
	if( (b = read_map(rip, pos)) == NO_BLOCK) return;
	if( (bp = get_block(rip->i_dev, b, NORMAL)) == NIL_BUF)
	   panic(__FILE__, "zeroblock_range: no block", NO_NUM);
	offset = pos % rip->i_sp->s_block_size;
	if(offset + len > rip->i_sp->s_block_size)
	   panic(__FILE__, "zeroblock_range: len too long", len);
	memset(bp->b_data + offset, 0, len);
	bp->b_dirt = DIRTY;
	put_block(bp, FULL_DATA_BLOCK);
}

/*===========================================================================*
 *				remove_dir				     *
 *===========================================================================*/
PRIVATE int remove_dir(rldirp, rip, dir_name)
struct inode *rldirp;		 	/* parent directory */
struct inode *rip;			/* directory to be removed */
char dir_name[NAME_MAX];		/* name of directory to be removed */
{
  /* A directory file has to be removed. Five conditions have to met:
   * 	- The file must be a directory
   *	- The directory must be empty (except for . and ..)
   *	- The final component of the path must not be . or ..
   *	- The directory must not be the root of a mounted file system
   *	- The directory must not be anybody's root/working directory
   */

  int r;
  register struct fproc *rfp;

  /* search_dir checks that rip is a directory too. */
  if ((r = search_dir(rip, "", (ino_t *) 0, IS_EMPTY)) != OK) return r;

  if (strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0)return(EINVAL);
  if (rip->i_num == ROOT_INODE) return(EBUSY); /* can't remove 'root' */
  
  for (rfp = &fproc[INIT_PROC_NR + 1]; rfp < &fproc[NR_PROCS]; rfp++)
	if (rfp->fp_pid != PID_FREE &&
	     (rfp->fp_workdir == rip || rfp->fp_rootdir == rip))
		return(EBUSY); /* can't remove anybody's working dir */

  /* Actually try to unlink the file; fails if parent is mode 0 etc. */
  if ((r = unlink_file(rldirp, rip, dir_name)) != OK) return r;

  /* Unlink . and .. from the dir. The super user can link and unlink any dir,
   * so don't make too many assumptions about them.
   */
  (void) unlink_file(rip, NIL_INODE, dot1);
  (void) unlink_file(rip, NIL_INODE, dot2);
  return(OK);
}

/*===========================================================================*
 *				unlink_file				     *
 *===========================================================================*/
PRIVATE int unlink_file(dirp, rip, file_name)
struct inode *dirp;		/* parent directory of file */
struct inode *rip;		/* inode of file, may be NIL_INODE too. */
char file_name[NAME_MAX];	/* name of file to be removed */
{
/* Unlink 'file_name'; rip must be the inode of 'file_name' or NIL_INODE. */

  ino_t numb;			/* inode number */
  int	r;

  /* If rip is not NIL_INODE, it is used to get faster access to the inode. */
  if (rip == NIL_INODE) {
  	/* Search for file in directory and try to get its inode. */
	err_code = search_dir(dirp, file_name, &numb, LOOK_UP);
	if (err_code == OK) rip = get_inode(dirp->i_dev, (int) numb);
	if (err_code != OK || rip == NIL_INODE) return(err_code);
  } else {
	dup_inode(rip);		/* inode will be returned with put_inode */
  }

  r = search_dir(dirp, file_name, (ino_t *) 0, DELETE);

  if (r == OK) {
	rip->i_nlinks--;	/* entry deleted from parent's dir */
	rip->i_update |= CTIME;
	rip->i_dirt = DIRTY;
  }

  put_inode(rip);
  return(r);
}
