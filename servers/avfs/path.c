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
#include "threads.h"
#include "vmnt.h"
#include "vnode.h"
#include "path.h"
#include "fproc.h"
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

FORWARD _PROTOTYPE( int lookup, (struct vnode *dirp, struct lookup *resolve,
				 node_details_t *node, struct fproc *rfp));
FORWARD _PROTOTYPE( int check_perms, (endpoint_t ep, cp_grant_id_t io_gr,
				      size_t pathlen)			);

/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
PUBLIC struct vnode *advance(dirp, resolve, rfp)
struct vnode *dirp;
struct lookup *resolve;
struct fproc *rfp;
{
/* Resolve a path name starting at dirp to a vnode. */
  int r;
  int do_downgrade = 1;
  struct vnode *new_vp, *vp;
  struct vmnt *vmp;
  struct node_details res = {0,0,0,0,0,0,0};
  tll_access_t initial_locktype;

  assert(dirp);
  assert(resolve->l_vnode_lock != TLL_NONE);
  assert(resolve->l_vmnt_lock != TLL_NONE);

  if (resolve->l_vnode_lock == VNODE_READ)
	initial_locktype = VNODE_OPCL;
  else
	initial_locktype = resolve->l_vnode_lock;

  /* Get a free vnode and lock it */
  if ((new_vp = get_free_vnode()) == NULL) return(NULL);
  lock_vnode(new_vp, initial_locktype);

  /* Lookup vnode belonging to the file. */
  if ((r = lookup(dirp, resolve, &res, rfp)) != OK) {
	err_code = r;
	unlock_vnode(new_vp);
	return(NULL);
  }

  /* Check whether we already have a vnode for that file */
  if ((vp = find_vnode(res.fs_e, res.inode_nr)) != NULL) {
	unlock_vnode(new_vp);	/* Don't need this anymore */
	do_downgrade = (lock_vnode(vp, initial_locktype) != EBUSY);

	/* Unfortunately, by the time we get the lock, another thread might've
	 * rid of the vnode (e.g., find_vnode found the vnode while a
	 * req_putnode was being processed). */
	if (vp->v_ref_count == 0) { /* vnode vanished! */
		/* As the lookup before increased the usage counters in the FS,
		 * we can simply set the usage counters to 1 and proceed as
		 * normal, because the putnode resulted in a use count of 1 in
		 * the FS. Other data is still valid, because the vnode was
		 * marked as pending lock, so get_free_vnode hasn't
		 * reinitialized the vnode yet. */
		vp->v_fs_count = 1;
		if (vp->v_mapfs_e != NONE) vp->v_mapfs_count = 1;
	} else {
		vp->v_fs_count++;	/* We got a reference from the FS */
	}

  } else {
	/* Vnode not found, fill in the free vnode's fields */

	new_vp->v_fs_e = res.fs_e;
	new_vp->v_inode_nr = res.inode_nr;
	new_vp->v_mode = res.fmode;
	new_vp->v_size = res.fsize;
	new_vp->v_uid = res.uid;
	new_vp->v_gid = res.gid;
	new_vp->v_sdev = res.dev;

	if( (vmp = find_vmnt(new_vp->v_fs_e)) == NULL)
		  panic("advance: vmnt not found");

	new_vp->v_vmnt = vmp;
	new_vp->v_dev = vmp->m_dev;
	new_vp->v_fs_count = 1;

	vp = new_vp;
  }

  dup_vnode(vp);
  if (do_downgrade) {
	/* Only downgrade a lock if we managed to lock it in the first place */
	*(resolve->l_vnode) = vp;

	if (initial_locktype != resolve->l_vnode_lock)
		tll_downgrade(&vp->v_lock);

#if LOCK_DEBUG
	if (resolve->l_vnode_lock == VNODE_READ)
		fp->fp_vp_rdlocks++;
#endif
  }

  return(vp);
}


/*===========================================================================*
 *				eat_path				     *
 *===========================================================================*/
PUBLIC struct vnode *eat_path(resolve, rfp)
struct lookup *resolve;
struct fproc *rfp;
{
/* Resolve path to a vnode. advance does the actual work. */
  struct vnode *start_dir;

  start_dir = (resolve->l_path[0] == '/' ? rfp->fp_rd : rfp->fp_wd);
  return advance(start_dir, resolve, rfp);
}


/*===========================================================================*
 *				last_dir				     *
 *===========================================================================*/
PUBLIC struct vnode *last_dir(resolve, rfp)
struct lookup *resolve;
struct fproc *rfp;
{
/* Parse a path, as far as the last directory, fetch the vnode
 * for the last directory into the vnode table, and return a pointer to the
 * vnode. In addition, return the final component of the path in 'string'. If
 * the last directory can't be opened, return NULL and the reason for
 * failure in 'err_code'. We can't parse component by component as that would
 * be too expensive. Alternatively, we cut off the last component of the path,
 * and parse the path up to the penultimate component.
 */

  size_t len;
  char *cp;
  char dir_entry[NAME_MAX+1];
  struct vnode *start_dir, *res;
  int r;

  *resolve->l_vnode = NULL;
  *resolve->l_vmp = NULL;

  /* Is the path absolute or relative? Initialize 'start_dir' accordingly. */
  start_dir = (resolve->l_path[0] == '/' ? rfp->fp_rd : rfp->fp_wd);

  len = strlen(resolve->l_path);

  /* If path is empty, return ENOENT. */
  if (len == 0)	{
	err_code = ENOENT;
	return(NULL);
  }

#if !DO_POSIX_PATHNAME_RES
  /* Remove trailing slashes */
  while (len > 1 && resolve->l_path[len-1] == '/') {
	  len--;
	  resolve->l_path[len]= '\0';
  }
#endif

  cp = strrchr(resolve->l_path, '/');
  if (cp == NULL) {
	/* Just one entry in the current working directory */
	struct vmnt *vmp;

	vmp = find_vmnt(start_dir->v_fs_e);
	r = lock_vmnt(vmp, resolve->l_vmnt_lock);
	if (r == EDEADLK)
		return(NULL);
	else if (r == OK)
		*resolve->l_vmp = vmp;

	lock_vnode(start_dir, resolve->l_vnode_lock);
	*resolve->l_vnode = start_dir;
	dup_vnode(start_dir);
	return(start_dir);

  } else if (cp[1] == '\0') {
	/* Path ends in a slash. The directory entry is '.' */
	strcpy(dir_entry, ".");
  } else {
	/* A path name for the directory and a directory entry */
	strncpy(dir_entry, cp+1, NAME_MAX);
	cp[1] = '\0';
	dir_entry[NAME_MAX] = '\0';
  }

  /* Remove trailing slashes */
  while(cp > resolve->l_path && cp[0] == '/') {
	cp[0]= '\0';
	cp--;
  }

  resolve->l_flags = PATH_NOFLAGS;
  res = advance(start_dir, resolve, rfp);
  if (res == NULL) return(NULL);

  /* Copy the directory entry back to user_fullpath */
  strncpy(resolve->l_path, dir_entry, NAME_MAX + 1);

  return(res);
}

/*===========================================================================*
 *				lookup					     *
 *===========================================================================*/
PRIVATE int lookup(start_node, resolve, result_node, rfp)
struct vnode *start_node;
struct lookup *resolve;
node_details_t *result_node;
struct fproc *rfp;
{
/* Resolve a path name relative to start_node. */

  int r, symloop;
  endpoint_t fs_e;
  size_t path_off, path_left_len;
  ino_t dir_ino, root_ino;
  uid_t uid;
  gid_t gid;
  struct vnode *dir_vp;
  struct vmnt *vmp, *vmpres;
  struct lookup_res res;

  assert(resolve->l_vmp);
  assert(resolve->l_vnode);

  *(resolve->l_vmp) = vmpres = NULL; /* No vmnt found nor locked yet */

  /* Empty (start) path? */
  if (resolve->l_path[0] == '\0') {
	result_node->inode_nr = 0;
	return(ENOENT);
  }

  if (!rfp->fp_rd || !rfp->fp_wd) {
	printf("VFS: lookup %d: no rd/wd\n", rfp->fp_endpoint);
	return(ENOENT);
  }

  fs_e = start_node->v_fs_e;
  dir_ino = start_node->v_inode_nr;
  vmpres = find_vmnt(fs_e);

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

  /* Lock vmnt */
  if ((r = lock_vmnt(vmpres, resolve->l_vmnt_lock)) != OK) {
	if (r == EBUSY) /* vmnt already locked */
		vmpres = NULL;
	else
		return(r);
  }
  *(resolve->l_vmp) = vmpres;

  /* Issue the request */
  r = req_lookup(fs_e, dir_ino, root_ino, uid, gid, resolve, &res, rfp);

  if (r != OK && r != EENTERMOUNT && r != ELEAVEMOUNT && r != ESYMLINK) {
	if (vmpres) unlock_vmnt(vmpres);
	*(resolve->l_vmp) = NULL;
	return(r); /* i.e., an error occured */
  }

  /* While the response is related to mount control set the
   * new requests respectively */
  while (r == EENTERMOUNT || r == ELEAVEMOUNT || r == ESYMLINK) {
	/* Update user_fullpath to reflect what's left to be parsed. */
	path_off = res.char_processed;
	path_left_len = strlen(&resolve->l_path[path_off]);
	memmove(resolve->l_path, &resolve->l_path[path_off], path_left_len);
	resolve->l_path[path_left_len] = '\0'; /* terminate string */

	/* Update the current value of the symloop counter */
	symloop += res.symloop;
	if (symloop > SYMLOOP_MAX) {
		if (vmpres) unlock_vmnt(vmpres);
		*(resolve->l_vmp) = NULL;
		return(ELOOP);
	}

	/* Symlink encountered with absolute path */
	if (r == ESYMLINK) {
		dir_vp = rfp->fp_rd;
		vmp = NULL;
	} else if (r == EENTERMOUNT) {
		/* Entering a new partition */
		dir_vp = NULL;
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
		if (dir_vp == NULL) {
			printf("VFS: path lookup error; root node not found\n");
			if (vmpres) unlock_vmnt(vmpres);
			*(resolve->l_vmp) = NULL;
			return(EIO);
		}
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
		if(strncmp(resolve->l_path, "..", 2) != 0 ||
		   (resolve->l_path[2] != '\0' && resolve->l_path[2] != '/')) {
			printf("VFS: bogus path: %s\n", resolve->l_path);
			if (vmpres) unlock_vmnt(vmpres);
			*(resolve->l_vmp) = NULL;
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
	if (dir_vp->v_dev == rfp->fp_rd->v_dev)
		root_ino = rfp->fp_rd->v_inode_nr;
	else
		root_ino = 0;

	/* Unlock a previously locked vmnt if locked and lock new vmnt */
	if (vmpres) unlock_vmnt(vmpres);
	vmpres = find_vmnt(fs_e);
	if ((r = lock_vmnt(vmpres, resolve->l_vmnt_lock)) != OK) {
		if (r == EBUSY)
			vmpres = NULL;	/* Already locked */
		else
			return(r);
	}
	*(resolve->l_vmp) = vmpres;

	r = req_lookup(fs_e, dir_ino, root_ino, uid, gid, resolve, &res, rfp);

	if (r != OK && r != EENTERMOUNT && r != ELEAVEMOUNT && r != ESYMLINK) {
		if (vmpres) unlock_vmnt(vmpres);
		*(resolve->l_vmp) = NULL;
		return(r);
	}
  }

  /* Fill in response fields */
  result_node->inode_nr = res.inode_nr;
  result_node->fmode = res.fmode;
  result_node->fsize = res.fsize;
  result_node->dev = res.dev;
  result_node->fs_e = res.fs_e;
  result_node->uid = res.uid;
  result_node->gid = res.gid;

  return(r);
}

/*===========================================================================*
 *				lookup_init				     *
 *===========================================================================*/
PUBLIC void lookup_init(resolve, path, flags, vmp, vp)
struct lookup *resolve;
char *path;
int flags;
struct vmnt **vmp;
struct vnode **vp;
{
  assert(vmp != NULL);
  assert(vp != NULL);

  resolve->l_path = path;
  resolve->l_flags = flags;
  resolve->l_vmp = vmp;
  resolve->l_vnode = vp;
  resolve->l_vmnt_lock = TLL_NONE;
  resolve->l_vnode_lock = TLL_NONE;
  *vmp = NULL;	/* Initialize lookup result to NULL */
  *vp = NULL;
}

/*===========================================================================*
 *				get_name				     *
 *===========================================================================*/
PUBLIC int get_name(dirp, entry, ename)
struct vnode *dirp;
struct vnode *entry;
char ename[NAME_MAX + 1];
{
  u64_t pos, new_pos;
  int r, consumed, totalbytes;
  char buf[(sizeof(struct dirent) + NAME_MAX) * 8];
  struct dirent *cur;

  pos = make64(0, 0);

  if ((dirp->v_mode & I_TYPE) != I_DIRECTORY) {
	return(EBADF);
  }

  do {
	r = req_getdents(dirp->v_fs_e, dirp->v_inode_nr, pos, buf, sizeof(buf),
			 &new_pos, 1);

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
PUBLIC int canonical_path(orig_path, rfp)
char orig_path[PATH_MAX];
struct fproc *rfp;
{
/* Find canonical path of a given path */
  int len = 0;
  int r, symloop = 0;
  struct vnode *dir_vp, *parent_dir;
  struct vmnt *dir_vmp, *parent_vmp;
  char component[NAME_MAX+1];	/* NAME_MAX does /not/ include '\0' */
  char temp_path[PATH_MAX];
  struct lookup resolve;

  dir_vp = NULL;
  strncpy(temp_path, orig_path, PATH_MAX);
  temp_path[PATH_MAX - 1] = '\0';

  /* First resolve path to the last directory holding the file */
  do {
	if (dir_vp) {
		unlock_vnode(dir_vp);
		unlock_vmnt(dir_vmp);
		put_vnode(dir_vp);
	}

	lookup_init(&resolve, temp_path, PATH_NOFLAGS, &dir_vmp, &dir_vp);
	resolve.l_vmnt_lock = VMNT_READ;
	resolve.l_vnode_lock = VNODE_READ;
	if ((dir_vp = last_dir(&resolve, rfp)) == NULL) return(err_code);

	/* dir_vp points to dir and resolve path now contains only the
	 * filename.
	 */
	strncpy(orig_path, temp_path, NAME_MAX);	/* Store file name */

	/* check if the file is a symlink, if so resolve it */
	r = rdlink_direct(orig_path, temp_path, rfp);

	if (r <= 0)
		break;

	/* encountered a symlink -- loop again */
	strncpy(orig_path, temp_path, PATH_MAX - 1);
	symloop++;
  } while (symloop < SYMLOOP_MAX);

  if (symloop >= SYMLOOP_MAX) {
	if (dir_vp) {
		unlock_vnode(dir_vp);
		unlock_vmnt(dir_vmp);
		put_vnode(dir_vp);
	}
	return(ELOOP);
  }

  /* We've got the filename and the actual directory holding the file. From
   * here we start building up the canonical path by climbing up the tree */
  while (dir_vp != rfp->fp_rd) {

	strcpy(temp_path, "..");

	/* check if we're at the root node of the file system */
	if (dir_vp->v_vmnt->m_root_node == dir_vp) {
		unlock_vnode(dir_vp);
		unlock_vmnt(dir_vmp);
		put_vnode(dir_vp);
		dir_vp = dir_vp->v_vmnt->m_mounted_on;
		dir_vmp = dir_vp->v_vmnt;
		assert(lock_vmnt(dir_vmp, VMNT_READ) == OK);
		assert(lock_vnode(dir_vp, VNODE_READ) == OK);
		dup_vnode(dir_vp);
	}

	lookup_init(&resolve, temp_path, PATH_NOFLAGS, &parent_vmp,
		    &parent_dir);
	resolve.l_vmnt_lock = VMNT_READ;
	resolve.l_vnode_lock = VNODE_READ;

	if ((parent_dir = advance(dir_vp, &resolve, rfp)) == NULL) {
		unlock_vnode(dir_vp);
		unlock_vmnt(dir_vmp);
		put_vnode(dir_vp);
		return(err_code);
	}

	/* now we have to retrieve the name of the parent directory */
	if (get_name(parent_dir, dir_vp, component) != OK) {
		unlock_vnode(parent_dir);
		unlock_vmnt(parent_vmp);
		unlock_vnode(dir_vp);
		unlock_vmnt(dir_vmp);
		put_vnode(parent_dir);
		put_vnode(dir_vp);
		return(ENOENT);
	}

	len += strlen(component) + 1;
	if (len >= PATH_MAX) {
		/* adding the component to orig_path would exceed PATH_MAX */
		unlock_vnode(parent_dir);
		unlock_vmnt(parent_vmp);
		unlock_vnode(dir_vp);
		unlock_vmnt(dir_vmp);
		put_vnode(parent_dir);
		put_vnode(dir_vp);
		return(ENOMEM);
	}

	/* Store result of component in orig_path. First make space by moving
	 * the contents of orig_path to the right. Move strlen + 1 bytes to
	 * include the terminating '\0'. Move to strlen + 1 bytes to reserve
	 * space for the slash.
	 */
	memmove(orig_path+strlen(component)+1, orig_path, strlen(orig_path)+1);
	/* Copy component into canon_path */
	memmove(orig_path, component, strlen(component));
	/* Put slash into place */
	orig_path[strlen(component)] = '/';

	/* Store parent_dir result, and continue the loop once more */
	unlock_vnode(dir_vp);
	unlock_vmnt(dir_vmp);
	put_vnode(dir_vp);
	dir_vp = parent_dir;
  }

  unlock_vnode(dir_vp);
  unlock_vmnt(parent_vmp);

  put_vnode(dir_vp);

  /* add the leading slash */
  if (strlen(orig_path) >= PATH_MAX) return(ENAMETOOLONG);
  memmove(orig_path+1, orig_path, strlen(orig_path));
  orig_path[0] = '/';

  return(OK);
}

/*===========================================================================*
 *				check_perms				     *
 *===========================================================================*/
PRIVATE int check_perms(ep, io_gr, pathlen)
endpoint_t ep;
cp_grant_id_t io_gr;
size_t pathlen;
{
  int r, slot;
  struct vnode *vp;
  struct vmnt *vmp;
  struct fproc *rfp;
  char canon_path[PATH_MAX];
  struct lookup resolve;

  if (isokendpt(ep, &slot) != OK) return(EINVAL);
  if (pathlen < UNIX_PATH_MAX || pathlen >= PATH_MAX) return(EINVAL);

  rfp = &(fproc[slot]);
  r = sys_safecopyfrom(PFS_PROC_NR, io_gr, (vir_bytes) 0,
				(vir_bytes) canon_path, pathlen, D);
  if (r != OK) return(r);
  canon_path[pathlen] = '\0';

  /* Turn path into canonical path to the socket file */
  if ((r = canonical_path(canon_path, rfp)) != OK)
	return(r);

  if (strlen(canon_path) >= pathlen) return(ENAMETOOLONG);

  /* copy canon_path back to PFS */
  r = sys_safecopyto(PFS_PROC_NR, (cp_grant_id_t) io_gr, (vir_bytes) 0,
				(vir_bytes) canon_path, pathlen, D);
  if (r != OK) return(r);

  /* Now do permissions checking */
  lookup_init(&resolve, canon_path, PATH_NOFLAGS, &vmp, &vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;
  if ((vp = eat_path(&resolve, rfp)) == NULL) return(err_code);

  /* check permissions */
  r = forbidden(vp, (R_BIT | W_BIT));

  unlock_vnode(vp);
  unlock_vmnt(vmp);

  put_vnode(vp);
  return(r);
}

/*===========================================================================*
 *				do_check_perms				     *
 *===========================================================================*/
PUBLIC int do_check_perms(void)
{
  return check_perms(m_in.USER_ENDPT, (cp_grant_id_t) m_in.IO_GRANT,
		     (size_t) m_in.COUNT);
}
