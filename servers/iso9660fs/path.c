#include "inc.h"
#include <string.h>
#include <minix/com.h>
#include <minix/vfsif.h>

#include "buf.h"

FORWARD _PROTOTYPE(char *get_name, (char *old_name, char string [NAME_MAX]));
FORWARD _PROTOTYPE( char *get_name_s, (char *name, char string[NAME_MAX+1]) );
FORWARD _PROTOTYPE( int parse_path_s, (ino_t dir_ino, ino_t root_ino,
				       int flags, struct dir_record **res_inop,
				       size_t *offsetp));
FORWARD _PROTOTYPE( int advance_s, (struct dir_record *dirp,
				    char string[NAME_MAX], struct dir_record **resp));

/* Lookup is a function used to ``look up" a particular path. It is called
 * very often. */
/*===========================================================================*
 *                             lookup					     *
 *===========================================================================*/
PUBLIC int lookup()
{
  char string[PATH_MAX];
  int s_error, flags;
  int len;
  struct dir_record *dir;

  string[0] = '\0';
  
  /* Check length. */
  len = fs_m_in.REQ_PATH_LEN;
  if(len > sizeof(user_path)) return E2BIG;	/* too big for buffer */
  if(len < 1) return EINVAL;			/* too small for \0 */

  /* Copy the pathname and set up caller's user and group id */
  err_code = sys_datacopy(FS_PROC_NR, (vir_bytes) fs_m_in.REQ_PATH, SELF, 
            (vir_bytes) user_path, (phys_bytes) len);
  if (err_code != OK) {
    printf("i9660fs:%s:%d: sys_datacopy failed: %d\n", __FILE__, __LINE__, err_code);
    return err_code;
  }

  /* Verify this is a null-terminated path. */
  if(user_path[len-1] != '\0') {
    printf("i9660fs:lookup: didn't get null-terminated string.\n");
    return EINVAL;
  }
  
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  flags = fs_m_in.REQ_FLAGS;
  
  /* Clear RES_OFFSET for ENOENT */
  fs_m_out.RES_OFFSET= 0;

  /* Lookup inode */
  dir = parse_path(user_path, string, flags);

  /* Copy back the last name if it is required */
  if (err_code != OK || (flags & PATH_PENULTIMATE)) {
    s_error = sys_datacopy(SELF_E, (vir_bytes) string, FS_PROC_NR,
			   (vir_bytes) fs_m_in.REQ_USER_ADDR, (phys_bytes) NAME_MAX);
    if (s_error != OK) {
      printf("i9660fs:%s:%d: sys_datacopy failed: %d\n",
	     __FILE__, __LINE__, s_error);
      return s_error;
    }
  }

  /* Error or mount point encountered */
  if (dir == NULL) {
    if (err_code != EENTERMOUNT)
      fs_m_out.RES_INODE_NR = 0;		/* signal no inode */
    return err_code;
  }

  fs_m_out.RES_INODE_NR = ID_DIR_RECORD(dir);
  fs_m_out.RES_MODE = dir->d_mode;
  fs_m_out.RES_FILE_SIZE = dir->d_file_size; 
  
  /* Drop inode (path parse increased the counter) */
  release_dir_record(dir);

  return err_code;
}

/*===========================================================================*
 *                             fs_lookup_s				     *
 *===========================================================================*/
PUBLIC int fs_lookup_s() {
  cp_grant_id_t grant;
  int r, r1, len, flags;
  size_t offset, size;
  ino_t dir_ino, root_ino;
  struct dir_record *dir;

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
  if(len < 1)return EINVAL;			/* too small */

  /* Copy the pathname and set up caller's user and group id */
  r = sys_safecopyfrom(FS_PROC_NR, grant, offset, 
            (vir_bytes) user_path, (phys_bytes) len, D);

  if (r != OK) {
	printf("iso9660fs:fs_lookup_s: sys_safecopyfrom failed: %d\n", r);
	return r;
  }

  /* Verify this is a null-terminated path. */
  if(user_path[len-1] != '\0') {
	printf("iso9660fs:fs_lookup_s: didn't get null-terminated string.\n");
	return EINVAL;
  }

  /* Lookup inode */
  dir = NULL;
  r = parse_path_s(dir_ino, root_ino, flags, &dir, &offset);

  if (r == ELEAVEMOUNT) {
    /* Report offset and the error */
    fs_m_out.RES_OFFSET = offset;
    fs_m_out.RES_SYMLOOP = 0;
    if (dir) panic(__FILE__, "fs_lookup_s: dir should be clear",
		   (unsigned)dir);
    return r;
  }

  if (r != OK && r != EENTERMOUNT) {
    if (dir)
      panic(__FILE__, "fs_lookup_s: dir should be clear",
	    (unsigned)dir);
    return r;
  }

  fs_m_out.RES_OFFSET = offset;
  fs_m_out.RES_INODE_NR = ID_DIR_RECORD(dir);
  fs_m_out.RES_MODE = dir->d_mode;
  fs_m_out.RES_FILE_SIZE = dir->d_file_size; 
  fs_m_out.RES_SYMLOOP2 = 0;
  fs_m_out.RES_UID = 0; 	/* root */
  fs_m_out.RES_GID = 0;		/* operator */

/*   /\* Drop inode (path parse increased the counter) *\/ */
/*   release_dir_record(dir); */

  if (r == EENTERMOUNT)
    release_dir_record(dir);

  return r;
}

/* The search dir actually performs the operation of searching for the
 * compoent ``string" in ldir_ptr. It returns the response and the number of
 * the inode in numb. */
/*===========================================================================*
 *				search_dir				     *
 *===========================================================================*/
PUBLIC int search_dir(ldir_ptr,string,numb)
     register struct dir_record *ldir_ptr; /*  dir record parent */
     char string[NAME_MAX];	      /* component to search for */
     ino_t *numb;		      /* pointer to new dir record */
{
  struct dir_record *dir_tmp;
  register struct buf *bp,*bp2;
  int pos,r,len;
  char* comma_pos = NULL;
  char tmp_string[NAME_MAX];

  /* This function search a particular element (in string) in a inode and
   * return its number */

  /* Initialize the tmp array */
  memset(tmp_string,'\0',NAME_MAX);

  if ((ldir_ptr->d_mode & I_TYPE) != I_DIRECTORY) {
    return(ENOTDIR);
  }
  
  r = OK;

  if (strcmp(string,".") == 0) {
    *numb = ID_DIR_RECORD(ldir_ptr);
    return OK;
  }

  if (strcmp(string,"..") == 0 && ldir_ptr->loc_extent_l == v_pri.dir_rec_root->loc_extent_l) {
    *numb = ROOT_INO_NR;
/*     *numb = ID_DIR_RECORD(ldir_ptr); */
    return OK;
  }

  /* Read the dir's content */
  pos = ldir_ptr->ext_attr_rec_length;
  bp = get_block(ldir_ptr->loc_extent_l);

  if (bp == NIL_BUF)
    return EINVAL;

  while (pos < v_pri.logical_block_size_l) {
    if ((dir_tmp = get_free_dir_record()) == NULL) {
      put_block(bp);
      return EINVAL;
    }

    if (create_dir_record(dir_tmp,bp->b_data + pos,
			  ldir_ptr->loc_extent_l*v_pri.logical_block_size_l + pos) == EINVAL)
      return EINVAL;

    if (dir_tmp->length == 0) {
      release_dir_record(dir_tmp);
      put_block(bp);
      return EINVAL;
    }
    
    memcpy(tmp_string,dir_tmp->file_id,dir_tmp->length_file_id);
    comma_pos = strchr(tmp_string,';');
    if (comma_pos != NULL)
      *comma_pos = 0;
    else
      tmp_string[dir_tmp->length_file_id] = 0;
    if (tmp_string[strlen(tmp_string) - 1] == '.')
      tmp_string[strlen(tmp_string) - 1] = '\0';
    
    if (strcmp(tmp_string,string) == 0 ||
	(dir_tmp->file_id[0] == 1 && strcmp(string,"..") == 0)) {

      /* If the element is found or we are searchig for... */

      if (dir_tmp->loc_extent_l == dir_records->loc_extent_l) {
	/* In this case the inode is a root because the parent
	 * points to the same location than the inode. */
	*numb = 1;
 	release_dir_record(dir_tmp);
	put_block(bp);
	return OK;
      }

      if (dir_tmp->ext_attr_rec_length != 0) {
	dir_tmp->ext_attr = get_free_ext_attr();
	create_ext_attr(dir_tmp->ext_attr,bp->b_data);
      }

      *numb = ID_DIR_RECORD(dir_tmp);
      release_dir_record(dir_tmp);
      put_block(bp);
      
      return OK;
    }

    pos += dir_tmp->length;
    release_dir_record(dir_tmp);
  }
  
  put_block(bp);
  return EINVAL;
}

/* Parse path will parse a particular path and return the final dir record.
 * The final component of this path will be returned in string. It works in
 * two ways: the first is PATH_PENULTIMATE and it returns the last dir of the
 * path while the second is PATH_NON_SYMBOLIC where it returns the last
 * component of the path. */
/*===========================================================================*
 *                             parse_path				     *
 *===========================================================================*/
PUBLIC struct dir_record *parse_path(path, string, action)
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

  char *new_name;
  char lstring[NAME_MAX];
  struct dir_record *start_dir, *chroot_dir, *old_dir, *dir;

  /* Find starting inode inode according to the request message */
  if ((start_dir = get_dir_record(fs_m_in.REQ_INODE_NR)) == NULL) {
    printf("I9660FS: couldn't find starting inode req_nr: %d %s\n", req_nr,
	   user_path);
    err_code = ENOENT;
    printf("%s, %d\n", __FILE__, __LINE__);
    return NULL;
  }

  /* Set user and group ID */
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
      
  /* No characters were processed yet */
  path_processed = 0;

  /* Current number of symlinks encountered */
  symloop = fs_m_in.REQ_SYMLOOP;

  if (*path == '\0') {
    return start_dir;
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

    if ((new_name = get_name(path+slashes, string)) == (char*) 0) {
      release_dir_record(start_dir);	/* bad path in user space */
      return(NULL);
    }

    if (*new_name == '\0' && (action & PATH_PENULTIMATE)) {
      if ((start_dir->file_flags & I_TYPE) ==I_DIRECTORY) {
	return(start_dir);	/* normal exit */
      } else {
	/* last file of path prefix is not a directory */
	release_dir_record(start_dir);
	err_code = ENOTDIR;
	return(NULL);
      }
    }
 
    /* There is more path.  Keep parsing. */
    old_dir = start_dir;
    start_dir = advance(&old_dir, string);

    if (start_dir == NULL) {
      if (*new_name == '\0' && (action & PATH_NONSYMBOLIC) != 0) {
	return(old_dir);
      }
      else if (err_code == ENOENT) {
	return(old_dir);
      }
      else {
	release_dir_record(old_dir);
	return(NULL);
      }
    }
    
    if (*new_name != '\0') {
      release_dir_record(old_dir);
      path = new_name;
      continue;
    }
      
    /* Either last name reached or symbolic link is opaque */
    if ((action & PATH_NONSYMBOLIC) != 0) {
      release_dir_record(start_dir);
      return(old_dir);
    } else {
      release_dir_record(old_dir);
      return(start_dir);
    }
  }
}

/* Parse the path in user_path, starting at dir_ino. If the path is the empty
 * string, just return dir_ino. It is upto the caller to treat an empty
 * path in a special way. Otherwise, if the path consists of just one or
 * more slash ('/') characters, the path is replaced with ".". Otherwise,
 * just look up the first (or only) component in path after skipping any
 * leading slashes. 
 */
/*===========================================================================*
 *                             parse_path_s				     *
 *===========================================================================*/
PRIVATE int parse_path_s(dir_ino, root_ino, flags, res_inop, offsetp)
ino_t dir_ino;
ino_t root_ino;
int flags;
struct dir_record **res_inop;
size_t *offsetp;
{
  int r;
  char string[NAME_MAX+1];
  char *cp, *ncp;
  struct dir_record *start_dir, *old_dir;

  /* Find starting inode inode according to the request message */
  if ((start_dir = get_dir_record(dir_ino)) == NULL) {
    printf("I9660FS: couldn't find starting inode req_nr: %d %s\n", req_nr,
	   user_path);
    printf("%s, %d\n", __FILE__, __LINE__);
    return ENOENT;
  }
  
  cp = user_path;

  /* Scan the path component by component. */
  while (TRUE) {
    if (cp[0] == '\0') {
      /* Empty path */
      *res_inop= start_dir;
      *offsetp += cp-user_path;

      /* Return EENTERMOUNT if we are at a mount point */
      if (start_dir->d_mountpoint)
       	return EENTERMOUNT;

      return OK;
    }

    if (cp[0] == '/') {
      /* Special case code. If the remaining path consists of just
       * slashes, we need to look up '.'
       */
      while(cp[0] == '/')
	cp++;
      if (cp[0] == '\0') {
	strcpy(string, ".");
	ncp = cp;
      }
      else
	ncp = get_name_s(cp, string);
    } else
      /* Just get the first component */
      ncp = get_name_s(cp, string);
  
    /* Special code for '..'. A process is not allowed to leave a chrooted
     * environment. A lookup of '..' at the root of a mounted filesystem
     * has to return ELEAVEMOUNT.
     */
    if (strcmp(string, "..") == 0) {

      /* This condition is not necessary since it will never be the root filesystem */
      /*       if (start_dir == dir_records) { */
      /* 	cp = ncp; */
      /* 	continue;	/\* Just ignore the '..' at a process' */
      /* 			 * root. */
      /* 			 *\/ */
      /*       } */

      if (start_dir == dir_records) {
	/* Climbing up mountpoint */
	release_dir_record(start_dir);
	*res_inop = NULL;
	*offsetp += cp-user_path;
	return ELEAVEMOUNT;
      }
    } else {
      /* Only check for a mount point if we are not looking for '..'. */
      if (start_dir->d_mountpoint) {
	*res_inop= start_dir;
	*offsetp += cp-user_path;
	return EENTERMOUNT;
      }
    }
 
    /* There is more path.  Keep parsing. */
    old_dir = start_dir;

    r = advance_s(old_dir, string, &start_dir);

    if (r != OK) {
      release_dir_record(old_dir);
      return r;
    }

    release_dir_record(old_dir);
    cp = ncp;
  }
}

/* This function will return the componsent in ``string" looking in the dir
 * pdirp... */
/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
PUBLIC struct dir_record *advance(pdirp, string)
struct dir_record **pdirp;	/* inode for directory to be searched */
char string[NAME_MAX];		/* component name to look for */
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.  If it can't be done, return NULL.
 */

  register struct dir_record *rip, *dirp;
  int r, inumb;
  dev_t mnt_dev;
  ino_t numb;

  dirp = *pdirp;

  /* If 'string' is empty, yield same inode straight away. */
  if (string[0] == '\0') {
    return dirp;
  }

  /* Check for NULL. */
  if (dirp == NULL) {
    return(NULL);
  }

  /* If 'string' is not present in the directory, signal error. */
  if ( (r = search_dir(dirp, string, &numb)) != OK) {
    err_code = r;
    return(NULL);
  }

  /* The component has been found in the directory.  Get inode. */
  if ( (rip = get_dir_record((int) numb)) == NULL)  {
    return(NULL);
  }

  return(rip);
}

/*===========================================================================*
 *				advance_s				     *
 *===========================================================================*/
PRIVATE int advance_s(dirp, string, resp)
struct dir_record *dirp;		/* inode for directory to be searched */
char string[NAME_MAX];		        /* component name to look for */
struct dir_record **resp;		/* resulting inode */
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.
 */

  register struct dir_record *rip = NULL;
  int r, inumb;
  dev_t mnt_dev;
  ino_t numb;

  /* If 'string' is empty, yield same inode straight away. */
  if (string[0] == '\0') {
    return ENOENT;
  }

  /* Check for NULL. */
  if (dirp == NULL) {
    return EINVAL;
  }

  /* If 'string' is not present in the directory, signal error. */
  if ( (r = search_dir(dirp, string, &numb)) != OK) {
    return r;
  }

  /* The component has been found in the directory.  Get inode. */
  if ( (rip = get_dir_record((int) numb)) == NULL)  {
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
