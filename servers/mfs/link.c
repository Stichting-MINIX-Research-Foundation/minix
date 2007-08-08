

#include "fs.h"
#include <sys/stat.h>
#include <string.h>
#include <minix/com.h>
#include <minix/callnr.h>

#include "buf.h"
#include "inode.h"
#include "super.h"

#include <minix/vfsif.h>

#define SAME 1000

FORWARD _PROTOTYPE( int remove_dir_o, (struct inode *rldirp, struct inode *rip,
			char dir_name[NAME_MAX])			);
FORWARD _PROTOTYPE( int remove_dir_nocheck, (struct inode *rldirp,
	struct inode *rip, char dir_name[NAME_MAX])			);
FORWARD _PROTOTYPE( int unlink_file_o, (struct inode *dirp, struct inode *rip,
			char file_name[NAME_MAX])			);
FORWARD _PROTOTYPE( int unlink_file_nocheck, (struct inode *dirp,
	struct inode *rip, char file_name[NAME_MAX])			);
FORWARD _PROTOTYPE( off_t nextblock, (off_t pos, int zonesize)		);
FORWARD _PROTOTYPE( void zeroblock_half, (struct inode *i, off_t p, int l));
FORWARD _PROTOTYPE( void zeroblock_range, (struct inode *i, off_t p, off_t h));

/* Args to zeroblock_half() */
#define FIRST_HALF	0
#define LAST_HALF	1


/*===========================================================================*
 *				fs_link_o				     *
 *===========================================================================*/
PUBLIC int fs_link_o()
{
/* Perform the link(name1, name2) system call. */

  struct inode *ip, *rip;
  register int r;
  char string[NAME_MAX];
  struct inode *new_ip;
  phys_bytes len;

  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  len = MFS_MIN(fs_m_in.REQ_PATH_LEN, sizeof(string));
  /* Copy the link name's last component */
  r = sys_datacopy(FS_PROC_NR, (vir_bytes) fs_m_in.REQ_PATH,
          SELF, (vir_bytes) string, (phys_bytes) len);
  if (r != OK) return r;
  MFS_NUL(string, len, sizeof(string));
  
  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_LINKED_FILE)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_link() failed\n", SELF_E);
        return(EINVAL);
  }
  
  /* Check to see if the file has maximum number of links already. */
  r = OK;
  if (rip->i_nlinks >= (rip->i_sp->s_version == V1 ? CHAR_MAX : SHRT_MAX))
	r = EMLINK;

  /* Only super_user may link to directories. */
  if (r == OK)
	if ( (rip->i_mode & I_TYPE) == I_DIRECTORY && caller_uid != SU_UID) 
		r = EPERM;

  /* If error with 'name', return the inode. */
  if (r != OK) {
	put_inode(rip);
	return(r);
  }

  /* Temporarily open the last dir */
  if ( (ip = get_inode(fs_dev, fs_m_in.REQ_LINK_PARENT)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_link() failed\n", SELF_E);
        return(EINVAL);
  }

  /* If 'name2' exists in full (even if no space) set 'r' to error. */
  if (r == OK) {
	if ( (new_ip = advance_o(&ip, string)) == NIL_INODE) {
		r = err_code;
		if (r == ENOENT) r = OK;
	} else {
		put_inode(new_ip);
		r = EEXIST;
	}
  }

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
 *				fs_link_s				     *
 *===========================================================================*/
PUBLIC int fs_link_s()
{
/* Perform the link(name1, name2) system call. */

  struct inode *ip, *rip;
  register int r;
  char string[NAME_MAX];
  struct inode *new_ip;
  phys_bytes len;

#if 0
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
#endif
  
  len = MFS_MIN(fs_m_in.REQ_PATH_LEN, sizeof(string));
  /* Copy the link name's last component */
  r = sys_safecopyfrom(FS_PROC_NR, fs_m_in.REQ_GRANT, 0, 
          (vir_bytes) string, (phys_bytes) len, D);
  if (r != OK) return r;
  MFS_NUL(string, len, sizeof(string));
  
  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_LINKED_FILE)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_link() failed\n", SELF_E);
        return(EINVAL);
  }
  
  /* Check to see if the file has maximum number of links already. */
  r = OK;
  if (rip->i_nlinks >= (rip->i_sp->s_version == V1 ? CHAR_MAX : SHRT_MAX))
	r = EMLINK;

  /* Only super_user may link to directories. */
  if (r == OK)
	if ( (rip->i_mode & I_TYPE) == I_DIRECTORY && caller_uid != SU_UID) 
		r = EPERM;

  /* If error with 'name', return the inode. */
  if (r != OK) {
	put_inode(rip);
	return(r);
  }

  /* Temporarily open the last dir */
  if ( (ip = get_inode(fs_dev, fs_m_in.REQ_LINK_PARENT)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_link() failed\n", SELF_E);
        return(EINVAL);
  }

  /* If 'name2' exists in full (even if no space) set 'r' to error. */
  if (r == OK) {
	if ( (new_ip = advance_nocheck(&ip, string)) == NIL_INODE) {
		r = err_code;
		if (r == ENOENT) r = OK;
	} else {
		put_inode(new_ip);
		r = EEXIST;
	}
  }

  /* Try to link. */
  if (r == OK)
	r = search_dir_nocheck(ip, string, &rip->i_num, ENTER);

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
 *				fs_unlink_o				     *
 *===========================================================================*/
PUBLIC int fs_unlink_o()
{
/* Perform the unlink(name) or rmdir(name) system call. The code for these two
 * is almost the same.  They differ only in some condition testing.  Unlink()
 * may be used by the superuser to do dangerous things; rmdir() may not.
 */
  register struct inode *rip;
  struct inode *rldirp;
  int r;
  char string[NAME_MAX];
  phys_bytes len;
  
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Copy the last component */
  len = MFS_MIN(fs_m_in.REQ_PATH_LEN, sizeof(string));
  r = sys_datacopy(FS_PROC_NR, (vir_bytes) fs_m_in.REQ_PATH,
          SELF, (vir_bytes) string, (phys_bytes) len);
  if (r != OK) return r;
  MFS_NUL(string, len, sizeof(string));
  
  /* Temporarily open the dir. */
  if ( (rldirp = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
        return(EINVAL);
  }
  
  /* The last directory exists.  Does the file also exist? */
  r = OK;
  if ( (rip = advance_o(&rldirp, string)) == NIL_INODE) r = err_code;

  /* If error, return inode. */
  if (r != OK) {
	printf("fs_unlink_o: advance_o failed: %d\n", r);
        /* Mount point? */
        if (r == EENTERMOUNT || r == ELEAVEMOUNT)
            r = EBUSY;
	put_inode(rldirp);
	return(r);
  }

  /* Now test if the call is allowed, separately for unlink() and rmdir(). */
  if (fs_m_in.m_type == REQ_UNLINK_O) {
	/* Only the su may unlink directories, but the su can unlink any dir.*/
	if ( (rip->i_mode & I_TYPE) == I_DIRECTORY 
                && caller_uid != SU_UID) r = EPERM;

	/* Don't unlink a file if it is the root of a mounted file system. */
	if (rip->i_num == ROOT_INODE) r = EBUSY;

	/* Actually try to unlink the file; fails if parent is mode 0 etc. */
	if (r == OK) r = unlink_file_o(rldirp, rip, string);

  } 
  else {
	r = remove_dir_o(rldirp, rip, string); /* call is RMDIR */
  }

  /* If unlink was possible, it has been done, otherwise it has not. */
  put_inode(rip);
  put_inode(rldirp);
  if (r != OK) printf("fs_unlink_o: returning %d\n", r);
  return(r);
}


/*===========================================================================*
 *				fs_unlink_s				     *
 *===========================================================================*/
PUBLIC int fs_unlink_s()
{
/* Perform the unlink(name) or rmdir(name) system call. The code for these two
 * is almost the same.  They differ only in some condition testing.  Unlink()
 * may be used by the superuser to do dangerous things; rmdir() may not.
 */
  register struct inode *rip;
  struct inode *rldirp;
  int r;
  char string[NAME_MAX];
  phys_bytes len;
  
  /* Copy the last component */
  len = MFS_MIN(fs_m_in.REQ_PATH_LEN, sizeof(string));
  r = sys_safecopyfrom(FS_PROC_NR, fs_m_in.REQ_GRANT, 0, 
          (vir_bytes) string, (phys_bytes) len, D);
  if (r != OK) return r;
  MFS_NUL(string, len, sizeof(string));
  
  /* Temporarily open the dir. */
  if ( (rldirp = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
        return(EINVAL);
  }
  
  /* The last directory exists.  Does the file also exist? */
  r = OK;
  if ( (rip = advance_nocheck(&rldirp, string)) == NIL_INODE) r = err_code;

  /* If error, return inode. */
  if (r != OK) {
        /* Mount point? */
        if (r == EENTERMOUNT || r == ELEAVEMOUNT)
            r = EBUSY;
	put_inode(rldirp);
	return(r);
  }

  /* Now test if the call is allowed, separately for unlink() and rmdir(). */
  if (fs_m_in.m_type == REQ_UNLINK_S) {
	/* Only the su may unlink directories, but the su can unlink any dir.*/
	if ( (rip->i_mode & I_TYPE) == I_DIRECTORY) r = EPERM;

	/* Actually try to unlink the file; fails if parent is mode 0 etc. */
	if (r == OK) r = unlink_file_nocheck(rldirp, rip, string);
  } 
  else {
	r = remove_dir_nocheck(rldirp, rip, string); /* call is RMDIR */
  }

  /* If unlink was possible, it has been done, otherwise it has not. */
  put_inode(rip);
  put_inode(rldirp);
  return(r);
}



/*===========================================================================*
 *                             fs_rdlink_o                                   *
 *===========================================================================*/
PUBLIC int fs_rdlink_o()
{
  block_t b;                   /* block containing link text */
  struct buf *bp;              /* buffer containing link text */
  register struct inode *rip;  /* target inode */
  register int r;              /* return value */
  int copylen;
  
  copylen = fs_m_in.REQ_SLENGTH;
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;

  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
        return(EINVAL);
  }

  r = EACCES;
  if (S_ISLNK(rip->i_mode) && (b = read_map(rip, (off_t) 0)) != NO_BLOCK) {
       if (copylen <= 0) r = EINVAL;
       else if (copylen < rip->i_size) r = ERANGE;
       else {
	       if(rip->i_size < copylen) copylen = rip->i_size;
               bp = get_block(rip->i_dev, b, NORMAL);
               r = sys_vircopy(SELF, D, (vir_bytes) bp->b_data,
		fs_m_in.REQ_WHO_E, D, (vir_bytes) fs_m_in.REQ_USER_ADDR, 
                (vir_bytes) copylen);

               if (r == OK) r = copylen;
               put_block(bp, DIRECTORY_BLOCK);
       }
  }

  put_inode(rip);
  return(r);
}


/*===========================================================================*
 *                             fs_rdlink_s                                   *
 *===========================================================================*/
PUBLIC int fs_rdlink_s()
{
  block_t b;                   /* block containing link text */
  struct buf *bp;              /* buffer containing link text */
  register struct inode *rip;  /* target inode */
  register int r;              /* return value */
  int copylen;
  
  copylen = fs_m_in.REQ_SLENGTH;
  if (copylen <= 0) return(EINVAL);

  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
        return(EINVAL);
  }


  if (!S_ISLNK(rip->i_mode))
	r = EACCES;
  else if (copylen < rip->i_size) 
	r = ERANGE;
  else if ((b = read_map(rip, (off_t) 0)) == NO_BLOCK)
	r = EIO;
  else {
	/* Passed all checks */
	copylen = rip->i_size;
	bp = get_block(rip->i_dev, b, NORMAL);
	r = sys_safecopyto(FS_PROC_NR, fs_m_in.REQ_GRANT, 0, 
	(vir_bytes) bp->b_data, (vir_bytes) copylen, D);

	put_block(bp, DIRECTORY_BLOCK);
	if (r == OK) r = copylen;
  }

  put_inode(rip);
  return(r);
}


/*===========================================================================*
 *				remove_dir_o				     *
 *===========================================================================*/
PRIVATE int remove_dir_o(rldirp, rip, dir_name)
struct inode *rldirp;		 	/* parent directory */
struct inode *rip;			/* directory to be removed */
char dir_name[NAME_MAX];		/* name of directory to be removed */
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
  if ((r = search_dir(rip, "", (ino_t *) 0, IS_EMPTY)) != OK) return r;

  if (strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0)return(EINVAL);
  if (rip->i_num == ROOT_INODE) return(EBUSY); /* can't remove 'root' */
 
  /* Actually try to unlink the file; fails if parent is mode 0 etc. */
  if ((r = unlink_file_o(rldirp, rip, dir_name)) != OK) return r;

  /* Unlink . and .. from the dir. The super user can link and unlink any dir,
   * so don't make too many assumptions about them.
   */
  (void) unlink_file_o(rip, NIL_INODE, dot1);
  (void) unlink_file_o(rip, NIL_INODE, dot2);
  return(OK);
}


/*===========================================================================*
 *				remove_dir_nocheck			     *
 *===========================================================================*/
PRIVATE int remove_dir_nocheck(rldirp, rip, dir_name)
struct inode *rldirp;		 	/* parent directory */
struct inode *rip;			/* directory to be removed */
char dir_name[NAME_MAX];		/* name of directory to be removed */
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
  if ((r = search_dir_nocheck(rip, "", (ino_t *) 0, IS_EMPTY)) != OK) return r;

  if (strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0)return(EINVAL);
  if (rip->i_num == ROOT_INODE) return(EBUSY); /* can't remove 'root' */
 
  /* Actually try to unlink the file; fails if parent is mode 0 etc. */
  if ((r = unlink_file_nocheck(rldirp, rip, dir_name)) != OK) return r;

  /* Unlink . and .. from the dir. The super user can link and unlink any dir,
   * so don't make too many assumptions about them.
   */
  (void) unlink_file_nocheck(rip, NIL_INODE, dot1);
  (void) unlink_file_nocheck(rip, NIL_INODE, dot2);
  return(OK);
}


/*===========================================================================*
 *				unlink_file_o				     *
 *===========================================================================*/
PRIVATE int unlink_file_o(dirp, rip, file_name)
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


/*===========================================================================*
 *				unlink_file_nocheck			     *
 *===========================================================================*/
PRIVATE int unlink_file_nocheck(dirp, rip, file_name)
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
	err_code = search_dir_nocheck(dirp, file_name, &numb, LOOK_UP);
	if (err_code == OK) rip = get_inode(dirp->i_dev, (int) numb);
	if (err_code != OK || rip == NIL_INODE) return(err_code);
  } else {
	dup_inode(rip);		/* inode will be returned with put_inode */
  }

  r = search_dir_nocheck(dirp, file_name, (ino_t *) 0, DELETE);

  if (r == OK) {
	rip->i_nlinks--;	/* entry deleted from parent's dir */
	rip->i_update |= CTIME;
	rip->i_dirt = DIRTY;
  }

  put_inode(rip);
  return(r);
}


/*===========================================================================*
 *				fs_rename_o				     *
 *===========================================================================*/
PUBLIC int fs_rename_o()
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
  phys_bytes len;
  int r1;
  
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Copy the last component of the old name */
  len = MFS_MIN(fs_m_in.REQ_PATH_LEN, sizeof(old_name));
  r = sys_datacopy(FS_PROC_NR, (vir_bytes) fs_m_in.REQ_PATH,
          SELF, (vir_bytes) old_name, (phys_bytes) len);
  if (r != OK) return r;
  MFS_NUL(old_name, len, sizeof(old_name));
  
  /* Copy the last component of the new name */
  len = MFS_MIN(fs_m_in.REQ_SLENGTH, sizeof(new_name));
  r = sys_datacopy(FS_PROC_NR, (vir_bytes) fs_m_in.REQ_USER_ADDR,
          SELF, (vir_bytes) new_name, (phys_bytes) len);
  if (r != OK) return r;
  MFS_NUL(new_name, len, sizeof(new_name));

  /* Get old dir inode */ 
  if ( (old_dirp = get_inode(fs_dev, fs_m_in.REQ_OLD_DIR)) == NIL_INODE) 
        return(err_code);

  if ( (old_ip = advance_o(&old_dirp, old_name)) == NIL_INODE) r = err_code;

  /* Get new dir inode */ 
  if ( (new_dirp = get_inode(fs_dev, fs_m_in.REQ_NEW_DIR)) == NIL_INODE) 
      r = err_code;
  new_ip = advance_o(&new_dirp, new_name);	/* not required to exist */

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
				put_inode(new_superdirp);
				r = EINVAL;
				break;
			}
			next_new_superdirp = advance_o(&new_superdirp, dot2);
			put_inode(new_superdirp);
			/*
			if (next_new_superdirp == new_superdirp) {
				put_inode(new_superdirp);
				break;	
			}
			*/
			if (err_code == ELEAVEMOUNT) {
				/* imitate that we are back at the root,
				 * cross device checked already on VFS */
				/*next_new_superdirp = new_superdirp;*/
				err_code = OK;
				break;
			}
			new_superdirp = next_new_superdirp;
			if (new_superdirp == NIL_INODE) {
				/* Missing ".." entry.  Assume the worst. */
				r = EINVAL;
				break;
			}
		} 	
		/*put_inode(new_superdirp);*/
	}	

	/* The old or new name must not be . or .. */
	if (strcmp(old_name, ".")==0 || strcmp(old_name, "..")==0 ||
	    strcmp(new_name, ".")==0 || strcmp(new_name, "..")==0) {
		r = EINVAL;
	}
	/* Both parent directories must be on the same device. 
	if (old_dirp->i_dev != new_dirp->i_dev) r = EXDEV; */

	/* Parent dirs must be writable, searchable and on a writable device */
	if ((r1 = forbidden(old_dirp, W_BIT | X_BIT)) != OK ||
	    (r1 = forbidden(new_dirp, W_BIT | X_BIT)) != OK) {
		r = r1;
	}

	/* Some tests apply only if the new path exists. */
	if (new_ip == NIL_INODE) {
		/* don't rename a file with a file system mounted on it. 
		if (old_ip->i_dev != old_dirp->i_dev) r = EXDEV;*/
		if (odir && new_dirp->i_nlinks >=
		    (new_dirp->i_sp->s_version == V1 ? CHAR_MAX : SHRT_MAX) &&
		    !same_pdir && r == OK) { 
			r = EMLINK;
		}
	} 
	else {
		if (old_ip == new_ip) {
			r = SAME; /* old=new */
		}
		
		/* has the old file or new file a file system mounted on it? 
		if (old_ip->i_dev != new_ip->i_dev) r = EXDEV;
		*/

		ndir = ((new_ip->i_mode & I_TYPE) == I_DIRECTORY); /* dir ? */
		if (odir == TRUE && ndir == FALSE) {
			r = ENOTDIR;
		}
		if (odir == FALSE && ndir == TRUE) {
			r = EISDIR;
		}
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
			r = remove_dir_o(new_dirp, new_ip, new_name);
		else 
			r = unlink_file_o(new_dirp, new_ip, new_name);
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
	(void) unlink_file_o(old_ip, NIL_INODE, dot2);
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
 *				fs_rename_s				     *
 *===========================================================================*/
PUBLIC int fs_rename_s()
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
  phys_bytes len;
  int r1;
  
  /* Copy the last component of the old name */
  len = MFS_MIN(fs_m_in.REQ_REN_LEN_OLD, sizeof(old_name));
  r = sys_safecopyfrom(FS_PROC_NR, fs_m_in.REQ_REN_GRANT_OLD, 0, 
          (vir_bytes) old_name, (phys_bytes) len, D);
  if (r != OK) return r;
  MFS_NUL(old_name, len, sizeof(old_name));
  
  /* Copy the last component of the new name */
  len = MFS_MIN(fs_m_in.REQ_REN_LEN_NEW, sizeof(new_name));
  r = sys_safecopyfrom(FS_PROC_NR, fs_m_in.REQ_REN_GRANT_NEW, 0, 
          (vir_bytes) new_name, (phys_bytes) len, D);
  if (r != OK) return r;
  MFS_NUL(new_name, len, sizeof(new_name));

  /* Get old dir inode */ 
  if ( (old_dirp = get_inode(fs_dev, fs_m_in.REQ_REN_OLD_DIR)) == NIL_INODE) 
        return(err_code);

  if ( (old_ip = advance_nocheck(&old_dirp, old_name)) == NIL_INODE)
	r = err_code;

  /* Get new dir inode */ 
  if ( (new_dirp = get_inode(fs_dev, fs_m_in.REQ_REN_NEW_DIR)) == NIL_INODE) 
      r = err_code;
  new_ip = advance_nocheck(&new_dirp, new_name); /* not required to exist */

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
				put_inode(new_superdirp);
				r = EINVAL;
				break;
			}
printf("fs_rename_s: new_superdirp: %d on 0x%x\n",
	new_superdirp->i_num, new_superdirp->i_dev);

			next_new_superdirp = advance_nocheck(&new_superdirp,
				dot2);

printf("fs_rename_s: next_new_superdirp: %d on 0x%x\n",
	next_new_superdirp->i_num, next_new_superdirp->i_dev);

			put_inode(new_superdirp);
			if (next_new_superdirp == new_superdirp) {
				put_inode(new_superdirp);
				break;	
			}
			if (err_code == ELEAVEMOUNT) {
				/* imitate that we are back at the root,
				 * cross device checked already on VFS */
				/*next_new_superdirp = new_superdirp;*/
				err_code = OK;
				break;
			}
			new_superdirp = next_new_superdirp;
			if (new_superdirp == NIL_INODE) {
				/* Missing ".." entry.  Assume the worst. */
				r = EINVAL;
				break;
			}
		} 	
	}	

	/* The old or new name must not be . or .. */
	if (strcmp(old_name, ".")==0 || strcmp(old_name, "..")==0 ||
	    strcmp(new_name, ".")==0 || strcmp(new_name, "..")==0) {
		r = EINVAL;
	}
	/* Both parent directories must be on the same device. 
	if (old_dirp->i_dev != new_dirp->i_dev) r = EXDEV; */

	/* Some tests apply only if the new path exists. */
	if (new_ip == NIL_INODE) {
		/* don't rename a file with a file system mounted on it. 
		if (old_ip->i_dev != old_dirp->i_dev) r = EXDEV;*/
		if (odir && new_dirp->i_nlinks >=
		    (new_dirp->i_sp->s_version == V1 ? CHAR_MAX : SHRT_MAX) &&
		    !same_pdir && r == OK) { 
			r = EMLINK;
		}
	} 
	else {
		if (old_ip == new_ip) {
			r = SAME; /* old=new */
		}
		
		/* has the old file or new file a file system mounted on it? 
		if (old_ip->i_dev != new_ip->i_dev) r = EXDEV;
		*/

		ndir = ((new_ip->i_mode & I_TYPE) == I_DIRECTORY); /* dir ? */
		if (odir == TRUE && ndir == FALSE) {
			r = ENOTDIR;
		}
		if (odir == FALSE && ndir == TRUE) {
			r = EISDIR;
		}
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
			r = remove_dir_nocheck(new_dirp, new_ip, new_name);
		else 
			r = unlink_file_nocheck(new_dirp, new_ip, new_name);
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
		r = search_dir_nocheck(old_dirp, old_name, (ino_t *) 0, DELETE);
						/* shouldn't go wrong. */
		if (r==OK) (void) search_dir_nocheck(old_dirp, new_name,
			&numb, ENTER);
	} else {
		r = search_dir_nocheck(new_dirp, new_name, &numb, ENTER);
		if (r == OK)
		    (void) search_dir_nocheck(old_dirp, old_name,
			(ino_t *) 0, DELETE);
	}
  }
  /* If r is OK, the ctime and mtime of old_dirp and new_dirp have been marked
   * for update in search_dir.
   */

  if (r == OK && odir && !same_pdir) {
	/* Update the .. entry in the directory (still points to old_dirp). */
	numb = new_dirp->i_num;
	(void) unlink_file_nocheck(old_ip, NIL_INODE, dot2);
	if (search_dir_nocheck(old_ip, dot2, &numb, ENTER) == OK) {
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
 *				fs_trunc				     *
 *===========================================================================*/
PUBLIC int fs_trunc()
{
  struct inode *rip;
  int r = OK;
  
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Temporarily open the file. */
  if ( (rip = get_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
printf("MFS(%d) get_inode by fs_chmod() failed\n", SELF_E);
        return(EINVAL);
  }
  
  if ( (rip->i_mode & I_TYPE) != I_REGULAR)
        r = EINVAL;
  else
        r = truncate_inode(rip, fs_m_in.REQ_LENGTH); 
  
  put_inode(rip);

  return r;
}

/*===========================================================================*
 *				fs_ftrunc				     *
 *===========================================================================*/
PUBLIC int fs_ftrunc(void)
{
  struct inode *rip;
  off_t start, end;
  int r;
  
  if ( (rip = find_inode(fs_dev, fs_m_in.REQ_FD_INODE_NR)) 
		  == NIL_INODE) {
          printf("FSfreesp: couldn't find inode %d\n", 
			  fs_m_in.REQ_FD_INODE_NR); 
          return EINVAL;
  }

  start = fs_m_in.REQ_FD_START;
  end = fs_m_in.REQ_FD_END;

  if (end == 0) {
      r = truncate_inode(rip, start);
  }
  else {
      r = freesp_inode(rip, start, end);
  }
  
  return r;
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
  rip->i_update |= CTIME | MTIME;
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

        rip->i_update |= CTIME | MTIME;
        rip->i_dirt = DIRTY;

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


