/* lookup() is the main routine that controls the path name lookup. It
 * handles mountpoints and symbolic links. The actual lookup requests
 * are sent through the req_lookup wrapper function.
 */

#include "fs.h"
#include <string.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
#include <minix/vfsif.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include "vmnt.h"
#include "vnode.h"
#include "path.h"

/* Set to following define to 1 if you really want to use the POSIX definition
 * (IEEE Std 1003.1, 2004) of pathname resolution. POSIX requires pathnames
 * with a traling slash (and that do not entirely consist of slash characters)
 * to be treated as if a single dot is appended. This means that for example
 * mkdir("dir/", ...) and rmdir("dir/") will fail because the call tries to
 * create or remove the directory '.'. Historically, Unix systems just ignore
 * trailing slashes.
 */
#define DO_POSIX_PATHNAME_RES	0

static int lookup(struct vnode *dirp, struct lookup *resolve,
	node_details_t *node, struct fproc *rfp);

/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
struct vnode *
advance(struct vnode *dirp, struct lookup *resolve, struct fproc *rfp)
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
struct vnode *
eat_path(struct lookup *resolve, struct fproc *rfp)
{
/* Resolve path to a vnode. advance does the actual work. */
  struct vnode *start_dir;

  start_dir = (resolve->l_path[0] == '/' ? rfp->fp_rd : rfp->fp_wd);
  return advance(start_dir, resolve, rfp);
}

/*===========================================================================*
 *				last_dir				     *
 *===========================================================================*/
struct vnode *
last_dir(struct lookup *resolve, struct fproc *rfp)
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
  struct vnode *start_dir, *res_vp, *sym_vp, *sym_vp_l, *loop_start;
  struct vmnt *sym_vmp = NULL;
  int r, symloop = 0, ret_on_symlink = 0;
  struct lookup symlink;

  *resolve->l_vnode = NULL;
  *resolve->l_vmp = NULL;
  loop_start = NULL;
  sym_vp = NULL;

  ret_on_symlink = !!(resolve->l_flags & PATH_RET_SYMLINK);

  do {
	/* Is the path absolute or relative? Initialize 'start_dir'
	 * accordingly. Use loop_start in case we're looping.
	 */
	if (loop_start != NULL)
		start_dir = loop_start;
	else
		start_dir = (resolve->l_path[0] == '/' ? rfp->fp_rd:rfp->fp_wd);

	len = strlen(resolve->l_path);

	/* If path is empty, return ENOENT. */
	if (len == 0)	{
		err_code = ENOENT;
		res_vp = NULL;
		break;
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
		/* Just an entry in the current working directory. Prepend
		 * "./" in front of the path and resolve it.
		 */
		if (strlcpy(dir_entry, resolve->l_path, NAME_MAX+1) >= NAME_MAX + 1) {
			err_code = ENAMETOOLONG;
			res_vp = NULL;
			break;
		}
		dir_entry[NAME_MAX] = '\0';
		resolve->l_path[0] = '.';
		resolve->l_path[1] = '\0';
	} else if (cp[1] == '\0') {
		/* Path ends in a slash. The directory entry is '.' */
		strlcpy(dir_entry, ".", NAME_MAX+1);
	} else {
		/* A path name for the directory and a directory entry */
		if (strlcpy(dir_entry, cp+1, NAME_MAX+1) >= NAME_MAX + 1) {
			err_code = ENAMETOOLONG;
			res_vp = NULL;
			break;
		}
		cp[1] = '\0';
		dir_entry[NAME_MAX] = '\0';
	}

	/* Remove trailing slashes */
	while (cp > resolve->l_path && cp[0] == '/') {
		cp[0]= '\0';
		cp--;
	}

	/* Resolve up to and including the last directory of the path. Turn off
	 * PATH_RET_SYMLINK, because we do want to follow the symlink in this
	 * case. That is, the flag is meant for the actual filename of the path,
	 * not the last directory.
	 */
	resolve->l_flags &= ~PATH_RET_SYMLINK;
	if ((res_vp = advance(start_dir, resolve, rfp)) == NULL) {
		break;
	}

	/* If the directory entry is not a symlink we're done now. If it is a
	 * symlink, then we're not at the last directory, yet. */

	/* Copy the directory entry back to user_fullpath */
	strlcpy(resolve->l_path, dir_entry, NAME_MAX + 1);

	/* Look up the directory entry, but do not follow the symlink when it
	 * is one. Note: depending on the previous advance, we might not be
	 * able to lock the resulting vnode. For example, when we look up "./."
	 * and request a VNODE_WRITE lock on the result, then the previous
	 * advance has "./" locked. The next advance to "." will try to lock
	 * the same vnode with a VNODE_READ lock, and fail. When that happens,
	 * sym_vp_l will be NULL and we must not unlock the vnode. If we would
	 * unlock, we actually unlock the vnode locked by the previous advance.
	 */
	lookup_init(&symlink, resolve->l_path,
		    resolve->l_flags|PATH_RET_SYMLINK, &sym_vmp, &sym_vp_l);
	symlink.l_vmnt_lock = VMNT_READ;
	symlink.l_vnode_lock = VNODE_READ;
	sym_vp = advance(res_vp, &symlink, rfp);

	if (sym_vp == NULL) break;

	if (S_ISLNK(sym_vp->v_mode)) {
		/* Last component is a symlink, but if we've been asked to not
		 * resolve it, return now.
		 */
		if (ret_on_symlink) {
			break;
		}

		r = req_rdlink(sym_vp->v_fs_e, sym_vp->v_inode_nr, NONE,
				(vir_bytes) resolve->l_path, PATH_MAX - 1, 1);

		if (r < 0) {
			/* Failed to read link */
			err_code = r;
			unlock_vnode(res_vp);
			unlock_vmnt(*resolve->l_vmp);
			put_vnode(res_vp);
			*resolve->l_vmp = NULL;
			*resolve->l_vnode = NULL;
			res_vp = NULL;
			break;
		}
		resolve->l_path[r] = '\0';

		if (strrchr(resolve->l_path, '/') != NULL) {
			if (sym_vp_l != NULL)
				unlock_vnode(sym_vp);
			unlock_vmnt(*resolve->l_vmp);
			if (sym_vmp != NULL)
				unlock_vmnt(sym_vmp);
			*resolve->l_vmp = NULL;
			put_vnode(sym_vp);
			sym_vp = NULL;

			symloop++;

			/* Relative symlinks are relative to res_vp, not cwd */
			if (resolve->l_path[0] != '/') {
				loop_start = res_vp;
			} else {
				/* Absolute symlink, forget about res_vp */
				unlock_vnode(res_vp);
				put_vnode(res_vp);
			}

			continue;
		}
	} else {
		symloop = 0;	/* Not a symlink, so restart counting */

		/* If we're crossing a mount point, return root node of mount
		 * point on which the file resides. That's the 'real' last
		 * dir that holds the file we're looking for.
		 */
		if (sym_vp->v_fs_e != res_vp->v_fs_e) {
			assert(sym_vmp != NULL);

			/* Unlock final file, it might have wrong lock types */
			if (sym_vp_l != NULL)
				unlock_vnode(sym_vp);
			unlock_vmnt(sym_vmp);
			put_vnode(sym_vp);
			sym_vp = NULL;

			/* Also unlock and release erroneous result */
			unlock_vnode(*resolve->l_vnode);
			unlock_vmnt(*resolve->l_vmp);
			put_vnode(res_vp);

			/* Relock vmnt and vnode with correct lock types */
			lock_vmnt(sym_vmp, resolve->l_vmnt_lock);
			lock_vnode(sym_vmp->m_root_node, resolve->l_vnode_lock);
			res_vp = sym_vmp->m_root_node;
			dup_vnode(res_vp);
			*resolve->l_vnode = res_vp;
			*resolve->l_vmp = sym_vmp;

			/* We've effectively resolved the final component, so
			 * change it to current directory to prevent future
			 * 'advances' of returning erroneous results.
			 */
			strlcpy(dir_entry, ".", NAME_MAX+1);
		}
	}
	break;
  } while (symloop < _POSIX_SYMLOOP_MAX);

  if (symloop >= _POSIX_SYMLOOP_MAX) {
	err_code = ELOOP;
	res_vp = NULL;
  }

  if (sym_vp != NULL) {
	if (sym_vp_l != NULL) {
		unlock_vnode(sym_vp);
	}
	if (sym_vmp != NULL) {
		unlock_vmnt(sym_vmp);
	}
	put_vnode(sym_vp);
  }

  if (loop_start != NULL) {
	unlock_vnode(loop_start);
	put_vnode(loop_start);
  }

  /* Copy the directory entry back to user_fullpath */
  strlcpy(resolve->l_path, dir_entry, NAME_MAX + 1);

  /* Turn PATH_RET_SYMLINK flag back on if it was on */
  if (ret_on_symlink) resolve->l_flags |= PATH_RET_SYMLINK;

  return(res_vp);
}

/*===========================================================================*
 *				lookup					     *
 *===========================================================================*/
static int
lookup(struct vnode *start_node, struct lookup *resolve, node_details_t *result_node, struct fproc *rfp)
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
  tll_access_t mnt_lock_type;

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

  if (vmpres == NULL) return(EIO);	/* mountpoint vanished? */

  /* Is the process' root directory on the same partition?,
   * if so, set the chroot directory too. */
  if (rfp->fp_rd->v_dev == start_node->v_dev)
	root_ino = rfp->fp_rd->v_inode_nr;
  else
	root_ino = 0;

  /* Set user and group ids according to the system call */
  uid = (job_call_nr == VFS_ACCESS ? rfp->fp_realuid : rfp->fp_effuid);
  gid = (job_call_nr == VFS_ACCESS ? rfp->fp_realgid : rfp->fp_effgid);

  symloop = 0;	/* Number of symlinks seen so far */

  /* Lock vmnt */
  if (resolve->l_vmnt_lock == VMNT_READ)
	mnt_lock_type = VMNT_WRITE;
  else
	mnt_lock_type = resolve->l_vmnt_lock;

  if ((r = lock_vmnt(vmpres, mnt_lock_type)) != OK) {
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
	if (symloop > _POSIX_SYMLOOP_MAX) {
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
	if (vmpres == NULL) return(EIO);	/* mount point vanished? */
	if ((r = lock_vmnt(vmpres, mnt_lock_type)) != OK) {
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

  if (*(resolve->l_vmp) != NULL && resolve->l_vmnt_lock != mnt_lock_type) {
	/* downgrade VMNT_WRITE to VMNT_READ */
	downgrade_vmnt_lock(*(resolve->l_vmp));
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
void
lookup_init(struct lookup *resolve, char *path, int flags, struct vmnt **vmp, struct vnode **vp)
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
int
get_name(struct vnode *dirp, struct vnode *entry, char ename[NAME_MAX + 1])
{
#define DIR_ENTRIES 8
#define DIR_ENTRY_SIZE (sizeof(struct dirent) + NAME_MAX)
  off_t pos, new_pos;
  int r, consumed, totalbytes, name_len;
  char buf[DIR_ENTRY_SIZE * DIR_ENTRIES];
  struct dirent *cur;

  pos = 0;

  if (!S_ISDIR(dirp->v_mode)) return(EBADF);

  do {
	r = req_getdents(dirp->v_fs_e, dirp->v_inode_nr, pos, (vir_bytes)buf,
		sizeof(buf), &new_pos, 1);

	if (r == 0) {
		return(ENOENT); /* end of entries -- matching inode !found */
	} else if (r < 0) {
		return(r); /* error */
	}

	consumed = 0; /* bytes consumed */
	totalbytes = r; /* number of bytes to consume */

	do {
		cur = (struct dirent *) (buf + consumed);
		name_len = cur->d_reclen - offsetof(struct dirent, d_name) - 1;

		if(cur->d_name + name_len+1 > &buf[sizeof(buf)])
			return(EINVAL);	/* Rubbish in dir entry */
		if (entry->v_inode_nr == cur->d_fileno) {
			/* found the entry we were looking for */
			int copylen = MIN(name_len + 1, NAME_MAX + 1);
			if (strlcpy(ename, cur->d_name, copylen) >= copylen) {
				return(ENAMETOOLONG);
			}
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
int
canonical_path(char orig_path[PATH_MAX], struct fproc *rfp)
{
/* Find canonical path of a given path */
  int len = 0;
  int r, symloop = 0;
  struct vnode *dir_vp, *parent_dir;
  struct vmnt *dir_vmp, *parent_vmp;
  char component[NAME_MAX+1];	/* NAME_MAX does /not/ include '\0' */
  char temp_path[PATH_MAX];
  struct lookup resolve;

  parent_dir = dir_vp = NULL;
  parent_vmp = dir_vmp = NULL;
  strlcpy(temp_path, orig_path, PATH_MAX);
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
	strlcpy(orig_path, temp_path, NAME_MAX+1);	/* Store file name */

	/* If we're just crossing a mount point, our name has changed to '.' */
	if (!strcmp(orig_path, ".")) orig_path[0] = '\0';

	/* check if the file is a symlink, if so resolve it */
	r = rdlink_direct(orig_path, temp_path, rfp);

	if (r <= 0)
		break;

	/* encountered a symlink -- loop again */
	strlcpy(orig_path, temp_path, PATH_MAX);
	symloop++;
  } while (symloop < _POSIX_SYMLOOP_MAX);

  if (symloop >= _POSIX_SYMLOOP_MAX) {
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

	strlcpy(temp_path, "..", NAME_MAX+1);

	/* check if we're at the root node of the file system */
	if (dir_vp->v_vmnt->m_root_node == dir_vp) {
		if (dir_vp->v_vmnt->m_mounted_on == NULL) {
			/* Bail out, we can't go any higher */
			break;
		}
		unlock_vnode(dir_vp);
		unlock_vmnt(dir_vmp);
		put_vnode(dir_vp);
		dir_vp = dir_vp->v_vmnt->m_mounted_on;
		dir_vmp = dir_vp->v_vmnt;
		if (lock_vmnt(dir_vmp, VMNT_READ) != OK)
			panic("failed to lock vmnt");
		if (lock_vnode(dir_vp, VNODE_READ) != OK)
			panic("failed to lock vnode");
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
	if ((r = get_name(parent_dir, dir_vp, component)) != OK) {
		unlock_vnode(parent_dir);
		unlock_vmnt(parent_vmp);
		unlock_vnode(dir_vp);
		unlock_vmnt(dir_vmp);
		put_vnode(parent_dir);
		put_vnode(dir_vp);
		return(r);
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
	dir_vmp = parent_vmp;
	parent_vmp = NULL;
  }

  unlock_vmnt(dir_vmp);
  unlock_vnode(dir_vp);
  put_vnode(dir_vp);

  /* add the leading slash */
  len = strlen(orig_path);
  if (strlen(orig_path) >= PATH_MAX) return(ENAMETOOLONG);
  memmove(orig_path+1, orig_path, len + 1 /* include terminating nul */);
  orig_path[0] = '/';

  /* remove trailing slash if there is any */
  if (len > 1 && orig_path[len] == '/') orig_path[len] = '\0';

  return(OK);
}

/*===========================================================================*
 *				do_socketpath				     *
 *===========================================================================*/
int do_socketpath(void)
{
/*
 * Perform a path action on an on-disk socket file.  This call may be performed
 * by the UDS service only.  The action is always on behalf of a user process
 * that is currently making a socket call to the UDS service, and thus, VFS may
 * rely on the fact that the user process is blocked.  TODO: there should be
 * checks in place to prevent (even accidental) abuse of this function, though.
 */
  int r, what, slot;
  endpoint_t ep;
  cp_grant_id_t io_gr;
  size_t pathlen;
  struct vnode *dirp, *vp;
  struct vmnt *vmp, *vmp2;
  struct fproc *rfp;
  char path[PATH_MAX];
  struct lookup resolve, resolve2;
  mode_t bits;

  /* This should be replaced by an ACL check. */
  if (!super_user) return EPERM;

  ep = job_m_in.m_lsys_vfs_socketpath.endpt;
  io_gr = job_m_in.m_lsys_vfs_socketpath.grant;
  pathlen = job_m_in.m_lsys_vfs_socketpath.count;
  what = job_m_in.m_lsys_vfs_socketpath.what;

  if (isokendpt(ep, &slot) != OK) return(EINVAL);
  rfp = &fproc[slot];

  /* Copy in the path name, which must not be empty.  It is typically not null
   * terminated.
   */
  if (pathlen < 1 || pathlen >= sizeof(path)) return(EINVAL);
  r = sys_safecopyfrom(who_e, io_gr, (vir_bytes)0, (vir_bytes)path, pathlen);
  if (r != OK) return(r);
  path[pathlen] = '\0';

  /* Now perform the requested action.  For the SPATH_CHECK action, a socket
   * file is expected to exist already, and we should check whether the given
   * user process has access to it.  For the SPATH_CREATE action, no file is
   * expected to exist yet, and a socket file should be created on behalf of
   * the user process.  In both cases, on success, return the socket file's
   * device and inode numbers to the caller.
   *
   * Since the above canonicalization releases all locks once done, we need to
   * recheck absolutely everything now.  TODO: do not release locks in between.
   */
  switch (what) {
  case SPATH_CHECK:
	lookup_init(&resolve, path, PATH_NOFLAGS, &vmp, &vp);
	resolve.l_vmnt_lock = VMNT_READ;
	resolve.l_vnode_lock = VNODE_READ;
	if ((vp = eat_path(&resolve, rfp)) == NULL) return(err_code);

	/* Check file type and permissions. */
	if (!S_ISSOCK(vp->v_mode))
		r = ENOTSOCK; /* not in POSIX spec; this is what NetBSD does */
	else
		r = forbidden(rfp, vp, R_BIT | W_BIT);

	if (r == OK) {
		job_m_out.m_vfs_lsys_socketpath.device = vp->v_dev;
		job_m_out.m_vfs_lsys_socketpath.inode = vp->v_inode_nr;
	}

	unlock_vnode(vp);
	unlock_vmnt(vmp);
	put_vnode(vp);
	break;

  case SPATH_CREATE:
	/* This is effectively simulating a mknod(2) call by the user process,
	 * including the application of its umask to the file permissions.
	 */
	lookup_init(&resolve, path, PATH_RET_SYMLINK, &vmp, &dirp);
	resolve.l_vmnt_lock = VMNT_WRITE;
	resolve.l_vnode_lock = VNODE_WRITE;

	if ((dirp = last_dir(&resolve, rfp)) == NULL) return(err_code);

	bits = S_IFSOCK | (ACCESSPERMS & rfp->fp_umask);

	if (!S_ISDIR(dirp->v_mode))
		r = ENOTDIR;
	else if ((r = forbidden(rfp, dirp, W_BIT | X_BIT)) == OK) {
		r = req_mknod(dirp->v_fs_e, dirp->v_inode_nr, path,
		    rfp->fp_effuid, rfp->fp_effgid, bits, NO_DEV);
		if (r == OK) {
			/* Now we need to find out the device and inode number
			 * of the socket file we just created.  The vmnt lock
			 * should prevent any trouble here.
			 */
			lookup_init(&resolve2, resolve.l_path,
			    PATH_RET_SYMLINK, &vmp2, &vp);
			resolve2.l_vmnt_lock = VMNT_READ;
			resolve2.l_vnode_lock = VNODE_READ;
			vp = advance(dirp, &resolve2, rfp);
			assert(vmp2 == NULL);
			if (vp != NULL) {
				job_m_out.m_vfs_lsys_socketpath.device =
				    vp->v_dev;
				job_m_out.m_vfs_lsys_socketpath.inode =
				    vp->v_inode_nr;
				unlock_vnode(vp);
				put_vnode(vp);
			} else {
				/* Huh.  This should never happen.  If it does,
				 * we assume the socket file has somehow been
				 * lost, so we do not try to unlink it.
				 */
				printf("VFS: socketpath did not find created "
				    "node at %s (%d)\n", path, err_code);
				r = err_code;
			}
		} else if (r == EEXIST)
			r = EADDRINUSE;
	}

	unlock_vnode(dirp);
	unlock_vmnt(vmp);
	put_vnode(dirp);
	break;

  default:
	r = ENOSYS;
  }

  return(r);
}
