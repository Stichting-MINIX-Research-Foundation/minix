/* lookup() is the main routine that controls the path name lookup. It  
 * handles mountpoints and symbolic links. The actual lookup requests
 * are sent through the req_lookup wrapper function.
 */

#include "fs.h"
#include <string.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <unistd.h>
#include <assert.h>
#include <minix/vfsif.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <dirent.h>
#include "fproc.h"
#include "vmnt.h"
#include "vnode.h"
#include "param.h"

/* Set to following define to 1 if you really want to use the POSIX definition
 * (IEEE Std 1003.1, 2004) of pathname resolution. POSIX requires pathnames
 * with a traling slash (and that do not entirely consist of slash characters)
 * to be treated as if a single dot is appended. This means that for example
 * mkdir("dir/", ...) and rmdir("dir/") will fail because the call tries to
 * create or remove the directory '.'. Historically, Unix systems just ignore
 * trailing slashes.
 */
#define DO_POSIX_PATHNAME_RES	0

FORWARD _PROTOTYPE( int lookup, (struct vnode *dirp, int flags,
				 node_details_t *node, struct fproc *rfp));

/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
PUBLIC struct vnode *advance(dirp, flags, rfp)
struct vnode *dirp;
int flags;
struct fproc *rfp;
{
/* Resolve a pathname (in user_fullpath) starting at dirp to a vnode. */
  int r;
  struct vnode *new_vp, *vp;
  struct vmnt *vmp;
  struct node_details res;

  assert(dirp);

  /* Get a free vnode */
  if((new_vp = get_free_vnode()) == NULL) return(NULL);
  
  /* Lookup vnode belonging to the file. */
  if ((r = lookup(dirp, flags, &res, rfp)) != OK) {
	err_code = r;
	return(NULL);
  }
  
  /* Check whether vnode is already in use or not */
  if ((vp = find_vnode(res.fs_e, res.inode_nr)) != NULL) {
	  dup_vnode(vp);
	  vp->v_fs_count++;	/* We got a reference from the FS */
	  return(vp);
  }

  /* Fill in the free vnode's fields */
  new_vp->v_fs_e = res.fs_e;
  new_vp->v_inode_nr = res.inode_nr;
  new_vp->v_mode = res.fmode;
  new_vp->v_size = res.fsize;
  new_vp->v_uid = res.uid;
  new_vp->v_gid = res.gid;
  new_vp->v_sdev = res.dev;
  
  if( (vmp = find_vmnt(new_vp->v_fs_e)) == NULL)
	  panic("VFS advance: vmnt not found");

  new_vp->v_vmnt = vmp; 
  new_vp->v_dev = vmp->m_dev;
  new_vp->v_fs_count = 1;
  new_vp->v_ref_count = 1;
  
  return(new_vp);
}


/*===========================================================================*
 *				eat_path				     *
 *===========================================================================*/
PUBLIC struct vnode *eat_path(flags, rfp)
int flags;
struct fproc *rfp;
{
/* Resolve 'user_fullpath' to a vnode. advance does the actual work. */
  struct vnode *vp;

  vp = (user_fullpath[0] == '/' ? rfp->fp_rd : rfp->fp_wd);
  return advance(vp, flags, rfp);
}


/*===========================================================================*
 *				last_dir				     *
 *===========================================================================*/
PUBLIC struct vnode *last_dir(rfp)
struct fproc *rfp;
{
/* Parse a path, 'user_fullpath', as far as the last directory, fetch the vnode
 * for the last directory into the vnode table, and return a pointer to the
 * vnode. In addition, return the final component of the path in 'string'. If
 * the last directory can't be opened, return NULL and the reason for
 * failure in 'err_code'. We can't parse component by component as that would
 * be too expensive. Alternatively, we cut off the last component of the path,
 * and parse the path up to the penultimate component.
 */  

  size_t len;
  char *cp;
  char dir_entry[PATH_MAX+1];
  struct vnode *vp, *res;
  
  /* Is the path absolute or relative? Initialize 'vp' accordingly. */
  vp = (user_fullpath[0] == '/' ? rfp->fp_rd : rfp->fp_wd);

  len = strlen(user_fullpath);

  /* If path is empty, return ENOENT. */
  if (len == 0)	{
	err_code = ENOENT;
	return(NULL); 
  }

#if !DO_POSIX_PATHNAME_RES
  /* Remove trailing slashes */
  while (len > 1 && user_fullpath[len-1] == '/') {
	  len--;
	  user_fullpath[len]= '\0';
  }
#endif

  cp = strrchr(user_fullpath, '/');
  if (cp == NULL) {
	  /* Just one entry in the current working directory */
	  dup_vnode(vp);
	  return(vp);
  } else if (cp[1] == '\0') {
	  /* Path ends in a slash. The directory entry is '.' */
	  strcpy(dir_entry, ".");
  } else {
	  /* A path name for the directory and a directory entry */
	  strcpy(dir_entry, cp+1);
	  cp[1]= '\0';
  }

  /* Remove trailing slashes */
  while(cp > user_fullpath && cp[0] == '/') {
	  cp[0]= '\0';
	  cp--;
  }

  res = advance(vp, PATH_NOFLAGS, rfp);
  if (res == NULL) return(NULL);

  /* Copy the directory entry back to user_fullpath */
  strcpy(user_fullpath, dir_entry);
  
  return(res);
}


/*===========================================================================*
 *				lookup					     *
 *===========================================================================*/
PRIVATE int lookup(start_node, flags, node, rfp)
struct vnode *start_node;
int flags;
node_details_t *node;
struct fproc *rfp;
{
/* Resolve a pathname (in user_fullpath) relative to start_node. */

  int r, symloop;
  endpoint_t fs_e;
  size_t path_off, path_left_len;
  ino_t dir_ino, root_ino;
  uid_t uid;
  gid_t gid;
  struct vnode *dir_vp;
  struct vmnt *vmp;
  struct lookup_res res;

  /* Empty (start) path? */
  if (user_fullpath[0] == '\0') {
	node->inode_nr = 0;
	return(ENOENT);
  }

  if(!rfp->fp_rd || !rfp->fp_wd) {
	printf("VFS: lookup_rel %d: no rd/wd\n", rfp->fp_endpoint);
	return(ENOENT);
  }

  fs_e = start_node->v_fs_e;
  dir_ino = start_node->v_inode_nr;
 
  /* Is the process' root directory on the same partition?,
   * if so, set the chroot directory too. */
  if (rfp->fp_rd->v_dev == rfp->fp_wd->v_dev)
	root_ino = rfp->fp_rd->v_inode_nr; 
  else
	root_ino = 0;

  /* Set user and group ids according to the system call */
  uid = (call_nr == ACCESS ? rfp->fp_realuid : rfp->fp_effuid); 
  gid = (call_nr == ACCESS ? rfp->fp_realgid : rfp->fp_effgid); 

  symloop = 0;	/* Number of symlinks seen so far */

  /* Issue the request */
  r = req_lookup(fs_e, dir_ino, root_ino, uid, gid, flags, &res, rfp);

  if (r != OK && r != EENTERMOUNT && r != ELEAVEMOUNT && r != ESYMLINK)
	return(r); /* i.e., an error occured */

  /* While the response is related to mount control set the 
   * new requests respectively */
  while(r == EENTERMOUNT || r == ELEAVEMOUNT || r == ESYMLINK) {
	/* Update user_fullpath to reflect what's left to be parsed. */
	path_off = res.char_processed;
	path_left_len = strlen(&user_fullpath[path_off]);
	memmove(user_fullpath, &user_fullpath[path_off], path_left_len);
	user_fullpath[path_left_len] = '\0'; /* terminate string */ 

	/* Update the current value of the symloop counter */
	symloop += res.symloop;
	if (symloop > SYMLOOP_MAX)
		return(ELOOP);

	/* Symlink encountered with absolute path */
	if (r == ESYMLINK) {
		dir_vp = rfp->fp_rd;
	} else if (r == EENTERMOUNT) {
		/* Entering a new partition */
		dir_vp = 0;
		/* Start node is now the mounted partition's root node */
		for (vmp = &vmnt[0]; vmp != &vmnt[NR_MNTS]; ++vmp) {
			if (vmp->m_dev != NO_DEV && vmp->m_mounted_on) {
			   if (vmp->m_mounted_on->v_inode_nr == res.inode_nr &&
			       vmp->m_mounted_on->v_fs_e == res.fs_e) {
				dir_vp = vmp->m_root_node;
				break;
			   }
			}
		}
		assert(dir_vp);
	} else {
		/* Climbing up mount */
		/* Find the vmnt that represents the partition on
		 * which we "climb up". */
		if ((vmp = find_vmnt(res.fs_e)) == NULL) {
			panic("VFS lookup: can't find parent vmnt");
		}	  

		/* Make sure that the child FS does not feed a bogus path
		 * to the parent FS. That is, when we climb up the tree, we
		 * must've encountered ".." in the path, and that is exactly
		 * what we're going to feed to the parent */
		if(strncmp(user_fullpath, "..", 2) != 0 ||
			(user_fullpath[2] != '\0' && user_fullpath[2] != '/')) {
			printf("VFS: bogus path: %s\n", user_fullpath);
			return(ENOENT);
		}

		/* Start node is the vnode on which the partition is
		 * mounted */
		dir_vp = vmp->m_mounted_on;
	}

	/* Set the starting directories inode number and FS endpoint */
	fs_e = dir_vp->v_fs_e;
	dir_ino = dir_vp->v_inode_nr;

	/* Is the process' root directory on the same partition?,
	 * if so, set the chroot directory too. */
	if(dir_vp->v_dev == rfp->fp_rd->v_dev)
		root_ino = rfp->fp_rd->v_inode_nr; 
	else
		root_ino = 0;

	r = req_lookup(fs_e, dir_ino, root_ino, uid, gid, flags, &res, rfp);

	if(r != OK && r != EENTERMOUNT && r != ELEAVEMOUNT && r != ESYMLINK)
		return(r);
  }

  /* Fill in response fields */
  node->inode_nr = res.inode_nr;
  node->fmode = res.fmode;
  node->fsize = res.fsize;
  node->dev = res.dev;
  node->fs_e = res.fs_e;
  node->uid = res.uid;
  node->gid = res.gid;
  
  return(r);
}

/*===========================================================================*
 *				get_name				     *
 *===========================================================================*/
PUBLIC int get_name(dirp, entry, ename)
struct vnode *dirp;
struct vnode *entry;
char ename[NAME_MAX + 1];
{
  u64_t pos = {0, 0}, new_pos;
  int r, consumed, totalbytes;
  char buf[(sizeof(struct dirent) + NAME_MAX) * 8];
  struct dirent *cur;

  if ((dirp->v_mode & I_TYPE) != I_DIRECTORY) {
	return(EBADF);
  }

  do {
	r = req_getdents(dirp->v_fs_e, dirp->v_inode_nr, pos, 
						buf, sizeof(buf), &new_pos, 1);

	if (r == 0) {
		return(ENOENT); /* end of entries -- matching inode !found */
	} else if (r < 0) {
		return(r); /* error */
	}

	consumed = 0; /* bytes consumed */
	totalbytes = r; /* number of bytes to consume */

	do {
		cur = (struct dirent *) (buf + consumed);
		if (entry->v_inode_nr == cur->d_ino) {
			/* found the entry we were looking for */
			strncpy(ename, cur->d_name, NAME_MAX);
			ename[NAME_MAX] = '\0';
			return(OK);
		}

		/* not a match -- move on to the next dirent */
		consumed += cur->d_reclen;
	} while (consumed < totalbytes);

	pos = new_pos;
  } while (1);
}

/*===========================================================================*
 *				canonical_path				     *
 *===========================================================================*/
PUBLIC int canonical_path(orig_path, canon_path, rfp)
char *orig_path;
char *canon_path; /* should have length PATH_MAX+1 */
struct fproc *rfp;
{
  int len = 0;
  int r, symloop = 0;
  struct vnode *dir_vp, *parent_dir;
  char component[NAME_MAX+1];
  char link_path[PATH_MAX+1];

  dir_vp = NULL;
  strncpy(user_fullpath, orig_path, PATH_MAX);

  do {
	if (dir_vp) put_vnode(dir_vp);

	/* Resolve to the last directory holding the socket file */
	if ((dir_vp = last_dir(rfp)) == NULL) {
		return(err_code);
	}

	/* dir_vp points to dir and user_fullpath now contains only the
	 * filename.
	 */
	strcpy(canon_path, user_fullpath); /* Store file name */

	/* check if the file is a symlink, if so resolve it */
	r = rdlink_direct(canon_path, link_path, rfp);
	if (r <= 0) {
		strcpy(user_fullpath, canon_path);
		break;
	}

	/* encountered a symlink -- loop again */
	strcpy(user_fullpath, link_path);

	symloop++;
  } while (symloop < SYMLOOP_MAX);

  if (symloop >= SYMLOOP_MAX) {
	if (dir_vp) put_vnode(dir_vp);
	return ELOOP;
  }

  while(dir_vp != rfp->fp_rd) {

	strcpy(user_fullpath, "..");

	/* check if we're at the root node of the file system */
	if (dir_vp->v_vmnt->m_root_node == dir_vp) {
		put_vnode(dir_vp);
		dir_vp = dir_vp->v_vmnt->m_mounted_on;
		dup_vnode(dir_vp);
	}

	if ((parent_dir = advance(dir_vp, PATH_NOFLAGS, rfp)) == NULL) {
		put_vnode(dir_vp);
		return(err_code);
	}

	/* now we have to retrieve the name of the parent directory */
	if (get_name(parent_dir, dir_vp, component) != OK) {
		put_vnode(dir_vp);
		put_vnode(parent_dir);
		return(ENOENT);
	}

	len += strlen(component) + 1;
	if (len > PATH_MAX) {
		/* adding the component to canon_path would exceed PATH_MAX */
		put_vnode(dir_vp);
		put_vnode(parent_dir);
		return(ENOMEM);
	}

	/* store result of component in canon_path */

	/* first make space by moving the contents of canon_path to
	 * the right. Move strlen + 1 bytes to include the terminating '\0'.
	 */
	memmove(canon_path+strlen(component)+1, canon_path, 
						strlen(canon_path) + 1);

	/* Copy component into canon_path */
	memmove(canon_path, component, strlen(component));

	/* Put slash into place */
	canon_path[strlen(component)] = '/';

	/* Store parent_dir result, and continue the loop once more */
	put_vnode(dir_vp);
	dir_vp = parent_dir;
  }

  put_vnode(dir_vp);

  /* add the leading slash */
  if (strlen(canon_path) >= PATH_MAX) return(ENAMETOOLONG);
  memmove(canon_path+1, canon_path, strlen(canon_path));
  canon_path[0] = '/';

  return(OK);
}

/*===========================================================================*
 *				check_perms				     *
 *===========================================================================*/
PUBLIC int check_perms(ep, io_gr, pathlen)
endpoint_t ep;
cp_grant_id_t io_gr;
int pathlen;
{
  int r, i;
  struct vnode *vp;
  struct fproc *rfp;
  char orig_path[PATH_MAX+1];
  char canon_path[PATH_MAX+1];

  i = _ENDPOINT_P(ep);
  if (pathlen < UNIX_PATH_MAX || pathlen > PATH_MAX || i < 0 || i >= NR_PROCS) {
	return EINVAL;
  }
  rfp = &(fproc[i]);

  memset(canon_path, '\0', PATH_MAX+1);

  r = sys_safecopyfrom(PFS_PROC_NR, io_gr, (vir_bytes) 0,
				(vir_bytes) &user_fullpath, pathlen, D);
  if (r != OK) {
	return r;
  }
  user_fullpath[pathlen] = '\0';

  /* save path from pfs before permissions checking modifies it */
  memcpy(orig_path, user_fullpath, PATH_MAX+1);

  /* get the canonical path to the socket file */
  r = canonical_path(orig_path, canon_path, rfp);
  if (r != OK) {
	return r;
  }

  if (strlen(canon_path) >= pathlen) {
	return ENAMETOOLONG;
  }

  /* copy canon_path back to PFS */
  r = sys_safecopyto(PFS_PROC_NR, (cp_grant_id_t) io_gr, (vir_bytes) 0, 
				(vir_bytes) canon_path, strlen(canon_path)+1,
				D);
  if (r != OK) {
	return r;
  }

  /* reload user_fullpath for permissions checking */
  memcpy(user_fullpath, orig_path, PATH_MAX+1);
  if ((vp = eat_path(PATH_NOFLAGS, rfp)) == NULL) {
	return(err_code);
  }

  /* check permissions */
  r = forbidden(vp, (R_BIT | W_BIT));

  put_vnode(vp);
  return(r);
}

/*===========================================================================*
 *				do_check_perms				     *
 *===========================================================================*/
PUBLIC int do_check_perms(void)
{
  return check_perms(m_in.IO_ENDPT, (cp_grant_id_t) m_in.IO_GRANT, m_in.COUNT);
}
