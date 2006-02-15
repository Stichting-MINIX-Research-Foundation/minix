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
#include <sys/stat.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "super.h"

PUBLIC char dot1[2] = ".";	/* used for search_dir to bypass the access */
PUBLIC char dot2[3] = "..";	/* permissions for . and ..		    */

FORWARD _PROTOTYPE( char *get_name, (char *old_name, char string [NAME_MAX]) );

FORWARD _PROTOTYPE( struct inode *ltraverse, (struct inode *rip,
                       char *path, char *suffix, struct inode *ldip)   );

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
  char *new_name;
  int symloop;
  char lstring[NAME_MAX];

  /* Is the path absolute or relative?  Initialize 'rip' accordingly. */
  rip = (*path == '/' ? fp->fp_rootdir : fp->fp_workdir);

  /* If dir has been removed or path is empty, return ENOENT. */
  if (rip->i_nlinks == 0 || *path == '\0') {
	err_code = ENOENT;
	return(NIL_INODE);
  }

  dup_inode(rip);		/* inode will be returned with put_inode */

  symloop = 0;                 /* symbolic link traversal count */
  if (string == (char *) 0) string = lstring;

  /* Scan the path component by component. */
  while (TRUE) {
	/* Extract one component. */
	if ( (new_name = get_name(path, string)) == (char*) 0) {
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

       if (rip == NIL_INODE) {
               if (*new_name == '\0' && (action & PATH_NONSYMBOLIC) != 0)
                       return(dir_ip);
               else {
                       put_inode(dir_ip);
                       return(NIL_INODE);
               }
       }

       /* The call to advance() succeeded.  Fetch next component. */
       if (S_ISLNK(rip->i_mode)) {
               if (*new_name != '\0' || (action & PATH_OPAQUE) == 0) {
                       if (*new_name != '\0') new_name--;
                       rip = ltraverse(rip, path, new_name, dir_ip);
                       put_inode(dir_ip);
                       if (++symloop > SYMLOOP) {
                               err_code = ELOOP;
                               put_inode(rip);
                               rip = NIL_INODE;
                       }
                       if (rip == NIL_INODE) return(NIL_INODE);
                       continue;
               }
       } else if (*new_name != '\0') {
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

/*===========================================================================*
 *                             ltraverse				     *
 *===========================================================================*/
PRIVATE struct inode *ltraverse(rip, path, suffix, ldip)
register struct inode *rip;    /* symbolic link */
char *path;                    /* path containing link */
char *suffix;                  /* suffix following link within path */
register struct inode *ldip;   /* directory containing link */
{
/* Traverse a symbolic link. Copy the link text from the inode and insert
 * the text into the path. Return the inode of base directory and the
 * ammended path. The symbolic link inode is always freed. The inode
 * returned is already duplicated. NIL_INODE is returned on error.
 */
  
  block_t b;                   /* block containing link text */
  struct inode *bip;           /* inode of base directory */
  struct buf *bp;              /* buffer containing link text */
  size_t sl;                   /* length of link */
  size_t tl;                   /* length of suffix */
  char *sp;                    /* start of link text */

  bip = NIL_INODE;
  bp  = NIL_BUF;

  if ((b = read_map(rip, (off_t) 0)) != NO_BLOCK) {
       bp = get_block(rip->i_dev, b, NORMAL);
       sl = rip->i_size;
       sp = bp->b_data;

       /* Insert symbolic text into path name. */
       tl = strlen(suffix);
       if (sl > 0 && sl + tl <= PATH_MAX-1) {
               memmove(path+sl, suffix, tl);
               memmove(path, sp, sl);
               path[sl+tl] = 0;
               dup_inode(bip = path[0] == '/' ? fp->fp_rootdir : ldip);
       }
  }
  
  put_block(bp, DIRECTORY_BLOCK);
  put_inode(rip);
  if (bip == NIL_INODE)
  {
       err_code = ENOENT;
  }
  return (bip);
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
  while ( (c = *rnp) == '/') rnp++;	/* skip leading slashes */

  /* Copy the unparsed path, 'old_name', to the array, 'string'. */
  while ( rnp < &old_name[PATH_MAX]  &&  c != '/'   &&  c != '\0') {
	if (np < &string[NAME_MAX]) *np++ = c;
	c = *++rnp;		/* advance to next character */
  }

  /* To make /usr/ast/ equivalent to /usr/ast, skip trailing slashes. */
  while (c == '/' && rnp < &old_name[PATH_MAX]) c = *++rnp;

  if (np < &string[NAME_MAX]) *np = '\0';	/* Terminate string */

  if (rnp >= &old_name[PATH_MAX]) {
	err_code = ENAMETOOLONG;
	return((char *) 0);
  }
  return(rnp);
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

  /* Don't go beyond the current root directory, unless the string is dot2. */
  if (dirp == fp->fp_rootdir && strcmp(string, "..") == 0 && string != dot2)
		return(get_inode(dirp->i_dev, (int) dirp->i_num));

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
  if (rip->i_num == ROOT_INODE)
	if (dirp->i_num == ROOT_INODE) {
	    if (string[1] == '.') {
		sp= rip->i_sp;
		if (sp->s_imount != sp->s_isup)
		{
			/* Release the root inode.  Replace by the
			 * inode mounted on. Update parent.
			 */
			put_inode(rip);
			put_inode(dirp);
			mnt_dev = sp->s_imount->i_dev;
			inumb = (int) sp->s_imount->i_num;
			dirp = *pdirp = get_inode(mnt_dev, inumb);
			rip = advance(pdirp, string);
		}
	    }
	}
  if (rip == NIL_INODE) return(NIL_INODE);

  /* See if the inode is mounted on.  If so, switch to root directory of the
   * mounted file system.  The super_block provides the linkage between the
   * inode mounted on and the root directory of the mounted file system.
   */
  while (rip != NIL_INODE && rip->i_mount == I_MOUNT) {
	/* The inode is indeed mounted on. */
	for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++) {
		if (sp->s_imount == rip) {
			/* Release the inode mounted on.  Replace by the
			 * inode of the root inode of the mounted device.
			 */
			put_inode(rip);
			rip = get_inode(sp->s_dev, ROOT_INODE);
			break;
		}
	}
  }
  return(rip);		/* return pointer to inode's component */
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
				if (strncmp(dp->d_name, string, NAME_MAX) == 0) {
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
  for (i = 0; string[i] && i < NAME_MAX; i++) dp->d_name[i] = string[i];
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
