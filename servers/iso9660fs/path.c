#include "inc.h"
#include <string.h>
#include <minix/com.h>
#include <minix/vfsif.h>

#include "buf.h"

static char *get_name(char *name, char string[NAME_MAX+1]);
static int parse_path(ino_t dir_ino, ino_t root_ino, int flags, struct
	dir_record **res_inop, size_t *offsetp);


/*===========================================================================*
 *                             fs_lookup				     *
 *===========================================================================*/
int fs_lookup() {
  cp_grant_id_t grant;
  int r, len, flags;
  size_t offset;
  ino_t dir_ino, root_ino;
  struct dir_record *dir;

  grant		= fs_m_in.m_vfs_fs_lookup.grant_path;
  len		= fs_m_in.m_vfs_fs_lookup.path_len;	/* including terminating nul */
  dir_ino	= fs_m_in.m_vfs_fs_lookup.dir_ino;
  root_ino	= fs_m_in.m_vfs_fs_lookup.root_ino;
  flags		= fs_m_in.m_vfs_fs_lookup.flags;
  caller_uid	= fs_m_in.m_vfs_fs_lookup.uid;
  caller_gid	= fs_m_in.m_vfs_fs_lookup.gid;

  /* Check length. */
  if(len > sizeof(user_path)) return(E2BIG);	/* too big for buffer */
  if(len < 1) return(EINVAL);			/* too small */

  /* Copy the pathname and set up caller's user and group id */
  r = sys_safecopyfrom(VFS_PROC_NR, grant, 0, (vir_bytes) user_path, 
		       (phys_bytes) len);
  if (r != OK) {
	printf("ISOFS %s:%d sys_safecopyfrom failed: %d\n",
		__FILE__, __LINE__, r);
	return(r);
  }

  /* Verify this is a null-terminated path. */
  if(user_path[len-1] != '\0') return(EINVAL);

  /* Lookup inode */
  dir = NULL;
  offset = 0;
  r = parse_path(dir_ino, root_ino, flags, &dir, &offset);

  if (r == ELEAVEMOUNT) {
	/* Report offset and the error */
	fs_m_out.m_fs_vfs_lookup.offset = offset;
	fs_m_out.m_fs_vfs_lookup.symloop = 0;
	return(r);
  }

  if (r != OK && r != EENTERMOUNT) return(r);

  fs_m_out.m_fs_vfs_lookup.inode	= ID_DIR_RECORD(dir);
  fs_m_out.m_fs_vfs_lookup.mode		= dir->d_mode;
  fs_m_out.m_fs_vfs_lookup.file_size	= dir->d_file_size;
  fs_m_out.m_fs_vfs_lookup.symloop	= 0;
  fs_m_out.m_fs_vfs_lookup.uid		= SYS_UID;	/* root */
  fs_m_out.m_fs_vfs_lookup.gid		= SYS_GID;	/* operator */

  if (r == EENTERMOUNT) { 
  	fs_m_out.m_fs_vfs_lookup.offset = offset;
	release_dir_record(dir);
  }

  return(r);
}

/* The search dir actually performs the operation of searching for the
 * compoent ``string" in ldir_ptr. It returns the response and the number of
 * the inode in numb. */
/*===========================================================================*
 *				search_dir				     *
 *===========================================================================*/
int search_dir(
	register struct dir_record *ldir_ptr,	/* dir record parent */
	char string[NAME_MAX],			/* component to search for */
	ino_t *numb				/* pointer to new dir record */
) {
  struct dir_record *dir_tmp;
  register struct buf *bp;
  int pos;
  char* comma_pos = NULL;
  char tmp_string[NAME_MAX];

  /* This function search a particular element (in string) in a inode and
   * return its number */

  /* Initialize the tmp array */
  memset(tmp_string,'\0',NAME_MAX);

  if ((ldir_ptr->d_mode & I_TYPE) != I_DIRECTORY) {
    return(ENOTDIR);
  }
  
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

  if (bp == NULL)
    return EINVAL;

  while (pos < v_pri.logical_block_size_l) {
    if ((dir_tmp = get_free_dir_record()) == NULL) {
      put_block(bp);
      return EINVAL;
    }

    if (create_dir_record(dir_tmp,b_data(bp) + pos,
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
	create_ext_attr(dir_tmp->ext_attr,b_data(bp));
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


/*===========================================================================*
 *                             parse_path				     *
 *===========================================================================*/
static int parse_path(
ino_t dir_ino,
ino_t root_ino,
int flags,
struct dir_record **res_inop,
size_t *offsetp
) {
  int r;
  char string[NAME_MAX+1];
  char *cp, *ncp;
  struct dir_record *start_dir, *old_dir;

  /* Find starting inode inode according to the request message */
  if ((start_dir = get_dir_record(dir_ino)) == NULL) {
    printf("ISOFS: couldn't find starting inode %llu\n", dir_ino);
    return(ENOENT);
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
	strlcpy(string, ".", NAME_MAX + 1);
	ncp = cp;
      }
      else
	ncp = get_name(cp, string);
    } else
      /* Just get the first component */
      ncp = get_name(cp, string);
  
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

    r = advance(old_dir, string, &start_dir);

    if (r != OK) {
      release_dir_record(old_dir);
      return r;
    }

    release_dir_record(old_dir);
    cp = ncp;
  }
}


/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
int advance(dirp, string, resp)
struct dir_record *dirp;		/* inode for directory to be searched */
char string[NAME_MAX];		        /* component name to look for */
struct dir_record **resp;		/* resulting inode */
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the inode, open it, and return a pointer to its inode
 * slot.
 */

  register struct dir_record *rip = NULL;
  int r;
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
static char *get_name(path_name, string)
char *path_name;		/* path name to parse */
char string[NAME_MAX+1];	/* component extracted from 'old_name' */
{
/* Given a pointer to a path name in fs space, 'path_name', copy the first
 * component to 'string' (truncated if necessary, always nul terminated).
 * A pointer to the string after the first component of the name as yet
 * unparsed is returned.  Roughly speaking,
 * 'get_name' = 'path_name' - 'string'.
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
	strlcpy(string, ".", NAME_MAX + 1);
  }
  else
  {
	memcpy(string, cp, len);
	string[len]= '\0';
  }

  return ep;
}
