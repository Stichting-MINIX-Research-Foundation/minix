/* This file contains the procedures that look up path names in the directory
 * system and determine the inode number that goes with a given path name.
 *
 *  The entry points into this file are
 *   eat_path:	 the 'main' routine of the path-to-inode conversion mechanism
 *   last_dir:	 find the final directory on a given path
 *   advance:	 parse one component of a path name
 *   search_dir: search a directory for a string and return its inode number
 *
 */

#include "fs.h"
#include <string.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <sys/stat.h>
#include "buf.h"
#include "inode.h"
#include "super.h"

#include <minix/vfsif.h>


PUBLIC char dot1[2] = ".";	/* used for search_dir to bypass the access */
PUBLIC char dot2[3] = "..";	/* permissions for . and ..		    */

FORWARD _PROTOTYPE( char *get_name, (char *old_name, char string [NAME_MAX]) );
FORWARD _PROTOTYPE( char *get_name_s, (char *name, char string[NAME_MAX+1]) );
FORWARD _PROTOTYPE( int ltraverse, (struct inode *rip, char *path, 
                    char *suffix, int pathlen)                         );
FORWARD _PROTOTYPE( int ltraverse_s, (struct inode *rip, char *suffix)	);
FORWARD _PROTOTYPE( int advance_s1, (struct inode *dirp,
			char string[NAME_MAX], struct inode **resp)	);
FORWARD _PROTOTYPE( int parse_path_s, (ino_t dir_ino, ino_t root_ino,
					int flags, struct inode **res_inop,
					size_t *offsetp, int *symlinkp)	);


/*===========================================================================*
 *                             lookup_o					     *
 *===========================================================================*/
PUBLIC int lookup_o()
{
  char string[PATH_MAX];
  struct inode *rip;
  int s_error, flags;
  int len;

  string[0] = '\0';
  
  /* Check length. */
  len = fs_m_in.REQ_PATH_LEN;
  if(len > sizeof(user_path)) return E2BIG;	/* too big for buffer */
  if(len < 1) return EINVAL;			/* too small for \0 */

  /* Copy the pathname and set up caller's user and group id */
  err_code = sys_datacopy(FS_PROC_NR, (vir_bytes) fs_m_in.REQ_PATH, SELF, 
            (vir_bytes) user_path, (phys_bytes) len);
  if (err_code != OK) {
	printf("mfs:%s:%d: sys_datacopy failed: %d\n", __FILE__, __LINE__, err_code);
	return err_code;
  }

  /* Verify this is a null-terminated path. */
  if(user_path[len-1] != '\0') {
	printf("mfs:lookup: didn't get null-terminated string.\n");
	return EINVAL;
  }

  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  flags = fs_m_in.REQ_FLAGS;

  /* Clear RES_OFFSET for ENOENT */
  fs_m_out.RES_OFFSET= 0;

  /* Lookup inode */
  rip = parse_path_o(user_path, string, flags);
  
  /* Copy back the last name if it is required */
  if (err_code != OK || (flags & PATH_PENULTIMATE)) {
      	s_error = sys_datacopy(SELF_E, (vir_bytes) string, FS_PROC_NR, 
              (vir_bytes) fs_m_in.REQ_USER_ADDR, (phys_bytes) NAME_MAX);
      if (s_error != OK) {
	printf("mfs:%s:%d: sys_datacopy failed: %d\n",
		__FILE__, __LINE__, s_error);
	return s_error;
      }
  }

  /* Error or mount point encountered */
  if (rip == NIL_INODE)
  {
	if (err_code != EENTERMOUNT)
		fs_m_out.RES_INODE_NR = 0;		/* signal no inode */
	return err_code;
  }

  fs_m_out.RES_INODE_NR = rip->i_num;
  fs_m_out.RES_MODE = rip->i_mode;
  fs_m_out.RES_FILE_SIZE = rip->i_size;
  
  if ( (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL) {
        fs_m_out.RES_DEV = (dev_t) rip->i_zone[0];
  }

  /* Drop inode (path parse increased the counter) */
  put_inode(rip);

  return err_code;
}


/*===========================================================================*
 *                             fs_lookup_s				     *
 *===========================================================================*/
PUBLIC int fs_lookup_s()
{
  cp_grant_id_t grant;
  int r, r1, len, flags, symlinks;
  size_t offset, size;
  ino_t dir_ino, root_ino;
  struct inode *rip;

  grant= fs_m_in.REQ_L_GRANT;
  size= fs_m_in.REQ_L_PATH_SIZE;		/* Size of the buffer */
  len = fs_m_in.REQ_L_PATH_LEN;			/* including terminating nul */
  offset= fs_m_in.REQ_L_PATH_OFF;		/* offset in buffer */
  dir_ino= fs_m_in.REQ_L_DIR_INO;
  root_ino= fs_m_in.REQ_L_ROOT_INO;
  flags = fs_m_in.REQ_L_FLAGS;
  caller_uid = fs_m_in.REQ_L_UID;
  caller_gid = fs_m_in.REQ_L_GID;

  /* Check length. */
  if(len > sizeof(user_path)) return E2BIG;	/* too big for buffer */
  if(len < 1)
  {
	printf("mfs:fs_lookup_s: string too small.\n");
	return EINVAL;			/* too small */
  }

  /* Copy the pathname and set up caller's user and group id */
  r = sys_safecopyfrom(FS_PROC_NR, grant, offset, 
            (vir_bytes) user_path, (phys_bytes) len, D);
  if (r != OK) {
	printf("mfs:fs_lookup_s: sys_safecopyfrom failed: %d\n", r);
	return r;
  }

  /* Verify this is a null-terminated path. */
  if(user_path[len-1] != '\0') {
	printf("mfs:fs_lookup_s: didn't get null-terminated string.\n");
	return EINVAL;
  }

#if 0
  printf("mfs:fs_lookup_s: string '%s', ino %d, root %d\n",
	user_path, dir_ino, root_ino);
#endif

  /* Lookup inode */
  rip= NULL;
  r = parse_path_s(dir_ino, root_ino, flags, &rip, &offset, &symlinks);

  if (symlinks != 0 && (r == ELEAVEMOUNT || r == EENTERMOUNT || r == ESYMLINK))
  {
	len= strlen(user_path)+1;
	if (len > size)
		return ENAMETOOLONG;
	r1 = sys_safecopyto(FS_PROC_NR, grant, 0, 
		(vir_bytes) user_path, (phys_bytes) len, D);
	if (r1 != OK) {
		printf("mfs:fs_lookup_s: sys_safecopyto failed: %d\n", r1);
		return r1;
	}
#if 0
	printf("mfs:fs_lookup_s: copied back path '%s', offset %d\n",
		user_path, offset);
#endif
  }

  if (r == ELEAVEMOUNT || r == ESYMLINK)
  {
	/* Report offset and the error */
	fs_m_out.RES_OFFSET = offset;
	fs_m_out.RES_SYMLOOP = symlinks;
#if 0
	printf("mfs:fs_lookup_s: returning %d, offset %d\n", r, offset);
#endif
	if (rip) panic(__FILE__, "fs_lookup_s: rip should be clear",
		(unsigned)rip);
	return r;
  }

  if (r != OK && r != EENTERMOUNT)
  {
	if (rip) panic(__FILE__, "fs_lookup_s: rip should be clear",
		(unsigned)rip);
	return r;
  }

  fs_m_out.RES_INODE_NR = rip->i_num;
  fs_m_out.RES_MODE = rip->i_mode;
  fs_m_out.RES_FILE_SIZE = rip->i_size;
  fs_m_out.RES_OFFSET = offset;
  fs_m_out.RES_SYMLOOP2 = symlinks;
  fs_m_out.RES_UID = rip->i_uid;
  fs_m_out.RES_GID = rip->i_gid;
  
  /* This is only valid for block and character specials. But it doesn't
   * cause any harm to set RES_DEV always.
   */
  fs_m_out.RES_DEV = (dev_t) rip->i_zone[0];

  if (r == EENTERMOUNT)
	put_inode(rip);	/* Only return a reference to the final object */

  return r;
}


/*===========================================================================*
 *                             parse_path_o				     *
 *===========================================================================*/
PUBLIC struct inode *parse_path_o(path, string, action)
char *path;                    /* the path name to be parsed */
char string[NAME_MAX];         /* the final component is returned here */
int action;                    /* action on last part of path */
{
/* This is the actual code for last_dir and eat_path. Return the inode of
 * the last directory and the name of object within that directory, or the
 * inode of the last object (an empty name will be returned). Names are
 * returned in string. If string is null the name is discarded. The action
 * code determines how "last" is defined. If an error occurs, NIL_INODE
 * will be returned with an error code in err_code.
 */

  struct inode *rip, *dir_ip;
  struct inode *ver_rip;
  char *new_name;
  char lstring[NAME_MAX];
  
  /* Find starting inode inode according to the request message */
  if ((rip = find_inode(fs_dev, fs_m_in.REQ_INODE_NR)) == NIL_INODE) {
        printf("mfs:parse_path: couldn't find starting inode %d for %s\n",
		fs_m_in.REQ_INODE_NR, user_path);
        err_code = ENOENT;
        return NIL_INODE;
  }

  /* Find chroot inode according to the request message */
  if (fs_m_in.REQ_CHROOT_NR != 0) {
	  if ((chroot_dir = find_inode(fs_dev, fs_m_in.REQ_CHROOT_NR)) 
			  == NIL_INODE) {
		  printf("FS: couldn't find chroot inode\n");
		  err_code = ENOENT;
		  return NIL_INODE;
	  }
  }
  else chroot_dir = NIL_INODE;

  /* Set user and group ID */
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
      
  /* No characters were processed yet */
  path_processed = 0;  

  /* Current number of symlinks encountered */
  Xsymloop = fs_m_in.REQ_SYMLOOP;

  /* If dir has been removed return ENOENT. */
  /* Note: empty (start) path is checked in the VFS process */
  if (rip->i_nlinks == 0/* || *path == '\0'*/) {
	err_code = ENOENT;
	return(NIL_INODE);
  }

  /* There is only one way how the starting directory of the lookup
   * can be a mount point which is not a root directory, 
   * namely: climbing up on a mount (ELEAVEMOUNT).
   * In this case the lookup is intrested in the parent dir of the mount
   * point, but the last ".." component was processed in the 'previous'
   * FS process. Let's do that first.
   */
  if (rip->i_mountpoint && rip->i_num != ROOT_INODE) {
	dir_ip = rip;
	rip = advance_o(&dir_ip, "..");
  	if (rip == NIL_INODE)
	{
		return NIL_INODE;
	}
	put_inode(rip);  	/* advance() increased the counter */
  }

  dup_inode(rip);		/* inode will be returned with put_inode */
  
  /* Looking for the starting directory? 
   * Note: this happens after EENTERMOUNT or ELEAVEMOUNT 
   * without more path component */
  if (*path == '\0') {
	  return rip;
  }

  if (string == (char *) 0) string = lstring;

  /* Scan the path component by component. */
  while (TRUE) {
	int slashes = 0;
	/* Extract one component. Skip slashes first. */
	while (path[slashes] == '/') {
	  slashes++;
	  path_processed++;
  	}
	fs_m_out.RES_OFFSET = path_processed;	/* For ENOENT */
	if ( (new_name = get_name(path+slashes, string)) == (char*) 0) {
		put_inode(rip);	/* bad path in user space */
		return(NIL_INODE);
	}
	if (*new_name == '\0' && (action & PATH_PENULTIMATE)) {
		if ( (rip->i_mode & I_TYPE) == I_DIRECTORY) {
			return(rip);	/* normal exit */
		} else {
			/* last file of path prefix is not a directory */
			put_inode(rip);
			err_code = ENOTDIR;			
			return(NIL_INODE);
		}
        }

	/* There is more path.  Keep parsing. */
	dir_ip = rip;
	rip = advance_o(&dir_ip, string);
	
	/* Mount point encountered? */
	if (rip == NIL_INODE && (err_code == EENTERMOUNT || 
				err_code == ELEAVEMOUNT)) {
		put_inode(dir_ip);
		return NIL_INODE;
	}

	if (rip == NIL_INODE) {
		if (*new_name == '\0' && (action & PATH_NONSYMBOLIC) != 0)
		{
			return(dir_ip);
		}
		else if (err_code == ENOENT)
		{
			return(dir_ip);
		}
		else {
			put_inode(dir_ip);
			return(NIL_INODE);
		}
	}

       /* The call to advance() succeeded.  Fetch next component. */
       if (S_ISLNK(rip->i_mode)) {
                if (*new_name != '\0' || (action & PATH_OPAQUE) == 0) {
                    
                       if (*new_name != '\0') new_name--;

                       /* Extract path name from the symlink file */
                       if (ltraverse(rip, user_path, new_name,
			sizeof(user_path)) != OK) {
                           put_inode(dir_ip);
                           err_code = ENOENT;
                           return NIL_INODE;
                       }

                       /* Symloop limit reached? */
                       if (++Xsymloop > SYMLOOP_MAX) {
                           put_inode(dir_ip);
                           err_code = ELOOP;
                           return NIL_INODE;
                       }

                       /* Start over counting */
                       path_processed = 0;
                       
                       /* Check whether new path is relative or absolute */
                       if (user_path[0] == '/') {
                           /* Go back to VFS */
                           put_inode(dir_ip);
                           err_code = ESYMLINK;
                           fs_m_out.RES_OFFSET = path_processed;
                           fs_m_out.RES_SYMLOOP = Xsymloop;
                           return NIL_INODE;
                       }
                       /* Path is relative */
                       else {
                           rip = dir_ip;
                           path = user_path;
                           continue;
                       }
               }
       } 
       else if (*new_name != '\0') {
               put_inode(dir_ip);
               path = new_name;
               continue;
	}
      
       /* Either last name reached or symbolic link is opaque */
       if ((action & PATH_NONSYMBOLIC) != 0) {
               put_inode(rip);
               return(dir_ip);
       } else {
               put_inode(dir_ip);
               return(rip);
       }
  }
}


/*===========================================================================*
 *                             parse_path_s				     *
 *===========================================================================*/
PRIVATE int parse_path_s(dir_ino, root_ino, flags, res_inop, offsetp, symlinkp)
ino_t dir_ino;
ino_t root_ino;
int flags;
struct inode **res_inop;
size_t *offsetp;
int *symlinkp;
{
  /* Parse the path in user_path, starting at dir_ino. If the path is the empty
   * string, just return dir_ino. It is upto the caller to treat an empty
   * path in a special way. Otherwise, if the path consists of just one or
   * more slash ('/') characters, the path is replaced with ".". Otherwise,
   * just look up the first (or only) component in path after skipping any
   * leading slashes. 
   */
  int r;
  struct inode *rip, *dir_ip;
  char *cp, *ncp;
  char string[NAME_MAX+1];
#if 0
  struct inode *ver_rip;
  char *new_name;
#endif
  
  /* Find starting inode inode according to the request message */
  if ((rip = find_inode(fs_dev, dir_ino)) == NIL_INODE) {
        printf("mfs:parse_path_s: couldn't find starting inode\n");
        return ENOENT;
  }
  dup_inode(rip);

  /* No characters were processed yet */
  cp= user_path;  

  /* No symlinks encountered yet */
  *symlinkp = 0;

  /* If dir has been removed return ENOENT. */
  if (rip->i_nlinks == 0) {
	put_inode(rip);
	return ENOENT;
  }

  /* Scan the path component by component. */
  while (TRUE) {
	if (cp[0] == '\0')
	{
		/* Empty path */
		*res_inop= rip;
		*offsetp += cp-user_path;

		/* Return EENTERMOUNT if we are at a mount point */
		if (rip->i_mountpoint)
		{
			return EENTERMOUNT;
		}
		return OK;
	}

	if (cp[0] == '/')
	{
		/* Special case code. If the remaining path consists of just
		 * slashes, we need to look up '.'
		 */
		while(cp[0] == '/')
			cp++;
		if (cp[0] == '\0')
		{
			strcpy(string, ".");
			ncp= cp;
		}
		else
			ncp= get_name_s(cp, string);
	}
	else
	{
		/* Just get the first component */
		ncp= get_name_s(cp, string);
	}

	/* Special code for '..'. A process is not allowed to leave a chrooted
	 * environment. A lookup of '..' at the root of a mounted filesystem
	 * has to return ELEAVEMOUNT.
	 */
	if (strcmp(string, "..") == 0)
	{
		if (rip->i_num == root_ino)
		{
			cp= ncp;
			continue;	/* Just ignore the '..' at a process'
					 * root.
					 */
		}
		if (rip->i_num == ROOT_INODE && !rip->i_sp->s_is_root) {
			/* Climbing up mountpoint */
			put_inode(rip);
			*res_inop= NULL;
			*offsetp += cp-user_path;
			return ELEAVEMOUNT;
		}
	}
	else
	{
		/* Only check for a mount point if we are not looking for '..'.
		 */
		if (rip->i_mountpoint)
		{
			*res_inop= rip;
			*offsetp += cp-user_path;
			return EENTERMOUNT;
		}
	}

	/* There is more path.  Keep parsing. */
	dir_ip = rip;
	r = advance_s1(dir_ip, string, &rip);

	if (r != OK)
	{
		put_inode(dir_ip);
		return r;
	}
	
	/* The call to advance() succeeded.  Fetch next component. */
	if (S_ISLNK(rip->i_mode)) {

		if (ncp[0] == '\0' && (flags & PATH_RET_SYMLINK))
		{
			put_inode(dir_ip);
			*res_inop= rip;
			*offsetp += ncp-user_path;

			return OK;
		}

		/* Extract path name from the symlink file */
		r= ltraverse_s(rip, ncp);
		ncp= user_path;

		/* Symloop limit reached? */
		if (++(*symlinkp) > SYMLOOP_MAX)
			r= ELOOP;

		/* Extract path name from the symlink file */
		if (r != OK)
		{
			put_inode(dir_ip);
			put_inode(rip);
			return r;
		}

		if (ncp[0] == '/')
		{
                        put_inode(dir_ip);
                        put_inode(rip);
			*res_inop= NULL;
			*offsetp= 0;
                        return ESYMLINK;
		}
	
		put_inode(rip);
		dup_inode(dir_ip);
		rip= dir_ip;

	} 

	put_inode(dir_ip);
	cp= ncp;
  }
}

/*===========================================================================*
 *                             ltraverse				     *
 *===========================================================================*/
PRIVATE int ltraverse(rip, path, suffix, pathlen)
register struct inode *rip;    /* symbolic link */
char *path;                    /* path containing link */
char *suffix;                  /* suffix following link within path */
int pathlen;
{
/* Traverse a symbolic link. Copy the link text from the inode and insert
 * the text into the path. Return error code or report success. Base 
 * directory has to be determined according to the first character of the
 * new pathname.
 */
  
  block_t b;                   /* block containing link text */
  struct buf *bp;              /* buffer containing link text */
  size_t sl;                   /* length of link */
  size_t tl;                   /* length of suffix */
  char *sp;                    /* start of link text */
  int r = OK;

  bp  = NIL_BUF;

  if ((b = read_map(rip, (off_t) 0)) != NO_BLOCK) {
       bp = get_block(rip->i_dev, b, NORMAL);
       sl = rip->i_size;
       sp = bp->b_data;

       /* Insert symbolic text into path name. */
       tl = strlen(suffix);
       if (sl > 0 && sl + tl <= PATH_MAX-1) {
	   if(sl+tl >= pathlen)
		panic(__FILE__,"path too small for symlink", sl+tl);
           memmove(path+sl, suffix, tl);
           memmove(path, sp, sl);
           path[sl+tl] = 0;
           
           /* Copy back to VFS layer   THIS SHOULD BE IN parse_path.
	    * sys_datacopy() error, if any, gets returned as r later.
	    */
           r = sys_datacopy(SELF_E, (vir_bytes) path, FS_PROC_NR, 
                   (vir_bytes) vfs_slink_storage, (phys_bytes) sl+tl+1);
           /*
           dup_inode(bip = path[0] == '/' ? chroot_dir : ldip);
           */
	   if(r != OK) {
		printf("mfs:%s:%d: sys_datacopy failed: %d\n",
			__FILE__, __LINE__, r);
	   }
       } else panic(__FILE__,"didn't copy symlink", sl+tl);
  }
  else {
       r = ENOENT;
  }
  
  put_block(bp, DIRECTORY_BLOCK);
  put_inode(rip);
  return r;
}


/*===========================================================================*
 *                             ltraverse_s				     *
 *===========================================================================*/
PRIVATE int ltraverse_s(rip, suffix)
register struct inode *rip;	/* symbolic link */
char *suffix;			/* current remaining path. Has to point in the
				 * user_path buffer
				 */
{
/* Traverse a symbolic link. Copy the link text from the inode and insert
 * the text into the path. Return error code or report success. Base 
 * directory has to be determined according to the first character of the
 * new pathname.
 */
  
  block_t b;                   /* block containing link text */
  size_t sl;                   /* length of link */
  size_t tl;                   /* length of suffix */
  struct buf *bp;              /* buffer containing link text */
  char *sp;                    /* start of link text */
#if 0
  int r = OK;
#endif

  bp  = NIL_BUF;

  if ((b = read_map(rip, (off_t) 0)) == NO_BLOCK)
	return EIO;

  bp = get_block(rip->i_dev, b, NORMAL);
  sl = rip->i_size;
  sp = bp->b_data;

  tl = strlen(suffix);
  if (tl > 0)
  {
	/* For simplicity we require that suffix starts with a slash */
	if (suffix[0] != '/')
	{
		panic(__FILE__,
			"ltraverse_s: suffix does not start with a slash",
			NO_NUM);
	}

	/* Move suffix to the right place */
	if (sl + tl + 1 > sizeof(user_path))
		return ENAMETOOLONG;
	if (suffix-user_path != sl)
		memmove(&user_path[sl], suffix, tl+1);
  }
  else
  {
	/* Set terminating nul */
	user_path[sl]= '\0';
  }
  memmove(user_path, sp, sl);

#if 0
  printf("mfs:ltraverse_s: new path '%s'\n", user_path);
#endif
   
  put_block(bp, DIRECTORY_BLOCK);
  return OK;
}


/*===========================================================================*
 *				advance_nocheck				     *
 *===========================================================================*/
PUBLIC struct inode *advance_nocheck(pdirp, string)
struct inode **pdirp;		/* inode for directory to be searched */
char string[NAME_MAX];		/* component name to look for */
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.  If it can't be done, return NIL_INODE.
 */

  register struct inode *rip, *dirp;
  register struct super_block *sp;
  int r, inumb;
  dev_t mnt_dev;
  ino_t numb;

  dirp = *pdirp;

  /* If 'string' is empty, yield same inode straight away. */
  if (string[0] == '\0') { return(get_inode(dirp->i_dev, (int) dirp->i_num)); }

  /* Check for NIL_INODE. */
  if (dirp == NIL_INODE) { return(NIL_INODE); }

  /* If 'string' is not present in the directory, signal error. */
  if ( (r = search_dir_nocheck(dirp, string, &numb, LOOK_UP)) != OK) {
	err_code = r;
	return(NIL_INODE);
  }

  /* Don't go beyond the current root directory, unless the string is dot2. 
   * Note: it has to be checked only if this FS process owns the chroot
   * directory of the process */
  if (chroot_dir != NIL_INODE) {
	  if (dirp == chroot_dir && strcmp(string, "..") == 0 && string != dot2)
		  return(get_inode(dirp->i_dev, (int) dirp->i_num));
  }

  /* The component has been found in the directory.  Get inode. */
  if ( (rip = get_inode(dirp->i_dev, (int) numb)) == NIL_INODE)  {
	return(NIL_INODE);
  }

  /* The following test is for "mountpoint/.." where mountpoint is a
   * mountpoint. ".." will refer to the root of the mounted filesystem,
   * but has to become a reference to the parent of the 'mountpoint'
   * directory.
   *
   * This case is recognized by the looked up name pointing to a
   * root inode, and the directory in which it is held being a
   * root inode, _and_ the name[1] being '.'. (This is a test for '..'
   * and excludes '.'.)
   */
  if (rip->i_num == ROOT_INODE) {
	  if (dirp->i_num == ROOT_INODE) {
		  if (string[1] == '.') {
			  sp = rip->i_sp;
			  if (!sp->s_is_root) {
/*printf("FSadvance: ELEAVEMOUNT callnr: %d, cp: %d, restp: %s\n", 
	call_nr, Xpath_processed, user_path + Xpath_processed);*/

				  /* Climbing up mountpoint */
				  err_code = ELEAVEMOUNT;
				  /* This will be the FS process endoint */
				  fs_m_out.m_source = rip->i_dev;
				  fs_m_out.RES_OFFSET = path_processed;
                                  fs_m_out.RES_SYMLOOP = Xsymloop;
				  put_inode(rip);
				  /*put_inode(dirp);*/
				  rip = NIL_INODE;
			  }
		  }
	  }
  }
  if (rip == NIL_INODE) return(NIL_INODE);

  /* See if the inode is mounted on.  If so, switch to root directory of the
   * mounted file system.  The super_block provides the linkage between the
   * inode mounted on and the root directory of the mounted file system.
   */
  if (rip != NIL_INODE && rip->i_mountpoint) {

	  /* Mountpoint encountered, report it */
	  err_code = EENTERMOUNT;
	  fs_m_out.RES_INODE_NR = rip->i_num;
	  fs_m_out.RES_OFFSET = path_processed;
          fs_m_out.RES_SYMLOOP = Xsymloop;
	  put_inode(rip);
	  rip = NIL_INODE;
  }
  return(rip);		/* return pointer to inode's component */
}


/*===========================================================================*
 *				advance_o				     *
 *===========================================================================*/
PUBLIC struct inode *advance_o(pdirp, string)
struct inode **pdirp;		/* inode for directory to be searched */
char string[NAME_MAX];		/* component name to look for */
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.  If it can't be done, return NIL_INODE.
 */

  register struct inode *rip, *dirp;
  register struct super_block *sp;
  int r, inumb;
  dev_t mnt_dev;
  ino_t numb;

  dirp = *pdirp;

  /* If 'string' is empty, yield same inode straight away. */
  if (string[0] == '\0') { return(get_inode(dirp->i_dev, (int) dirp->i_num)); }

  /* Check for NIL_INODE. */
  if (dirp == NIL_INODE) { return(NIL_INODE); }

  /* If 'string' is not present in the directory, signal error. */
  if ( (r = search_dir(dirp, string, &numb, LOOK_UP)) != OK) {
	err_code = r;
	return(NIL_INODE);
  }

  /* Don't go beyond the current root directory, unless the string is dot2. 
   * Note: it has to be checked only if this FS process owns the chroot
   * directory of the process */
  if (chroot_dir != NIL_INODE) {
	  if (dirp == chroot_dir && strcmp(string, "..") == 0 && string != dot2)
		  return(get_inode(dirp->i_dev, (int) dirp->i_num));
  }

  /* The component has been found in the directory.  Get inode. */
  if ( (rip = get_inode(dirp->i_dev, (int) numb)) == NIL_INODE)  {
	return(NIL_INODE);
  }

  /* The following test is for "mountpoint/.." where mountpoint is a
   * mountpoint. ".." will refer to the root of the mounted filesystem,
   * but has to become a reference to the parent of the 'mountpoint'
   * directory.
   *
   * This case is recognized by the looked up name pointing to a
   * root inode, and the directory in which it is held being a
   * root inode, _and_ the name[1] being '.'. (This is a test for '..'
   * and excludes '.'.)
   */
  if (rip->i_num == ROOT_INODE) {
	  if (dirp->i_num == ROOT_INODE) {
		  if (string[1] == '.') {
			  sp = rip->i_sp;
			  if (!sp->s_is_root) {

				  /* Climbing up mountpoint */
				  err_code = ELEAVEMOUNT;
				  /* This will be the FS process endoint */
				  fs_m_out.m_source = rip->i_dev;
				  fs_m_out.RES_OFFSET = path_processed;
                                  fs_m_out.RES_SYMLOOP = Xsymloop;
				  put_inode(rip);
				  /*put_inode(dirp);*/
				  rip = NIL_INODE;
			  }
		  }
	  }
  }
  if (rip == NIL_INODE) return(NIL_INODE);

  /* See if the inode is mounted on.  If so, switch to root directory of the
   * mounted file system.  The super_block provides the linkage between the
   * inode mounted on and the root directory of the mounted file system.
   */
  if (rip != NIL_INODE && rip->i_mountpoint) {

	  /* Mountpoint encountered, report it */
	  err_code = EENTERMOUNT;
	  fs_m_out.RES_INODE_NR = rip->i_num;
	  fs_m_out.RES_OFFSET = path_processed;
          fs_m_out.RES_SYMLOOP = Xsymloop;
	  put_inode(rip);
	  rip = NIL_INODE;
  }
  return(rip);		/* return pointer to inode's component */
}


/*===========================================================================*
 *				advance_s1				     *
 *===========================================================================*/
PRIVATE int advance_s1(dirp, string, resp)
struct inode *dirp;		/* inode for directory to be searched */
char string[NAME_MAX];		/* component name to look for */
struct inode **resp;		/* resulting inode */
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.
 */
  int r;
  ino_t numb;
  struct inode *rip;

  /* If 'string' is empty, return an error. */
  if (string[0] == '\0') return ENOENT;

  /* Check for NIL_INODE. */
  if (dirp == NIL_INODE) panic(__FILE__, "advance_s: nil dirp", NO_NUM);

  /* If 'string' is not present in the directory, signal error. */
  if ( (r = search_dir(dirp, string, &numb, LOOK_UP)) != OK) {
	return(r);
  }

  /* The component has been found in the directory.  Get inode. */
  if ( (rip = get_inode(dirp->i_dev, (int) numb)) == NIL_INODE)  {
	return(err_code);
  }

  *resp= rip;
  return OK;
}


/*===========================================================================*
 *				get_name				     *
 *===========================================================================*/
PRIVATE char *get_name(old_name, string)
char *old_name;			/* path name to parse */
char string[NAME_MAX];		/* component extracted from 'old_name' */
{
/* Given a pointer to a path name in fs space, 'old_name', copy the next
 * component to 'string' and pad with zeros.  A pointer to that part of
 * the name as yet unparsed is returned.  Roughly speaking,
 * 'get_name' = 'old_name' - 'string'.
 *
 * This routine follows the standard convention that /usr/ast, /usr//ast,
 * //usr///ast and /usr/ast/ are all equivalent.
 */

  register int c;
  register char *np, *rnp;

  np = string;			/* 'np' points to current position */
  rnp = old_name;		/* 'rnp' points to unparsed string */

  c = *rnp;
  /* Copy the unparsed path, 'old_name', to the array, 'string'. */
  while ( rnp < &old_name[PATH_MAX]  &&  c != '/'   &&  c != '\0') {
	  if (np < &string[NAME_MAX]) *np++ = c;
	  c = *++rnp;		/* advance to next character */
	  path_processed++; 	/* count characters */
  }

  /* To make /usr/ast/ equivalent to /usr/ast, skip trailing slashes. */
  while (c == '/' && rnp < &old_name[PATH_MAX]) {
	  c = *++rnp;
	  path_processed++; 	/* count characters */
  }

  if (np < &string[NAME_MAX]) *np = '\0';	/* Terminate string */

  if (rnp >= &old_name[PATH_MAX]) {
	  err_code = ENAMETOOLONG;
	  return((char *) 0);
  }
  return(rnp);
}


/*===========================================================================*
 *				get_name_s				     *
 *===========================================================================*/
PRIVATE char *get_name_s(path_name, string)
char *path_name;		/* path name to parse */
char string[NAME_MAX+1];	/* component extracted from 'old_name' */
{
/* Given a pointer to a path name in fs space, 'path_name', copy the first
 * component to 'string' (truncated if necessary, always nul terminated).
 * A pointer to the string after the first component of the name as yet
 * unparsed is returned.  Roughly speaking,
 * 'get_name_s' = 'path_name' - 'string'.
 *
 * This routine follows the standard convention that /usr/ast, /usr//ast,
 * //usr///ast and /usr/ast/ are all equivalent.
 */
  size_t len;
  char *cp, *ep;

  cp= path_name;

  /* Skip leading slashes */
  while (cp[0] == '/')
	cp++;

  /* Find the end of the first component */
  ep= cp;
  while(ep[0] != '\0' && ep[0] != '/')
	ep++;

  len= ep-cp;

  /* Truncate the amount to be copied if it exceeds NAME_MAX */
  if (len > NAME_MAX)
	len= NAME_MAX;

  /* Special case of the string at cp is empty */
  if (len == 0)
  {
	/* Return "." */
	strcpy(string, ".");
  }
  else
  {
	memcpy(string, cp, len);
	string[len]= '\0';
  }

  return ep;
}

/*===========================================================================*
 *				search_dir				     *
 *===========================================================================*/
PUBLIC int search_dir(ldir_ptr, string, numb, flag)
register struct inode *ldir_ptr; /* ptr to inode for dir to search */
char string[NAME_MAX];		 /* component to search for */
ino_t *numb;			 /* pointer to inode number */
int flag;			 /* LOOK_UP, ENTER, DELETE or IS_EMPTY */
{
/* This function searches the directory whose inode is pointed to by 'ldip':
 * if (flag == ENTER)  enter 'string' in the directory with inode # '*numb';
 * if (flag == DELETE) delete 'string' from the directory;
 * if (flag == LOOK_UP) search for 'string' and return inode # in 'numb';
 * if (flag == IS_EMPTY) return OK if only . and .. in dir else ENOTEMPTY;
 *
 *    if 'string' is dot1 or dot2, no access permissions are checked.
 */

  register struct direct *dp = NULL;
  register struct buf *bp = NULL;
  int i, r, e_hit, t, match;
  mode_t bits;
  off_t pos;
  unsigned new_slots, old_slots;
  block_t b;
  struct super_block *sp;
  int extended = 0;

  /* If 'ldir_ptr' is not a pointer to a dir inode, error. */
  if ( (ldir_ptr->i_mode & I_TYPE) != I_DIRECTORY)  {
	return(ENOTDIR);
   }
  
  r = OK;

  if (flag != IS_EMPTY) {
	bits = (flag == LOOK_UP ? X_BIT : W_BIT | X_BIT);

	if (string == dot1 || string == dot2) {
		if (flag != LOOK_UP) r = read_only(ldir_ptr);
				     /* only a writable device is required. */
        }
	else r = forbidden(ldir_ptr, bits); /* check access permissions */
  }
  if (r != OK) return(r);
  
  /* Step through the directory one block at a time. */
  old_slots = (unsigned) (ldir_ptr->i_size/DIR_ENTRY_SIZE);
  new_slots = 0;
  e_hit = FALSE;
  match = 0;			/* set when a string match occurs */

  for (pos = 0; pos < ldir_ptr->i_size; pos += ldir_ptr->i_sp->s_block_size) {
	b = read_map(ldir_ptr, pos);	/* get block number */

	/* Since directories don't have holes, 'b' cannot be NO_BLOCK. */
	bp = get_block(ldir_ptr->i_dev, b, NORMAL);	/* get a dir block */

	if (bp == NO_BLOCK)
		panic(__FILE__,"get_block returned NO_BLOCK", NO_NUM);

	/* Search a directory block. */
	for (dp = &bp->b_dir[0];
		dp < &bp->b_dir[NR_DIR_ENTRIES(ldir_ptr->i_sp->s_block_size)];
		dp++) {
		if (++new_slots > old_slots) { /* not found, but room left */
			if (flag == ENTER) e_hit = TRUE;
			break;
		}

		/* Match occurs if string found. */
		if (flag != ENTER && dp->d_ino != 0) {
			if (flag == IS_EMPTY) {
				/* If this test succeeds, dir is not empty. */
				if (strcmp(dp->d_name, "." ) != 0 &&
				    strcmp(dp->d_name, "..") != 0) match = 1;
			} else {
				if (strncmp(dp->d_name, string, NAME_MAX) == 0){
					match = 1;
				}
			}
		}

		if (match) {
			/* LOOK_UP or DELETE found what it wanted. */
			r = OK;
			if (flag == IS_EMPTY) r = ENOTEMPTY;
			else if (flag == DELETE) {
				/* Save d_ino for recovery. */
				t = NAME_MAX - sizeof(ino_t);
				*((ino_t *) &dp->d_name[t]) = dp->d_ino;
				dp->d_ino = 0;	/* erase entry */
				bp->b_dirt = DIRTY;
				ldir_ptr->i_update |= CTIME | MTIME;
				ldir_ptr->i_dirt = DIRTY;
			} else {
				sp = ldir_ptr->i_sp;	/* 'flag' is LOOK_UP */
				*numb = conv4(sp->s_native, (int) dp->d_ino);
			}
			put_block(bp, DIRECTORY_BLOCK);
			return(r);
		}

		/* Check for free slot for the benefit of ENTER. */
		if (flag == ENTER && dp->d_ino == 0) {
			e_hit = TRUE;	/* we found a free slot */
			break;
		}
	}

	/* The whole block has been searched or ENTER has a free slot. */
	if (e_hit) break;	/* e_hit set if ENTER can be performed now */
	put_block(bp, DIRECTORY_BLOCK);	/* otherwise, continue searching dir */
  }

  /* The whole directory has now been searched. */
  if (flag != ENTER) {
  	return(flag == IS_EMPTY ? OK : ENOENT);
  }

  /* This call is for ENTER.  If no free slot has been found so far, try to
   * extend directory.
   */
  if (e_hit == FALSE) { /* directory is full and no room left in last block */
	new_slots++;		/* increase directory size by 1 entry */
	if (new_slots == 0) return(EFBIG); /* dir size limited by slot count */
	if ( (bp = new_block(ldir_ptr, ldir_ptr->i_size)) == NIL_BUF)
		return(err_code);
	dp = &bp->b_dir[0];
	extended = 1;
  }

  /* 'bp' now points to a directory block with space. 'dp' points to slot. */
  (void) memset(dp->d_name, 0, (size_t) NAME_MAX); /* clear entry */
  for (i = 0; i < NAME_MAX && string[i]; i++) dp->d_name[i] = string[i];
  sp = ldir_ptr->i_sp; 
  dp->d_ino = conv4(sp->s_native, (int) *numb);
  bp->b_dirt = DIRTY;
  put_block(bp, DIRECTORY_BLOCK);
  ldir_ptr->i_update |= CTIME | MTIME;	/* mark mtime for update later */
  ldir_ptr->i_dirt = DIRTY;
  if (new_slots > old_slots) {
	ldir_ptr->i_size = (off_t) new_slots * DIR_ENTRY_SIZE;
	/* Send the change to disk if the directory is extended. */
	if (extended) rw_inode(ldir_ptr, WRITING);
  }
  return(OK);
}


/*===========================================================================*
 *				search_dir_nocheck			     *
 *===========================================================================*/
PUBLIC int search_dir_nocheck(ldir_ptr, string, numb, flag)
register struct inode *ldir_ptr; /* ptr to inode for dir to search */
char string[NAME_MAX];		 /* component to search for */
ino_t *numb;			 /* pointer to inode number */
int flag;			 /* LOOK_UP, ENTER, DELETE or IS_EMPTY */
{
/* This function searches the directory whose inode is pointed to by 'ldip':
 * if (flag == ENTER)  enter 'string' in the directory with inode # '*numb';
 * if (flag == DELETE) delete 'string' from the directory;
 * if (flag == LOOK_UP) search for 'string' and return inode # in 'numb';
 * if (flag == IS_EMPTY) return OK if only . and .. in dir else ENOTEMPTY;
 *
 *    if 'string' is dot1 or dot2, no access permissions are checked.
 */

  register struct direct *dp = NULL;
  register struct buf *bp = NULL;
  int i, r, e_hit, t, match;
  mode_t bits;
  off_t pos;
  unsigned new_slots, old_slots;
  block_t b;
  struct super_block *sp;
  int extended = 0;

  /* If 'ldir_ptr' is not a pointer to a dir inode, error. */
  if ( (ldir_ptr->i_mode & I_TYPE) != I_DIRECTORY)  {
	return(ENOTDIR);
   }
  
  r = OK;

  if (flag != IS_EMPTY) {
	bits = (flag == LOOK_UP ? X_BIT : W_BIT | X_BIT);

	if (string == dot1 || string == dot2) {
		if (flag != LOOK_UP) r = read_only(ldir_ptr);
				     /* only a writable device is required. */
        }
  }
  if (r != OK) return(r);
  
  /* Step through the directory one block at a time. */
  old_slots = (unsigned) (ldir_ptr->i_size/DIR_ENTRY_SIZE);
  new_slots = 0;
  e_hit = FALSE;
  match = 0;			/* set when a string match occurs */

  for (pos = 0; pos < ldir_ptr->i_size; pos += ldir_ptr->i_sp->s_block_size) {
	b = read_map(ldir_ptr, pos);	/* get block number */

	/* Since directories don't have holes, 'b' cannot be NO_BLOCK. */
	bp = get_block(ldir_ptr->i_dev, b, NORMAL);	/* get a dir block */

	if (bp == NO_BLOCK)
		panic(__FILE__,"get_block returned NO_BLOCK", NO_NUM);

	/* Search a directory block. */
	for (dp = &bp->b_dir[0];
		dp < &bp->b_dir[NR_DIR_ENTRIES(ldir_ptr->i_sp->s_block_size)];
		dp++) {
		if (++new_slots > old_slots) { /* not found, but room left */
			if (flag == ENTER) e_hit = TRUE;
			break;
		}

		/* Match occurs if string found. */
		if (flag != ENTER && dp->d_ino != 0) {
			if (flag == IS_EMPTY) {
				/* If this test succeeds, dir is not empty. */
				if (strcmp(dp->d_name, "." ) != 0 &&
				    strcmp(dp->d_name, "..") != 0) match = 1;
			} else {
				if (strncmp(dp->d_name, string, NAME_MAX) == 0){
					match = 1;
				}
			}
		}

		if (match) {
			/* LOOK_UP or DELETE found what it wanted. */
			r = OK;
			if (flag == IS_EMPTY) r = ENOTEMPTY;
			else if (flag == DELETE) {
				/* Save d_ino for recovery. */
				t = NAME_MAX - sizeof(ino_t);
				*((ino_t *) &dp->d_name[t]) = dp->d_ino;
				dp->d_ino = 0;	/* erase entry */
				bp->b_dirt = DIRTY;
				ldir_ptr->i_update |= CTIME | MTIME;
				ldir_ptr->i_dirt = DIRTY;
			} else {
				sp = ldir_ptr->i_sp;	/* 'flag' is LOOK_UP */
				*numb = conv4(sp->s_native, (int) dp->d_ino);
			}
			put_block(bp, DIRECTORY_BLOCK);
			return(r);
		}

		/* Check for free slot for the benefit of ENTER. */
		if (flag == ENTER && dp->d_ino == 0) {
			e_hit = TRUE;	/* we found a free slot */
			break;
		}
	}

	/* The whole block has been searched or ENTER has a free slot. */
	if (e_hit) break;	/* e_hit set if ENTER can be performed now */
	put_block(bp, DIRECTORY_BLOCK);	/* otherwise, continue searching dir */
  }

  /* The whole directory has now been searched. */
  if (flag != ENTER) {
  	return(flag == IS_EMPTY ? OK : ENOENT);
  }

  /* This call is for ENTER.  If no free slot has been found so far, try to
   * extend directory.
   */
  if (e_hit == FALSE) { /* directory is full and no room left in last block */
	new_slots++;		/* increase directory size by 1 entry */
	if (new_slots == 0) return(EFBIG); /* dir size limited by slot count */
	if ( (bp = new_block(ldir_ptr, ldir_ptr->i_size)) == NIL_BUF)
		return(err_code);
	dp = &bp->b_dir[0];
	extended = 1;
  }

  /* 'bp' now points to a directory block with space. 'dp' points to slot. */
  (void) memset(dp->d_name, 0, (size_t) NAME_MAX); /* clear entry */
  for (i = 0; i < NAME_MAX && string[i]; i++) dp->d_name[i] = string[i];
  sp = ldir_ptr->i_sp; 
  dp->d_ino = conv4(sp->s_native, (int) *numb);
  bp->b_dirt = DIRTY;
  put_block(bp, DIRECTORY_BLOCK);
  ldir_ptr->i_update |= CTIME | MTIME;	/* mark mtime for update later */
  ldir_ptr->i_dirt = DIRTY;
  if (new_slots > old_slots) {
	ldir_ptr->i_size = (off_t) new_slots * DIR_ENTRY_SIZE;
	/* Send the change to disk if the directory is extended. */
	if (extended) rw_inode(ldir_ptr, WRITING);
  }
  return(OK);
}


/*===========================================================================*
 *                             eat_path_o				     *
 *===========================================================================*/
PUBLIC struct inode *eat_path_o(path)
char *path;                    /* the path name to be parsed */
{
 /* Parse the path 'path' and put its inode in the inode table. If not possible,
  * return NIL_INODE as function value and an error code in 'err_code'.
  */
  
  return parse_path_o(path, (char *) 0, EAT_PATH);
}

/*===========================================================================*
 *                             last_dir_o				     *
 *===========================================================================*/
PUBLIC struct inode *last_dir_o(path, string)
char *path;                    /* the path name to be parsed */
char string[NAME_MAX];         /* the final component is returned here */
{
/* Given a path, 'path', located in the fs address space, parse it as
 * far as the last directory, fetch the inode for the last directory into
 * the inode table, and return a pointer to the inode.  In
 * addition, return the final component of the path in 'string'.
 * If the last directory can't be opened, return NIL_INODE and
 * the reason for failure in 'err_code'.
 */
  
  return parse_path_o(path, string, LAST_DIR);
}

