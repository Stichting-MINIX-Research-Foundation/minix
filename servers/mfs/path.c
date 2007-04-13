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
FORWARD _PROTOTYPE( int ltraverse, (struct inode *rip, char *path, 
                    char *suffix, int pathlen)                         );


/*===========================================================================*
 *                             lookup					     *
 *===========================================================================*/
PUBLIC int lookup()
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
  rip = parse_path(user_path, string, flags);
  
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
  
  /* If 'path' is a block special file, return dev number. */
  if ( (rip->i_mode & I_TYPE) == I_BLOCK_SPECIAL) {
        fs_m_out.RES_DEV = (dev_t) rip->i_zone[0];
  }

  /* Drop inode (path parse increased the counter) */
  put_inode(rip);

  return err_code;
}


/*===========================================================================*
 *                             parse_path				     *
 *===========================================================================*/
PUBLIC struct inode *parse_path(path, string, action)
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
        printf("FS: couldn't find starting inode req_nr: %d %s\n", req_nr,
			user_path);
        err_code = ENOENT;
printf("%s, %d\n", __FILE__, __LINE__);
        return NIL_INODE;
  }

  /* Find chroot inode according to the request message */
  if (fs_m_in.REQ_CHROOT_NR != 0) {
	  if ((chroot_dir = find_inode(fs_dev, fs_m_in.REQ_CHROOT_NR)) 
			  == NIL_INODE) {
		  printf("FS: couldn't find chroot inode\n");
		  err_code = ENOENT;
printf("%s, %d\n", __FILE__, __LINE__);
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
  symloop = fs_m_in.REQ_SYMLOOP;

  /* If dir has been removed return ENOENT. */
  /* Note: empty (start) path is checked in the VFS process */
  if (rip->i_nlinks == 0/* || *path == '\0'*/) {
	err_code = ENOENT;
printf("%s, %d\n", __FILE__, __LINE__);
	return(NIL_INODE);
  }

  /* There is only one way how the starting directory of the lookup
   * can be a mount point which is not a root directory, 
   * namely: climbing up on a mount (ELEAVEMOUNT).
   * In this case the lookup is intrested in the parent dir of the mount
   * point, but the last ".." component was processed in the 'previous'
   * FS process. Let's do that first.
   */
  if (rip->i_mount == I_MOUNT && rip->i_num != ROOT_INODE) {
	dir_ip = rip;
	rip = advance(&dir_ip, "..");
  	if (rip == NIL_INODE)
	{
printf("%s, %d\n", __FILE__, __LINE__);
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
	rip = advance(&dir_ip, string);
	
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
                       if (++symloop > SYMLOOP) {
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
                           fs_m_out.RES_SYMLOOP = symloop;
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
 *				advance					     *
 *===========================================================================*/
PUBLIC struct inode *advance(pdirp, string)
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
/*printf("FSadvance: ELEAVEMOUNT callnr: %d, cp: %d, restp: %s\n", 
	call_nr, path_processed, user_path + path_processed);*/

				  /* Climbing up mountpoint */
				  err_code = ELEAVEMOUNT;
				  /* This will be the FS process endoint */
				  fs_m_out.m_source = rip->i_dev;
				  fs_m_out.RES_OFFSET = path_processed;
                                  fs_m_out.RES_SYMLOOP = symloop;
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
  if (rip != NIL_INODE && rip->i_mount == I_MOUNT) {
/*printf("FSadvance: EENTERMOUNT callnr: %d, cp: %d, vmnti: %d, restp: %s\n", 
	call_nr, path_processed, rip->i_vmnt_ind, user_path + path_processed);*/

	  /* Mountpoint encountered, report it */
	  err_code = EENTERMOUNT;
	  fs_m_out.RES_INODE_NR = rip->i_num;
	  fs_m_out.RES_OFFSET = path_processed;
          fs_m_out.RES_SYMLOOP = symloop;
	  put_inode(rip);
	  rip = NIL_INODE;
  }
  return(rip);		/* return pointer to inode's component */
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
 *                             eat_path					     *
 *===========================================================================*/
PUBLIC struct inode *eat_path(path)
char *path;                    /* the path name to be parsed */
{
 /* Parse the path 'path' and put its inode in the inode table. If not possible,
  * return NIL_INODE as function value and an error code in 'err_code'.
  */
  
  return parse_path(path, (char *) 0, EAT_PATH);
}

/*===========================================================================*
 *                             last_dir					     *
 *===========================================================================*/
PUBLIC struct inode *last_dir(path, string)
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
  
  return parse_path(path, string, LAST_DIR);
}

