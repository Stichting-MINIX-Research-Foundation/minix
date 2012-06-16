#include "fs.h"

#include <stdlib.h>
#include <assert.h>

#include "puffs.h"
#include "puffs_priv.h"

#define SAME 1000


/*===========================================================================*
 *                              fs_ftrunc                                    *
 *===========================================================================*/
int fs_ftrunc(void)
{
  int r;
  struct puffs_node *pn;
  off_t start, end;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if ((pn = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
          return(EINVAL);

  start = fs_m_in.REQ_TRC_START_LO;
  end = fs_m_in.REQ_TRC_END_LO;

  if (end == 0) {
	struct vattr va;

	if (pn->pn_va.va_size == start)
		return(OK);

	if (global_pu->pu_ops.puffs_node_setattr == NULL)
		return(EINVAL);
	
	puffs_vattr_null(&va);
	va.va_size = start;

	r = global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr);
	if (r) return(EINVAL);
  } else {
	/* XXX zerofill the given region. Can we make a hole? */
	off_t bytes_left = end - start;
	char* rw_buf;

	if (global_pu->pu_ops.puffs_node_write == NULL)
		return(EINVAL);

	/* XXX split into chunks? */
	rw_buf = malloc(bytes_left);
	if (!rw_buf)
		panic("fs_ftrunc: failed to allocated memory\n");
	memset(rw_buf, 0, bytes_left);

	r = global_pu->pu_ops.puffs_node_write(global_pu, pn, (uint8_t *)rw_buf,
					start, (size_t *) &bytes_left, pcr, 0);
	free(rw_buf);
	if (r) return(EINVAL);
  }

  update_times(pn, CTIME | MTIME, 0);

  return(r);
}


/*===========================================================================*
 *                              fs_link                                      *
 *===========================================================================*/
int fs_link()
{
/* Perform the link(name1, name2) system call. */

  register int r;
  char string[NAME_MAX + 1];
  phys_bytes len;
  struct puffs_node *pn, *pn_dir, *new_pn;
  time_t cur_time;
  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};

  if (global_pu->pu_ops.puffs_node_link == NULL)
  	return(OK);

  /* Copy the link name's last component */
  len = fs_m_in.REQ_PATH_LEN;
  if (len > NAME_MAX + 1)
        return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_GRANT, 0,
                       (vir_bytes) string, (size_t) len);
  if (r != OK) return(r);
  NUL(string, len, sizeof(string));

  if ((pn = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  /* Check to see if the file has maximum number of links already. */
  if (pn->pn_va.va_nlink >= LINK_MAX)
	return(EMLINK);

  /* Only super_user may link to directories. */
  if ((pn->pn_va.va_mode & I_TYPE) == I_DIRECTORY && caller_uid != SU_UID)
	return(EPERM);

  if ((pn_dir = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_DIR_INO)) == NULL)
        return(EINVAL);

  if (pn_dir->pn_va.va_nlink == NO_LINK) {
	/* Dir does not actually exist */
        return(ENOENT);
  }

  /* If 'name2' exists in full (even if no space) set 'r' to error. */
  if ((new_pn = advance(pn_dir, string, IGN_PERM)) == NULL) {
        r = err_code;
        if (r == ENOENT) r = OK;
  } else {
        r = EEXIST;
  }

  if (r != OK) return(r);

  /* Try to link. */
  pcn.pcn_namelen = strlen(string);
  assert(pcn.pcn_namelen <= MAXPATHLEN);
  strcpy(pcn.pcn_name, string);

  if (buildpath) {
	if (puffs_path_pcnbuild(global_pu, &pcn, pn_dir) != 0)
		return(EINVAL);
  }

  if (global_pu->pu_ops.puffs_node_link(global_pu, pn_dir, pn, &pcn) != 0)
	r = EINVAL;

  if (buildpath)
	global_pu->pu_pathfree(global_pu, &pcn.pcn_po_full);
  
  if (r != OK) return(EINVAL);

  cur_time = clock_time();
  update_times(pn, CTIME, cur_time);
  update_times(pn_dir, MTIME | CTIME, cur_time);

  return(OK);
}


/*===========================================================================*
 *                             fs_rdlink                                     *
 *===========================================================================*/
int fs_rdlink()
{
  register int r;              /* return value */
  size_t copylen;
  struct puffs_node *pn;
  char path[PATH_MAX];
  PUFFS_MAKECRED(pcr, &global_kcred);

  copylen = fs_m_in.REQ_MEM_SIZE < UMAX_FILE_POS ?
		fs_m_in.REQ_MEM_SIZE : UMAX_FILE_POS;
  
  assert(copylen <= PATH_MAX);

  if ((pn = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  if (!S_ISLNK(pn->pn_va.va_mode))
	return(EACCES);

  if (global_pu->pu_ops.puffs_node_readlink == NULL)
	return(EINVAL);

  r = global_pu->pu_ops.puffs_node_readlink(global_pu, pn, pcr, path,
						&copylen);
  if (r != OK) {
	if (r > 0) r = -r;
	return(r);
  }

  r = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_GRANT,
		  (vir_bytes) 0, (vir_bytes) path, (size_t) copylen);
  if (r == OK)
	  fs_m_out.RES_NBYTES = copylen;

  return(r);
}


/*===========================================================================*
 *                              fs_rename                                    *
 *===========================================================================*/
int fs_rename()
{
/* Perform the rename(name1, name2) system call. */
  struct puffs_node *old_dirp, *old_ip;      /* ptrs to old dir, file pnodes */
  struct puffs_node *new_dirp, *new_ip;      /* ptrs to new dir, file pnodes */
  struct puffs_kcn pkcnp_src;
  PUFFS_MAKECRED(pcr_src, &global_kcred);
  struct puffs_cn pcn_src = {&pkcnp_src, (struct puffs_cred *) __UNCONST(pcr_src), {0,0,0}};
  struct puffs_kcn pkcnp_dest;
  PUFFS_MAKECRED(pcr_dest, &global_kcred);
  struct puffs_cn pcn_targ = {&pkcnp_dest, (struct puffs_cred *) __UNCONST(pcr_dest), {0,0,0}};
  int r = OK;                           /* error flag; initially no error */
  int odir, ndir;                       /* TRUE iff {old|new} file is dir */
  int same_pdir;                        /* TRUE iff parent dirs are the same */
  phys_bytes len;
  time_t cur_time;

  if (global_pu->pu_ops.puffs_node_rename == NULL)
	return(EINVAL);

  /* Copy the last component of the old name */
  len = fs_m_in.REQ_REN_LEN_OLD; /* including trailing '\0' */
  if (len > NAME_MAX + 1)
        return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_REN_GRANT_OLD,
                (vir_bytes) 0, (vir_bytes) pcn_src.pcn_name, (size_t) len);
  if (r != OK) return(r);
  NUL(pcn_src.pcn_name, len, sizeof(pcn_src.pcn_name));
  pcn_src.pcn_namelen = len - 1;

  /* Copy the last component of the new name */
  len = fs_m_in.REQ_REN_LEN_NEW; /* including trailing '\0' */
  if (len > NAME_MAX + 1)
        return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_REN_GRANT_NEW,
                (vir_bytes) 0, (vir_bytes) pcn_targ.pcn_name, (size_t) len);
  if (r != OK) return(r);
  NUL(pcn_targ.pcn_name, len, sizeof(pcn_targ.pcn_name));
  pcn_targ.pcn_namelen = len - 1;

  /* Get old dir pnode */
  if ((old_dirp = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_REN_OLD_DIR))
	  == NULL)
        return(ENOENT);

  old_ip = advance(old_dirp, pcn_src.pcn_name, IGN_PERM);
  if (!old_ip) {
	return(ENOENT);
  }
  r = err_code;

  if (r == EENTERMOUNT || r == ELEAVEMOUNT) {
        old_ip = NULL;
        if (r == EENTERMOUNT) r = EXDEV;        /* should this fail at all? */
        else if (r == ELEAVEMOUNT) r = EINVAL;  /* rename on dot-dot */
  }

  /* Get new dir pnode */
  if ((new_dirp = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_REN_NEW_DIR))
	  == NULL) {
        r = ENOENT;
  } else {
        if (new_dirp->pn_va.va_nlink == NO_LINK) {
		/* Dir does not actually exist */
                return(ENOENT);
        }
  }

  /* not required to exist */
  new_ip = advance(new_dirp, pcn_targ.pcn_name, IGN_PERM);

  /* However, if the check failed because the file does exist, don't continue.
   * Note that ELEAVEMOUNT is covered by the dot-dot check later. */
  if (err_code == EENTERMOUNT) {
        new_ip = NULL;
        r = EBUSY;
  }

  if (old_ip != NULL) {
	/* TRUE iff dir */
	odir = ((old_ip->pn_va.va_mode & I_TYPE) == I_DIRECTORY);
  } else {
	odir = FALSE;
  }

  if (r != OK) return(r);

  /* Check for a variety of possible errors. */
  same_pdir = (old_dirp == new_dirp);

  /* The old or new name must not be . or .. */
  if (strcmp(pcn_src.pcn_name, ".") == 0 ||
      strcmp(pcn_src.pcn_name, "..") == 0 ||
      strcmp(pcn_targ.pcn_name, ".") == 0 ||
      strcmp(pcn_targ.pcn_name, "..") == 0) {
	r = EINVAL;
  }

  /* Some tests apply only if the new path exists. */
  if (new_ip == NULL) {
	if (odir && (new_dirp->pn_va.va_nlink >= SHRT_MAX ||
		     new_dirp->pn_va.va_nlink >= LINK_MAX) &&
	    !same_pdir && r == OK) {
		r = EMLINK;
	}
  } else {
	if (old_ip == new_ip) r = SAME; /* old=new */

	/* dir ? */
	ndir = ((new_ip->pn_va.va_mode & I_TYPE) == I_DIRECTORY);
	if (odir == TRUE && ndir == FALSE) r = ENOTDIR;
	if (odir == FALSE && ndir == TRUE) r = EISDIR;
  }

  if (r == SAME) {
	r = OK;
	goto rename_out;
  }

  if (r != OK) return(r);

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
   * 3. Something (doubtfully) else depending on the FS.
   */

  if (buildpath) {
	pcn_src.pcn_po_full = old_ip->pn_po;

	if (puffs_path_pcnbuild(global_pu, &pcn_targ, new_dirp) != 0)
		return(EINVAL);
  }

  r = global_pu->pu_ops.puffs_node_rename(global_pu, old_dirp, old_ip, &pcn_src,
						new_dirp, new_ip, &pcn_targ);
  if (r > 0) r = -r;

  if (buildpath) {
	if (r) {
		global_pu->pu_pathfree(global_pu, &pcn_targ.pcn_po_full);
	} else {
		struct puffs_pathinfo pi;
		struct puffs_pathobj po_old;

		/* handle this node */
		po_old = old_ip->pn_po;
		old_ip->pn_po = pcn_targ.pcn_po_full;

		if (old_ip->pn_va.va_type != VDIR) {
			global_pu->pu_pathfree(global_pu, &po_old);
			return(OK);
		}

		/* handle all child nodes for DIRs */
		pi.pi_old = &pcn_src.pcn_po_full;
		pi.pi_new = &pcn_targ.pcn_po_full;

		PU_LOCK();
		if (puffs_pn_nodewalk(global_pu, puffs_path_prefixadj, &pi)
				!= NULL) {
			/* Actually nomem */
			return(EINVAL);
		}
		PU_UNLOCK();
		global_pu->pu_pathfree(global_pu, &po_old);
	}
  }

rename_out: 
  cur_time = clock_time();
  update_times(old_dirp, MTIME | CTIME, cur_time);
  update_times(new_dirp, MTIME | CTIME, cur_time);

  /* XXX see release_node comment in fs_unlink */
  if (new_ip && new_ip->pn_count == 0) {
	release_node(global_pu, new_ip);
  }

  return(r);
}

static int remove_dir(struct puffs_node *pn_dir, struct puffs_node *pn,
	struct puffs_cn *pcn);
static int unlink_file(struct puffs_node *dirp, struct puffs_node *pn,
	struct puffs_cn *pcn);

/*===========================================================================*
 *                              fs_unlink                                    *
 *===========================================================================*/
int fs_unlink()
{
/* Perform the unlink(name) or rmdir(name) system call. The code for these two
 * is almost the same.  They differ only in some condition testing.  Unlink()
 * may be used by the superuser to do dangerous things; rmdir() may not.
 */
  int r;
  struct puffs_node *pn, *pn_dir;
  time_t cur_time;
  struct puffs_kcn pkcnp;
  struct puffs_cn pcn = {&pkcnp, 0, {0,0,0}};
  PUFFS_KCREDTOCRED(pcn.pcn_cred, &global_kcred);
  int len;

  /* Copy the last component */
  len = fs_m_in.REQ_PATH_LEN;
  pcn.pcn_namelen = len - 1;
  if (pcn.pcn_namelen > NAME_MAX)
        return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_GRANT,
		       (vir_bytes) 0, (vir_bytes) pcn.pcn_name,
		       (size_t) len);
  if (r != OK) return (r);
  NUL(pcn.pcn_name, len, sizeof(pcn.pcn_name));

  if ((pn_dir = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  /* The last directory exists. Does the file also exist? */
  pn = advance(pn_dir, pcn.pcn_name, IGN_PERM);
  r = err_code;

  /* If error, return pnode. */
  if (r != OK) {
        /* Mount point? */
        if (r == EENTERMOUNT || r == ELEAVEMOUNT) {
                r = EBUSY;
        }
        return(r);
  }

  /* Now test if the call is allowed, separately for unlink() and rmdir(). */
  if (fs_m_in.m_type == REQ_UNLINK) {
	/* Only the su may unlink directories, but the su can unlink any dir */
	if ((pn->pn_va.va_mode & I_TYPE) == I_DIRECTORY)
		r = EPERM;
	if (r == OK)
		r = unlink_file(pn_dir, pn, &pcn);
  } else {
	r = remove_dir(pn_dir, pn, &pcn); /* call is RMDIR */
  }

  if (pn->pn_va.va_nlink != 0) {
	cur_time = clock_time();
	update_times(pn, CTIME, cur_time);
	update_times(pn_dir, MTIME | CTIME, cur_time);
  }

  /* XXX Ideally, we should check pn->pn_flags & PUFFS_NODE_REMOVED, but
   * librefuse doesn't set it (neither manually or via puffs_pn_remove() ).
   * Thus we just check that "pn_count == 0". Otherwise release_node()
   * will be called in fs_put().
   */
  if (pn->pn_count == 0)
	release_node(global_pu, pn);

  return(r);
}


/*===========================================================================*
 *				remove_dir				     *
 *===========================================================================*/
static int remove_dir(pn_dir, pn, pcn)
struct puffs_node *pn_dir;		/* parent directory */
struct puffs_node *pn;			/* directory to be removed */
struct puffs_cn *pcn;			/* Name, creads of directory */
{
  /* A directory file has to be removed. Five conditions have to met:
   * 	- The file must be a directory
   *	- The directory must be empty (except for . and ..)
   *	- The final component of the path must not be . or ..
   *	- The directory must not be the root of a mounted file system (VFS)
   *	- The directory must not be anybody's root/working directory (VFS)
   */

  /* "." and ".." dentries can be stored in 28 bytes */
  #define EMPTY_DIR_DENTRIES_SIZE	28
  int r;
  char remove_dir_buf[EMPTY_DIR_DENTRIES_SIZE];
  struct dirent *dent = (struct dirent*) remove_dir_buf;
  int buf_left = EMPTY_DIR_DENTRIES_SIZE;
  off_t pos = 0;
  int eofflag = 0;

  if (global_pu->pu_ops.puffs_node_rmdir == NULL)
	return(EINVAL);

  if (!S_ISDIR(pn->pn_va.va_mode))
	return(ENOTDIR);

  /* Check if directory is empty */
  r = global_pu->pu_ops.puffs_node_readdir(global_pu, pn, dent, &pos,
			(size_t *)&buf_left, pcn->pcn_cred, &eofflag, 0, 0);
  if (r) return(EINVAL);
  if (!eofflag) return(ENOTEMPTY);

  if (strcmp(pcn->pcn_name, ".") == 0 || strcmp(pcn->pcn_name, "..") == 0)
	return(EINVAL);

  if (pn->pn_va.va_fileid == global_pu->pu_pn_root->pn_va.va_fileid)
	return(EBUSY); /* can't remove 'root' */
  
  if (buildpath) {
	r = puffs_path_pcnbuild(global_pu, pcn, pn_dir);
	if (r) return(EINVAL);
  }

  r = global_pu->pu_ops.puffs_node_rmdir(global_pu, pn_dir, pn, pcn);

  global_pu->pu_pathfree(global_pu, &pcn->pcn_po_full);

  if (r) return(EINVAL);

  return(OK);
}


/*===========================================================================*
 *				unlink_file				     *
 *===========================================================================*/
static int unlink_file(dirp, pn, pcn)
struct puffs_node *dirp;	/* parent directory of file */
struct puffs_node *pn;		/* pnode of file, may be NULL too. */
struct puffs_cn *pcn;		/* Name, creads of file */
{
/* Unlink 'file_name'; pn must be the pnode of 'file_name' */
  int	r;

  assert(pn != NULL);

  if (global_pu->pu_ops.puffs_node_remove == NULL)
  	return(EINVAL);

  if (S_ISDIR(pn->pn_va.va_mode))
	return(EINVAL);

  if (buildpath) {
	r = puffs_path_pcnbuild(global_pu, pcn, dirp);
	if (r)
		return(EINVAL);
  }

  r = global_pu->pu_ops.puffs_node_remove(global_pu, dirp, pn, pcn);

  global_pu->pu_pathfree(global_pu, &pcn->pcn_po_full);

  if (r) return(EINVAL);

  return(OK);
}
