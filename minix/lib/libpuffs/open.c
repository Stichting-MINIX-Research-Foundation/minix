/* Created (MFS based):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"

/*===========================================================================*
 *				fs_create				     *
 *===========================================================================*/
int fs_create(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid,
	struct fsdriver_node *node)
{
  int r;
  struct puffs_node *pn_dir;
  struct puffs_node *pn;
  struct puffs_newinfo pni;
  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};
  struct vattr va;
  struct timespec cur_time;

  if (global_pu->pu_ops.puffs_node_create == NULL) {
	lpuffs_debug("No puffs_node_create");
	return(ENFILE);
  }

  /* Copy the last component (i.e., file name) */
  pcn.pcn_namelen = strlen(name);
  assert(pcn.pcn_namelen <= NAME_MAX);
  strcpy(pcn.pcn_name, name);

  /* Get last directory pnode (i.e., directory that will hold the new pnode) */
  if ((pn_dir = puffs_pn_nodewalk(global_pu, find_inode_cb, &dir_nr)) == NULL)
	return(ENOENT);

  memset(&pni, 0, sizeof(pni));
  pni.pni_cookie = (void** )&pn;

  (void)clock_time(&cur_time);

  memset(&va, 0, sizeof(va));
  va.va_type = VREG;
  va.va_mode = mode;
  va.va_uid = uid;
  va.va_gid = gid;
  va.va_atime = va.va_mtime = va.va_ctime = cur_time;

  if (buildpath) {
	r = puffs_path_pcnbuild(global_pu, &pcn, pn_dir);
	if (r) {
		lpuffs_debug("pathbuild error\n");
		return(ENOENT);
	}
  }

  r = global_pu->pu_ops.puffs_node_create(global_pu, pn_dir, &pni, &pcn, &va);
  if (buildpath) {
	if (r) {
		global_pu->pu_pathfree(global_pu, &pcn.pcn_po_full);
	} else {
		struct puffs_node *_pn;

		_pn = PU_CMAP(global_pu, pn);
		_pn->pn_po = pcn.pcn_po_full;
	}
  }

  if (r != OK) {
	if (r > 0) r = -r;
	return(r);
  }

  /* Open pnode */
  pn->pn_count++;

  update_timens(pn_dir, MTIME | CTIME, &cur_time);

  /* Reply message */
  node->fn_ino_nr = pn->pn_va.va_fileid;
  node->fn_mode = pn->pn_va.va_mode;
  node->fn_size = pn->pn_va.va_size;
  node->fn_uid = pn->pn_va.va_uid;
  node->fn_gid = pn->pn_va.va_gid;
  node->fn_dev = NO_DEV;

  return(OK);
}


/*===========================================================================*
 *				fs_mknod				     *
 *===========================================================================*/
int fs_mknod(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid,
	dev_t dev)
{
  int r;
  struct puffs_node *pn_dir;
  struct puffs_node *pn;
  struct puffs_newinfo pni;
  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};
  struct vattr va;
  struct timespec cur_time;

  if (global_pu->pu_ops.puffs_node_mknod == NULL) {
	lpuffs_debug("No puffs_node_mknod");
	return(ENFILE);
  }

  /* Copy the last component */
  pcn.pcn_namelen = strlen(name);
  assert(pcn.pcn_namelen <= NAME_MAX);
  strcpy(pcn.pcn_name, name);

  /* Get last directory pnode */
  if ((pn_dir = puffs_pn_nodewalk(global_pu, find_inode_cb, &dir_nr)) == NULL)
	return(ENOENT);

  memset(&pni, 0, sizeof(pni));
  pni.pni_cookie = (void** )&pn;

  (void)clock_time(&cur_time);

  memset(&va, 0, sizeof(va));
  va.va_type = VDIR;
  va.va_mode = mode;
  va.va_uid = uid;
  va.va_gid = gid;
  va.va_rdev = dev;
  va.va_atime = va.va_mtime = va.va_ctime = cur_time;

  if (buildpath) {
	if (puffs_path_pcnbuild(global_pu, &pcn, pn_dir) != 0) {
		lpuffs_debug("pathbuild error\n");
		return(ENOENT);
	}
  }

  r = global_pu->pu_ops.puffs_node_mknod(global_pu, pn_dir, &pni, &pcn, &va);
  if (buildpath) {
	if (r) {
		global_pu->pu_pathfree(global_pu, &pcn.pcn_po_full);
	} else {
		struct puffs_node *_pn;

		_pn = PU_CMAP(global_pu, pn);
		_pn->pn_po = pcn.pcn_po_full;
	  }
  }

  if (r != OK) {
	if (r > 0) r = -r;
	return(r);
  }

  update_timens(pn_dir, MTIME | CTIME, &cur_time);

  return(OK);
}


/*===========================================================================*
 *				fs_mkdir				     *
 *===========================================================================*/
int fs_mkdir(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid)
{
  int r;
  struct puffs_node *pn_dir;
  struct puffs_node *pn;
  struct puffs_newinfo pni;
  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};
  struct vattr va;
  struct timespec cur_time;

  if (global_pu->pu_ops.puffs_node_mkdir == NULL) {
	lpuffs_debug("No puffs_node_mkdir");
	return(ENFILE);
  }

  /* Copy the last component */
  pcn.pcn_namelen = strlen(name);
  assert(pcn.pcn_namelen <= NAME_MAX);
  strcpy(pcn.pcn_name, name);

  /* Get last directory pnode */
  if ((pn_dir = puffs_pn_nodewalk(global_pu, find_inode_cb, &dir_nr)) == NULL)
	return(ENOENT);

  (void)clock_time(&cur_time);

  memset(&pni, 0, sizeof(pni));
  pni.pni_cookie = (void** )&pn;

  memset(&va, 0, sizeof(va));
  va.va_type = VDIR;
  va.va_mode = mode;
  va.va_uid = uid;
  va.va_gid = gid;
  va.va_atime = va.va_mtime = va.va_ctime = cur_time;

  if (buildpath) {
	r = puffs_path_pcnbuild(global_pu, &pcn, pn_dir);
	if (r) {
		lpuffs_debug("pathbuild error\n");
		return(ENOENT);
	}
  }

  r = global_pu->pu_ops.puffs_node_mkdir(global_pu, pn_dir, &pni, &pcn, &va);
  if (buildpath) {
	if (r) {
		global_pu->pu_pathfree(global_pu, &pcn.pcn_po_full);
	} else {
		struct puffs_node *_pn;

		_pn = PU_CMAP(global_pu, pn);
		_pn->pn_po = pcn.pcn_po_full;
	}
  }

  if (r != OK) {
	if (r > 0) r = -r;
	return(r);
  }

  update_timens(pn_dir, MTIME | CTIME, &cur_time);

  return(OK);
}


/*===========================================================================*
 *                             fs_slink 				     *
 *===========================================================================*/
int fs_slink(ino_t dir_nr, char *name, uid_t uid, gid_t gid,
	struct fsdriver_data *data, size_t bytes)
{
  int r;
  struct pnode *pn;		/* pnode containing symbolic link */
  struct pnode *pn_dir;		/* directory containing link */
  char target[PATH_MAX + 1];	/* target path */
  struct puffs_newinfo pni;
  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};
  struct vattr va;
  struct timespec cur_time;

  /* Copy the link name's last component */
  pcn.pcn_namelen = strlen(name);
  if (pcn.pcn_namelen > NAME_MAX)
	return(ENAMETOOLONG);
  strcpy(pcn.pcn_name, name);

  if (bytes >= PATH_MAX)
	return(ENAMETOOLONG);

  /* Copy the target path (note that it's not null terminated) */
  if ((r = fsdriver_copyin(data, 0, target, bytes)) != OK)
	return r;

  target[bytes] = '\0';

  if (strlen(target) != bytes) {
	/* This can happen if the user provides a buffer
	 * with a \0 in it. This can cause a lot of trouble
	 * when the symlink is used later. We could just use
	 * the strlen() value, but we want to let the user
	 * know he did something wrong. ENAMETOOLONG doesn't
	 * exactly describe the error, but there is no
	 * ENAMETOOWRONG.
	 */
	return(ENAMETOOLONG);
  }

  if ((pn_dir = puffs_pn_nodewalk(global_pu, find_inode_cb, &dir_nr)) == NULL)
	return(EINVAL);

  memset(&pni, 0, sizeof(pni));
  pni.pni_cookie = (void** )&pn;

  (void)clock_time(&cur_time);

  memset(&va, 0, sizeof(va));
  va.va_type = VLNK;
  va.va_mode = (I_SYMBOLIC_LINK | RWX_MODES);
  va.va_uid = uid;
  va.va_gid = gid;
  va.va_atime = va.va_mtime = va.va_ctime = cur_time;

  if (buildpath) {
	r = puffs_path_pcnbuild(global_pu, &pcn, pn_dir);
	if (r) {
		lpuffs_debug("pathbuild error\n");
		return(ENOENT);
	}
  }

  r = global_pu->pu_ops.puffs_node_symlink(global_pu, pn_dir, &pni, &pcn, &va, target);
  if (buildpath) {
	if (r) {
		global_pu->pu_pathfree(global_pu, &pcn.pcn_po_full);
	} else {
		struct puffs_node *_pn;

		_pn = PU_CMAP(global_pu, pn);
		_pn->pn_po = pcn.pcn_po_full;
	}
  }

  if (r > 0) r = -r;

  return(r);
}
