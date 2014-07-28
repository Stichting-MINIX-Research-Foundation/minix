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
static int remove_dir(struct inode *rldirp, struct inode *rip, char
	dir_name[NAME_MAX + 1]);
static int unlink_file(struct inode *dirp, struct inode *rip, char
	file_name[NAME_MAX + 1]);
static off_t nextblock(off_t pos, int blocksize);
static void zeroblock_half(struct inode *i, off_t p, int l);
static void zeroblock_range(struct inode *i, off_t p, off_t h);

/* Args to zeroblock_half() */
#define FIRST_HALF	0
#define LAST_HALF	1


/*===========================================================================*
 *				fs_link 				     *
 *===========================================================================*/
int fs_link()
{
/* Perform the link(name1, name2) system call. */

  struct inode *ip, *rip;
  register int r;
  char string[NAME_MAX + 1];
  struct inode *new_ip;
  phys_bytes len;

  /* Copy the link name's last component */
  len = fs_m_in.m_vfs_fs_link.path_len; /* including trailing '\0' */
  if (len > NAME_MAX + 1 || len > EXT2_NAME_MAX + 1)
	return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, fs_m_in.m_vfs_fs_link.grant, 0,
		       (vir_bytes) string, (size_t) len);
  if (r != OK) return r;
  NUL(string, len, sizeof(string));

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, fs_m_in.m_vfs_fs_link.inode)) == NULL)
	  return(EINVAL);

  /* Check to see if the file has maximum number of links already. */
  r = OK;
  if (rip->i_links_count >= USHRT_MAX)
	  r = EMLINK;
  if(rip->i_links_count >= LINK_MAX)
	  r = EMLINK;

  /* Only super_user may link to directories. */
  if(r == OK)
	  if( (rip->i_mode & I_TYPE) == I_DIRECTORY && caller_uid != SU_UID)
		  r = EPERM;

  /* If error with 'name', return the inode. */
  if (r != OK) {
	  put_inode(rip);
	  return(r);
  }

  /* Temporarily open the last dir */
  if( (ip = get_inode(fs_dev, fs_m_in.m_vfs_fs_link.dir_ino)) == NULL) {
  	put_inode(rip);
	return(EINVAL);
  }

  if (ip->i_links_count == NO_LINK) {	/* Dir does not actually exist */
  	put_inode(rip);
	put_inode(ip);
	return(ENOENT);
  }

  /* If 'name2' exists in full (even if no space) set 'r' to error. */
  if ((new_ip = advance(ip, string, IGN_PERM)) == NULL) {
	r = err_code;
	if(r == ENOENT)
		r = OK;
  } else {
	put_inode(new_ip);
	r = EEXIST;
  }

  /* Try to link. */
  if(r == OK)
	  r = search_dir(ip, string, &rip->i_num, ENTER, IGN_PERM,
			 rip->i_mode & I_TYPE);

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
int fs_unlink()
{
/* Perform the unlink(name) or rmdir(name) system call. The code for these two
 * is almost the same.  They differ only in some condition testing.  Unlink()
 * may be used by the superuser to do dangerous things; rmdir() may not.
 */
  register struct inode *rip;
  struct inode *rldirp;
  int r;
  char string[NAME_MAX + 1];
  phys_bytes len;

  /* Copy the last component */
  len = fs_m_in.m_vfs_fs_unlink.path_len; /* including trailing '\0' */
  if (len > NAME_MAX + 1 || len > EXT2_NAME_MAX + 1)
	return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, fs_m_in.m_vfs_fs_unlink.grant,
                       (vir_bytes) 0, (vir_bytes) string, (size_t) len);
  if (r != OK) return r;
  NUL(string, len, sizeof(string));

  /* Temporarily open the dir. */
  if((rldirp = get_inode(fs_dev, fs_m_in.m_vfs_fs_unlink.inode)) == NULL)
	  return(EINVAL);

  /* The last directory exists.  Does the file also exist? */
  rip = advance(rldirp, string, IGN_PERM);
  r = err_code;

  /* If error, return inode. */
  if(r != OK) {
	/* Mount point? */
	if (r == EENTERMOUNT || r == ELEAVEMOUNT) {
		put_inode(rip);
		r = EBUSY;
	}
	put_inode(rldirp);
	return(r);
  }

  /* Now test if the call is allowed, separately for unlink() and rmdir(). */
  if(fs_m_in.m_type == REQ_UNLINK) {
	  /* Only the su may unlink directories, but the su can unlink any
	   * dir.*/
	  if( (rip->i_mode & I_TYPE) == I_DIRECTORY) r = EPERM;

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
 *                             fs_rdlink                                     *
 *===========================================================================*/
int fs_rdlink()
{
  struct buf *bp = NULL;       /* buffer containing link text */
  char* link_text;             /* either bp->b_data or rip->i_block */
  register struct inode *rip;  /* target inode */
  register int r;              /* return value */
  size_t copylen;

  copylen = min(fs_m_in.m_vfs_fs_rdlink.mem_size, UMAX_FILE_POS);

  /* Temporarily open the file. */
  if( (rip = get_inode(fs_dev, fs_m_in.m_vfs_fs_rdlink.inode)) == NULL)
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
  /* We can safely cast to unsigned, because copylen is guaranteed to be
     below max file size */
	copylen = min( copylen, (unsigned) rip->i_size);
	r = sys_safecopyto(VFS_PROC_NR, fs_m_in.m_vfs_fs_rdlink.grant,
	                   (vir_bytes) 0, (vir_bytes) link_text,
			   (size_t) copylen);
	put_block(bp, DIRECTORY_BLOCK);
	if (r == OK)
		fs_m_out.m_fs_vfs_rdlink.nbytes = copylen;
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
char dir_name[NAME_MAX + 1];	/* name of directory to be removed */
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
  if ((r = search_dir(rip, "", NULL, IS_EMPTY, IGN_PERM, 0)) != OK)
	return r;

  if (strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0)return(EINVAL);
  if (rip->i_num == ROOT_INODE) return(EBUSY); /* can't remove 'root' */

  /* Actually try to unlink the file; fails if parent is mode 0 etc. */
  if ((r = unlink_file(rldirp, rip, dir_name)) != OK) return r;

  /* Unlink . and .. from the dir. The super user can link and unlink any dir,
   * so don't make too many assumptions about them.
   */
  (void) unlink_file(rip, NULL, dot1);
  (void) unlink_file(rip, NULL, dot2);
  return(OK);
}


/*===========================================================================*
 *				unlink_file				     *
 *===========================================================================*/
static int unlink_file(dirp, rip, file_name)
struct inode *dirp;		/* parent directory of file */
struct inode *rip;		/* inode of file, may be NULL too. */
char file_name[NAME_MAX + 1]; /* name of file to be removed */
{
/* Unlink 'file_name'; rip must be the inode of 'file_name' or NULL. */

  ino_t numb;			/* inode number */
  int	r;

  /* If rip is not NULL, it is used to get faster access to the inode. */
  if (rip == NULL) {
	/* Search for file in directory and try to get its inode. */
	err_code = search_dir(dirp, file_name, &numb, LOOK_UP, IGN_PERM, 0);
	if (err_code == OK) rip = get_inode(dirp->i_dev, (int) numb);
	if (err_code != OK || rip == NULL) return(err_code);
  } else {
	dup_inode(rip);		/* inode will be returned with put_inode */
  }

  r = search_dir(dirp, file_name, NULL, DELETE, IGN_PERM, 0);

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
int fs_rename()
{
/* Perform the rename(name1, name2) system call. */
  struct inode *old_dirp, *old_ip;	/* ptrs to old dir, file inodes */
  struct inode *new_dirp, *new_ip;	/* ptrs to new dir, file inodes */
  struct inode *new_superdirp, *next_new_superdirp;
  int r = OK;				/* error flag; initially no error */
  int odir, ndir;			/* TRUE iff {old|new} file is dir */
  int same_pdir = 0;			/* TRUE iff parent dirs are the same */
  char old_name[NAME_MAX + 1], new_name[NAME_MAX + 1];
  ino_t numb;
  phys_bytes len;

  /* Copy the last component of the old name */
  len = fs_m_in.m_vfs_fs_rename.len_old; /* including trailing '\0' */
  if (len > NAME_MAX + 1 || len > EXT2_NAME_MAX + 1)
	return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, fs_m_in.m_vfs_fs_rename.grant_old,
		       (vir_bytes) 0, (vir_bytes) old_name, (size_t) len);
  if (r != OK) return r;
  NUL(old_name, len, sizeof(old_name));

  /* Copy the last component of the new name */
  len = fs_m_in.m_vfs_fs_rename.len_new; /* including trailing '\0' */
  if (len > NAME_MAX + 1 || len > EXT2_NAME_MAX + 1)
	return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, fs_m_in.m_vfs_fs_rename.grant_new,
                       (vir_bytes) 0, (vir_bytes) new_name, (size_t) len);
  if (r != OK) return r;
  NUL(new_name, len, sizeof(new_name));

  /* Get old dir inode */
  if( (old_dirp = get_inode(fs_dev, fs_m_in.m_vfs_fs_rename.dir_old)) == NULL)
	return(err_code);

  old_ip = advance(old_dirp, old_name, IGN_PERM);
  r = err_code;

  if (r == EENTERMOUNT || r == ELEAVEMOUNT) {
	put_inode(old_ip);
	old_ip = NULL;
	if (r == EENTERMOUNT) r = EXDEV;	/* should this fail at all? */
	else if (r == ELEAVEMOUNT) r = EINVAL;	/* rename on dot-dot */
  } else if (old_ip == NULL) {
	return(err_code);
  }

  /* Get new dir inode */
  if ((new_dirp = get_inode(fs_dev, fs_m_in.m_vfs_fs_rename.dir_new)) == NULL){
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

  new_ip = advance(new_dirp, new_name, IGN_PERM); /* not required to exist */

  /* However, if the check failed because the file does exist, don't continue.
   * Note that ELEAVEMOUNT is covered by the dot-dot check later. */
  if(err_code == EENTERMOUNT) {
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
			next_new_superdirp = advance(new_superdirp, dot2,
						     IGN_PERM);

			put_inode(new_superdirp);
			if(next_new_superdirp == new_superdirp) {
				put_inode(new_superdirp);
				break;
			}
			if(err_code == ELEAVEMOUNT) {
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

	/* The old or new name must not be . or .. */
	if(strcmp(old_name, ".") == 0 || strcmp(old_name, "..") == 0 ||
	   strcmp(new_name, ".") == 0 || strcmp(new_name, "..") == 0) {
		r = EINVAL;
	}
	/* Both parent directories must be on the same device.
	if(old_dirp->i_dev != new_dirp->i_dev) r = EXDEV; */

	/* Some tests apply only if the new path exists. */
	if(new_ip == NULL) {
		/* don't rename a file with a file system mounted on it.
		if (old_ip->i_dev != old_dirp->i_dev) r = EXDEV;*/
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
		r = search_dir(old_dirp,old_name, NULL, DELETE,IGN_PERM, 0);
						/* shouldn't go wrong. */
		if(r == OK)
			(void) search_dir(old_dirp, new_name, &numb, ENTER, IGN_PERM,
					  old_ip->i_mode & I_TYPE);
	} else {
		r = search_dir(new_dirp, new_name, &numb, ENTER, IGN_PERM,
					old_ip->i_mode & I_TYPE);
		if(r == OK) {
			(void) search_dir(old_dirp, old_name, NULL,
					  DELETE, IGN_PERM, 0);
		}
	}
  }
  /* If r is OK, the ctime and mtime of old_dirp and new_dirp have been marked
   * for update in search_dir. */

  if(r == OK && odir && !same_pdir) {
	/* Update the .. entry in the directory (still points to old_dirp).*/
	numb = new_dirp->i_num;
	(void) unlink_file(old_ip, NULL, dot2);
	if(search_dir(old_ip, dot2, &numb, ENTER, IGN_PERM, I_DIRECTORY) == OK) {
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
 *				fs_ftrunc				     *
 *===========================================================================*/
int fs_ftrunc(void)
{
  struct inode *rip;
  off_t start, end;
  int r;

  if( (rip = find_inode(fs_dev, fs_m_in.m_vfs_fs_ftrunc.inode)) == NULL)
	  return(EINVAL);

  start = fs_m_in.m_vfs_fs_ftrunc.trc_start;
  end = fs_m_in.m_vfs_fs_ftrunc.trc_end;

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
	panic("zeroblock_range: no block");
  offset = pos % rip->i_sp->s_block_size;
  if (offset + len > rip->i_sp->s_block_size)
	panic("zeroblock_range: len too long: %lld", len);
  memset(b_data(bp) + offset, 0, len);
  lmfs_markdirty(bp);
  put_block(bp, FULL_DATA_BLOCK);
}
