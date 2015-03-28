/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

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

static int freesp_inode(struct inode *rip, off_t st, off_t end);
static int remove_dir(struct inode *rldirp, struct inode *rip,
	const char *dir_name);
static int unlink_file(struct inode *dirp, struct inode *rip,
	const char *file_name);
static off_t nextblock(off_t pos, int blocksize);
static void zeroblock_half(struct inode *i, off_t p, int l);
static void zeroblock_range(struct inode *i, off_t p, off_t h);

/* Args to zeroblock_half() */
#define FIRST_HALF	0
#define LAST_HALF	1


/*===========================================================================*
 *				fs_link 				     *
 *===========================================================================*/
int fs_link(ino_t dir_nr, char *name, ino_t ino_nr)
{
/* Perform the link(name1, name2) system call. */

  struct inode *ip, *rip;
  register int r;
  struct inode *new_ip;

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, ino_nr)) == NULL)
	  return(EINVAL);

  /* Check to see if the file has maximum number of links already. */
  r = OK;
  if (rip->i_links_count >= USHRT_MAX)
	  r = EMLINK;
  if(rip->i_links_count >= LINK_MAX)
	  r = EMLINK;

  /* Linking to directories is too dangerous to allow. */
  if(r == OK)
	  if( (rip->i_mode & I_TYPE) == I_DIRECTORY)
		  r = EPERM;

  /* If error with 'name', return the inode. */
  if (r != OK) {
	  put_inode(rip);
	  return(r);
  }

  /* Temporarily open the last dir */
  if( (ip = get_inode(fs_dev, dir_nr)) == NULL) {
  	put_inode(rip);
	return(EINVAL);
  }

  if (ip->i_links_count == NO_LINK) {	/* Dir does not actually exist */
  	put_inode(rip);
	put_inode(ip);
	return(ENOENT);
  }

  /* If 'name2' exists in full (even if no space) set 'r' to error. */
  if ((new_ip = advance(ip, name)) == NULL) {
	r = err_code;
	if(r == ENOENT)
		r = OK;
  } else {
	put_inode(new_ip);
	r = EEXIST;
  }

  /* Try to link. */
  if(r == OK)
	  r = search_dir(ip, name, &rip->i_num, ENTER, rip->i_mode & I_TYPE);

  /* If success, register the linking. */
  if(r == OK) {
	  rip->i_links_count++;
	  rip->i_update |= CTIME;
	  rip->i_dirt = IN_DIRTY;
  }

  /* Done.  Release both inodes. */
  put_inode(rip);
  put_inode(ip);
  return(r);
}


/*===========================================================================*
 *				fs_unlink				     *
 *===========================================================================*/
int fs_unlink(ino_t dir_nr, char *name, int call)
{
/* Perform the unlink(name) or rmdir(name) system call. The code for these two
 * is almost the same.  They differ only in some condition testing.
 */
  register struct inode *rip;
  struct inode *rldirp;
  int r;

  /* Temporarily open the dir. */
  if((rldirp = get_inode(fs_dev, dir_nr)) == NULL)
	  return(EINVAL);

  /* The last directory exists.  Does the file also exist? */
  rip = advance(rldirp, name);
  r = err_code;

  /* If error, return inode. */
  if(r != OK) {
	put_inode(rldirp);
	return(r);
  }
  if (rip->i_mountpoint) {
	put_inode(rip);
	put_inode(rldirp);
	return(EBUSY);
  }

  /* Now test if the call is allowed, separately for unlink() and rmdir(). */
  if (call == FSC_UNLINK) {
	  if( (rip->i_mode & I_TYPE) == I_DIRECTORY) r = EPERM;

	  /* Actually try to unlink the file; fails if parent is mode 0 etc. */
	  if (r == OK) r = unlink_file(rldirp, rip, name);
  } else {
	  r = remove_dir(rldirp, rip, name); /* call is RMDIR */
  }

  /* If unlink was possible, it has been done, otherwise it has not. */
  put_inode(rip);
  put_inode(rldirp);
  return(r);
}


/*===========================================================================*
 *                             fs_rdlink                                     *
 *===========================================================================*/
ssize_t fs_rdlink(ino_t ino_nr, struct fsdriver_data *data, size_t bytes)
{
  struct buf *bp = NULL;       /* buffer containing link text */
  char* link_text;             /* either bp->b_data or rip->i_block */
  register struct inode *rip;  /* target inode */
  register int r;              /* return value */

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, ino_nr)) == NULL)
	  return(EINVAL);

  if (rip->i_size >= MAX_FAST_SYMLINK_LENGTH) {
  /* normal symlink */
       	if(!(bp = get_block_map(rip, 0))) {
		r = EIO;
	} else {
		link_text = b_data(bp);
		r = OK;
	}
  } else {
        /* fast symlink, stored in inode */
        link_text = (char*) rip->i_block;
	r = OK;
  }
  if (r == OK) {
	/* Passed all checks */
	if (bytes > rip->i_size)
		bytes = rip->i_size;
	r = fsdriver_copyout(data, 0, link_text, bytes);
	put_block(bp);
	if (r == OK)
		r = bytes;
  }

  put_inode(rip);
  return(r);
}


/*===========================================================================*
 *				remove_dir				     *
 *===========================================================================*/
static int remove_dir(rldirp, rip, dir_name)
struct inode *rldirp;		 	/* parent directory */
struct inode *rip;			/* directory to be removed */
const char *dir_name;			/* name of directory to be removed */
{
  /* A directory file has to be removed. Five conditions have to met:
   * 	- The file must be a directory
   *	- The directory must be empty (except for . and ..)
   *	- The final component of the path must not be . or ..
   *	- The directory must not be the root of a mounted file system (VFS)
   *	- The directory must not be anybody's root/working directory (VFS)
   */
  int r;

  /* search_dir checks that rip is a directory too. */
  if ((r = search_dir(rip, "", NULL, IS_EMPTY, 0)) != OK)
	return r;

  if (rip->i_num == ROOT_INODE) return(EBUSY); /* can't remove 'root' */

  /* Actually try to unlink the file; fails if parent is mode 0 etc. */
  if ((r = unlink_file(rldirp, rip, dir_name)) != OK) return r;

  /* Unlink . and .. from the dir. The super user can link and unlink any dir,
   * so don't make too many assumptions about them.
   */
  (void) unlink_file(rip, NULL, ".");
  (void) unlink_file(rip, NULL, "..");
  return(OK);
}


/*===========================================================================*
 *				unlink_file				     *
 *===========================================================================*/
static int unlink_file(dirp, rip, file_name)
struct inode *dirp;		/* parent directory of file */
struct inode *rip;		/* inode of file, may be NULL too. */
const char *file_name;		/* name of file to be removed */
{
/* Unlink 'file_name'; rip must be the inode of 'file_name' or NULL. */

  ino_t numb;			/* inode number */
  int	r;

  /* If rip is not NULL, it is used to get faster access to the inode. */
  if (rip == NULL) {
	/* Search for file in directory and try to get its inode. */
	err_code = search_dir(dirp, file_name, &numb, LOOK_UP, 0);
	if (err_code == OK) rip = get_inode(dirp->i_dev, (int) numb);
	if (err_code != OK || rip == NULL) return(err_code);
  } else {
	dup_inode(rip);		/* inode will be returned with put_inode */
  }

  r = search_dir(dirp, file_name, NULL, DELETE, 0);

  if (r == OK) {
	rip->i_links_count--;	/* entry deleted from parent's dir */
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
  struct inode *old_dirp, *old_ip;	/* ptrs to old dir, file inodes */
  struct inode *new_dirp, *new_ip;	/* ptrs to new dir, file inodes */
  struct inode *new_superdirp, *next_new_superdirp;
  int r = OK;				/* error flag; initially no error */
  int odir, ndir;			/* TRUE iff {old|new} file is dir */
  int same_pdir = 0;			/* TRUE iff parent dirs are the same */
  ino_t numb;

  /* Get old dir inode */
  if( (old_dirp = get_inode(fs_dev, old_dir_nr)) == NULL)
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

  /* Get new dir inode */
  if ((new_dirp = get_inode(fs_dev, new_dir_nr)) == NULL){
	put_inode(old_ip);
	put_inode(old_dirp);
	return(err_code);
  } else {
	if (new_dirp->i_links_count == NO_LINK) { /* Dir does not exist */
		put_inode(old_ip);
		put_inode(old_dirp);
		put_inode(new_dirp);
		return(ENOENT);
	}
  }

  new_ip = advance(new_dirp, new_name); /* not required to exist */

  /* If the node does exist, make sure it's not a mountpoint. */
  if (new_ip != NULL && new_ip->i_mountpoint) {
	put_inode(new_ip);
	new_ip = NULL;
	r = EBUSY;
  }

  if(old_ip != NULL)
	  odir = ((old_ip->i_mode & I_TYPE) == I_DIRECTORY); /* TRUE iff dir */
  else
	  odir = FALSE;

  /* If it is ok, check for a variety of possible errors. */
  if(r == OK) {
	same_pdir = (old_dirp == new_dirp);

	/* The old inode must not be a superdirectory of the new last dir. */
	if (odir && !same_pdir) {
		dup_inode(new_superdirp = new_dirp);
		while (TRUE) {	/* may hang in a file system loop */
			if (new_superdirp == old_ip) {
				put_inode(new_superdirp);
				r = EINVAL;
				break;
			}
			next_new_superdirp = advance(new_superdirp, "..");

			put_inode(new_superdirp);
			if(next_new_superdirp == new_superdirp) {
				put_inode(new_superdirp);
				break;
			}
			if(next_new_superdirp->i_num == ROOT_INODE) {
				/* imitate that we are back at the root,
				 * cross device checked already on VFS */
				put_inode(next_new_superdirp);
				err_code = OK;
				break;
			}
			new_superdirp = next_new_superdirp;
			if(new_superdirp == NULL) {
				/* Missing ".." entry.  Assume the worst. */
				r = EINVAL;
				break;
			}
		}
	}

	/* Some tests apply only if the new path exists. */
	if(new_ip == NULL) {
		if(odir && (new_dirp->i_links_count >= SHRT_MAX ||
			    new_dirp->i_links_count >= LINK_MAX) &&
		   !same_pdir && r == OK) {
			r = EMLINK;
		}
	} else {
		if(old_ip == new_ip) r = SAME; /* old=new */

		ndir = ((new_ip->i_mode & I_TYPE) == I_DIRECTORY);/* dir ? */
		if(odir == TRUE && ndir == FALSE) r = ENOTDIR;
		if(odir == FALSE && ndir == TRUE) r = EISDIR;
	}
  }

  /* If a process has another root directory than the system root, we might
   * "accidently" be moving it's working directory to a place where it's
   * root directory isn't a super directory of it anymore. This can make
   * the function chroot useless. If chroot will be used often we should
   * probably check for it here. */

  /* The rename will probably work. Only two things can go wrong now:
   * 1. being unable to remove the new file. (when new file already exists)
   * 2. being unable to make the new directory entry. (new file doesn't exists)
   *     [directory has to grow by one block and cannot because the disk
   *      is completely full].
   */
  if(r == OK) {
	if(new_ip != NULL) {
		/* There is already an entry for 'new'. Try to remove it. */
		if(odir)
			r = remove_dir(new_dirp, new_ip, new_name);
		else
			r = unlink_file(new_dirp, new_ip, new_name);
	}
	/* if r is OK, the rename will succeed, while there is now an
	 * unused entry in the new parent directory. */
  }

  if(r == OK) {
	  /* If the new name will be in the same parent directory as the old
	   * one, first remove the old name to free an entry for the new name,
	   * otherwise first try to create the new name entry to make sure
	   * the rename will succeed.
	   */
	numb = old_ip->i_num;		/* inode number of old file */

	if(same_pdir) {
		r = search_dir(old_dirp,old_name, NULL, DELETE, 0);
						/* shouldn't go wrong. */
		if(r == OK)
			(void) search_dir(old_dirp, new_name, &numb, ENTER,
					  old_ip->i_mode & I_TYPE);
	} else {
		r = search_dir(new_dirp, new_name, &numb, ENTER,
					old_ip->i_mode & I_TYPE);
		if(r == OK) {
			(void) search_dir(old_dirp, old_name, NULL,
					  DELETE, 0);
		}
	}
  }
  /* If r is OK, the ctime and mtime of old_dirp and new_dirp have been marked
   * for update in search_dir. */

  if(r == OK && odir && !same_pdir) {
	/* Update the .. entry in the directory (still points to old_dirp).*/
	numb = new_dirp->i_num;
	(void) unlink_file(old_ip, NULL, "..");
	if(search_dir(old_ip, "..", &numb, ENTER, I_DIRECTORY) == OK) {
		/* New link created. */
		new_dirp->i_links_count++;
		new_dirp->i_dirt = IN_DIRTY;
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
 *				fs_trunc				     *
 *===========================================================================*/
int fs_trunc(ino_t ino_nr, off_t start, off_t end)
{
  struct inode *rip;
  int r;

  if( (rip = find_inode(fs_dev, ino_nr)) == NULL)
	  return(EINVAL);

  if (end == 0)
	  r = truncate_inode(rip, start);
  else
	  r = freesp_inode(rip, start, end);

  return(r);
}


/*===========================================================================*
 *				truncate_inode				     *
 *===========================================================================*/
int truncate_inode(rip, newsize)
register struct inode *rip;	/* pointer to inode to be truncated */
off_t newsize;			/* inode must become this size */
{
/* Set inode to a certain size, freeing any blocks no longer referenced
 * and updating the size in the inode. If the inode is extended, the
 * extra space is a hole that reads as zeroes.
 *
 * Nothing special has to happen to file pointers if inode is opened in
 * O_APPEND mode, as this is different per fd and is checked when
 * writing is done.
 */
  int r;
  mode_t file_type;

  discard_preallocated_blocks(rip);

  file_type = rip->i_mode & I_TYPE;	/* check to see if file is special */
  if (file_type == I_CHAR_SPECIAL || file_type == I_BLOCK_SPECIAL)
	return(EINVAL);
  if (newsize > rip->i_sp->s_max_size)	/* don't let inode grow too big */
	return(EFBIG);

  /* Free the actual space if truncating. */
  if (newsize < rip->i_size) {
	if ((r = freesp_inode(rip, newsize, rip->i_size)) != OK)
		return(r);
  }

  /* Clear the rest of the last block if expanding. */
  if (newsize > rip->i_size) zeroblock_half(rip, rip->i_size, LAST_HALF);

  /* Next correct the inode size. */
  rip->i_size = newsize;
  rip->i_update |= CTIME | MTIME;
  rip->i_dirt = IN_DIRTY;

  return(OK);
}


/*===========================================================================*
 *				freesp_inode				     *
 *===========================================================================*/
static int freesp_inode(rip, start, end)
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
  int r;
  unsigned short block_size = rip->i_sp->s_block_size;
  int zero_last, zero_first;

  discard_preallocated_blocks(rip);

  if (rip->i_blocks == 0) {
  /* Either hole or symlink. Freeing fast symlink using
   * write_map() causes segfaults since it doesn't use any
   * blocks, but uses i_block[] to store target.
   */
	return(OK);
  }

  if(end > rip->i_size)		/* freeing beyond end makes no sense */
	end = rip->i_size;
  if(end <= start)		/* end is uninclusive, so start<end */
	return(EINVAL);

  /* If freeing doesn't cross a block boundary, then we may only zero
   * a range of the block.
   */
  zero_last = start % block_size;
  zero_first = end % block_size && end < rip->i_size;
  if (start/block_size == (end-1)/block_size && (zero_last || zero_first)) {
	zeroblock_range(rip, start, end-start);
  } else {
	/* First zero unused part of partly used blocks. */
	if (zero_last)
		zeroblock_half(rip, start, LAST_HALF);
	if (zero_first)
		zeroblock_half(rip, end, FIRST_HALF);

	/* Now completely free the completely unused blocks.
	 * write_map() will free unused indirect
	 * blocks too. Converting the range to block numbers avoids
	 * overflow on p when doing e.g. 'p += block_size'.
	 */
	e = end / block_size;
	if (end == rip->i_size && (end % block_size))
		e++;
	for (p = nextblock(start, block_size)/block_size; p < e; p++) {
		if ((r = write_map(rip, p*block_size, NO_BLOCK, WMAP_FREE)) != OK)
			return(r);
	}
  }

  rip->i_update |= CTIME | MTIME;
  rip->i_dirt = IN_DIRTY;

  return(OK);
}


/*===========================================================================*
 *				nextblock				     *
 *===========================================================================*/
static off_t nextblock(pos, block_size)
off_t pos;
unsigned short block_size;
{
/* Return the first position in the next block after position 'pos'
 * (unless this is the first position in the current block).
 * This can be done in one expression, but that can overflow pos.
 */
  off_t p;
  p = (pos / block_size) * block_size;
  if (pos % block_size) p += block_size;	/* Round up. */
  return(p);
}


/*===========================================================================*
 *				zeroblock_half				     *
 *===========================================================================*/
static void zeroblock_half(rip, pos, half)
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
  off_t offset, len;

  /* Offset of zeroing boundary. */
  offset = pos % rip->i_sp->s_block_size;

  if(half == LAST_HALF)  {
	len = rip->i_sp->s_block_size - offset;
  } else {
	len = offset;
	pos -= offset;
  }

  zeroblock_range(rip, pos, len);
}


/*===========================================================================*
 *				zeroblock_range				     *
 *===========================================================================*/
static void zeroblock_range(rip, pos, len)
struct inode *rip;
off_t pos;
off_t len;
{
/* Zero a range in a block.
 * This function is used to zero a segment of a block.
 */
  struct buf *bp;
  off_t offset;

  if (!len) return; /* no zeroing to be done. */
  if (!(bp = get_block_map(rip, rounddown(pos, rip->i_sp->s_block_size))))
	return; /* skip holes */
  offset = pos % rip->i_sp->s_block_size;
  if (offset + len > rip->i_sp->s_block_size)
	panic("zeroblock_range: len too long: %lld", len);
  memset(b_data(bp) + offset, 0, len);
  lmfs_markdirty(bp);
  put_block(bp);
}
