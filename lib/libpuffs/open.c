/* Created (MFS based):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <minix/com.h>
#include <minix/vfsif.h>

#include "puffs.h"
#include "puffs_priv.h"


/*===========================================================================*
 *				fs_create				     *
 *===========================================================================*/
int fs_create()
{
  int r;
  struct puffs_node *pn_dir;
  struct puffs_node *pn;
  mode_t omode;
  struct puffs_newinfo pni;
  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};
  struct vattr va;
  time_t cur_time;
  int len;

  if (global_pu->pu_ops.puffs_node_create == NULL) {
  	lpuffs_debug("No puffs_node_create");
	return(ENFILE);
  }

  /* Read request message */
  omode = (mode_t) fs_m_in.REQ_MODE;
  caller_uid = (uid_t) fs_m_in.REQ_UID;
  caller_gid = (gid_t) fs_m_in.REQ_GID;

  /* Copy the last component (i.e., file name) */
  len = fs_m_in.REQ_PATH_LEN;
  pcn.pcn_namelen = len - 1;
  if (pcn.pcn_namelen > NAME_MAX)
	return(ENAMETOOLONG);

  err_code = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_GRANT,
			      (vir_bytes) 0, (vir_bytes) pcn.pcn_name,
			      (size_t) len);
  if (err_code != OK) return(err_code);
  NUL(pcn.pcn_name, len, sizeof(pcn.pcn_name));

  /* Get last directory pnode (i.e., directory that will hold the new pnode) */
  if ((pn_dir = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
	return(ENOENT);

  memset(&pni, 0, sizeof(pni));
  pni.pni_cookie = (void** )&pn;

  cur_time = clock_time();
  
  memset(&va, 0, sizeof(va));
  va.va_type = VREG;
  va.va_mode = omode;
  va.va_uid = caller_uid;
  va.va_gid = caller_gid;
  va.va_atime.tv_sec = va.va_mtime.tv_sec = va.va_ctime.tv_sec = cur_time;

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

  update_times(pn_dir, MTIME | CTIME, cur_time);

  /* Reply message */
  fs_m_out.RES_INODE_NR = pn->pn_va.va_fileid;
  fs_m_out.RES_MODE = pn->pn_va.va_mode;
  fs_m_out.RES_FILE_SIZE_LO = pn->pn_va.va_size;

  /* This values are needed for the execution */
  fs_m_out.RES_UID = pn->pn_va.va_uid;
  fs_m_out.RES_GID = pn->pn_va.va_gid;

  return(OK);
}


/*===========================================================================*
 *				fs_mknod				     *
 *===========================================================================*/
int fs_mknod()
{
  int r;
  struct puffs_node *pn_dir;
  struct puffs_node *pn;
  struct puffs_newinfo pni;
  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};
  struct vattr va;
  time_t cur_time;
  int len;

  if (global_pu->pu_ops.puffs_node_mknod == NULL) {
  	lpuffs_debug("No puffs_node_mknod");
	return(ENFILE);
  }

  /* Copy the last component and set up caller's user and group id */
  len = fs_m_in.REQ_PATH_LEN;
  pcn.pcn_namelen = len - 1;
  if (pcn.pcn_namelen > NAME_MAX)
	return(ENAMETOOLONG);

  err_code = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_GRANT,
                             (vir_bytes) 0, (vir_bytes) pcn.pcn_name,
			     (size_t) len);
  if (err_code != OK) return(err_code);
  NUL(pcn.pcn_name, len, sizeof(pcn.pcn_name));

  caller_uid = (uid_t) fs_m_in.REQ_UID;
  caller_gid = (gid_t) fs_m_in.REQ_GID;

  /* Get last directory pnode */
  if ((pn_dir = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
	return(ENOENT);

  memset(&pni, 0, sizeof(pni));
  pni.pni_cookie = (void** )&pn;

  cur_time = clock_time();

  memset(&va, 0, sizeof(va));
  va.va_type = VDIR;
  va.va_mode = (mode_t) fs_m_in.REQ_MODE;
  va.va_uid = caller_uid;
  va.va_gid = caller_gid;
  va.va_rdev = (dev_t) fs_m_in.REQ_DEV;
  va.va_atime.tv_sec = va.va_mtime.tv_sec = va.va_ctime.tv_sec = cur_time;

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

  update_times(pn_dir, MTIME | CTIME, cur_time);

  return(OK);
}


/*===========================================================================*
 *				fs_mkdir				     *
 *===========================================================================*/
int fs_mkdir()
{
  int r;
  struct puffs_node *pn_dir;
  struct puffs_node *pn;
  struct puffs_newinfo pni;
  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};
  struct vattr va;
  time_t cur_time;
  int len;

  if (global_pu->pu_ops.puffs_node_mkdir == NULL) {
  	lpuffs_debug("No puffs_node_mkdir");
	return(ENFILE);
  }

  /* Copy the last component and set up caller's user and group id */
  len = fs_m_in.REQ_PATH_LEN;
  pcn.pcn_namelen = len - 1;
  if (pcn.pcn_namelen > NAME_MAX)
	return(ENAMETOOLONG);

  err_code = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_GRANT,
			      (vir_bytes) 0, (vir_bytes) pcn.pcn_name,
			      (phys_bytes) len);
  if (err_code != OK) return(err_code);
  NUL(pcn.pcn_name, len, sizeof(pcn.pcn_name));

  caller_uid = (uid_t) fs_m_in.REQ_UID;
  caller_gid = (gid_t) fs_m_in.REQ_GID;

  /* Get last directory pnode */
  if ((pn_dir = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
	return(ENOENT);
  
  cur_time = clock_time();

  memset(&pni, 0, sizeof(pni));
  pni.pni_cookie = (void** )&pn;

  memset(&va, 0, sizeof(va));
  va.va_type = VDIR;
  va.va_mode = (mode_t) fs_m_in.REQ_MODE;
  va.va_uid = caller_uid;
  va.va_gid = caller_gid;
  va.va_atime.tv_sec = va.va_mtime.tv_sec = va.va_ctime.tv_sec = cur_time;

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

  update_times(pn_dir, MTIME | CTIME, cur_time);

  return(OK);
}


/*===========================================================================*
 *                             fs_slink 				     *
 *===========================================================================*/
int fs_slink()
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
  int len;

  caller_uid = (uid_t) fs_m_in.REQ_UID;
  caller_gid = (gid_t) fs_m_in.REQ_GID;

  /* Copy the link name's last component */
  len = fs_m_in.REQ_PATH_LEN;
  pcn.pcn_namelen = len - 1;
  if (pcn.pcn_namelen > NAME_MAX)
	return(ENAMETOOLONG);

  if (fs_m_in.REQ_MEM_SIZE >= PATH_MAX)
	return(ENAMETOOLONG);

  r = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_GRANT,
		       (vir_bytes) 0, (vir_bytes) pcn.pcn_name,
		       (size_t) len);
  if (r != OK) return(r);
  NUL(pcn.pcn_name, len, sizeof(pcn.pcn_name));

  /* Copy the target path (note that it's not null terminated) */
  r = sys_safecopyfrom(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_GRANT3,
		       (vir_bytes) 0, (vir_bytes) target, 
		       (size_t) fs_m_in.REQ_MEM_SIZE);
  if (r != OK) return(r);
  target[fs_m_in.REQ_MEM_SIZE] = '\0';

  if (strlen(target) != fs_m_in.REQ_MEM_SIZE) {
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

  if ((pn_dir = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL)
	return(EINVAL);

  memset(&pni, 0, sizeof(pni));
  pni.pni_cookie = (void** )&pn;

  memset(&va, 0, sizeof(va));
  va.va_type = VLNK;
  va.va_mode = (mode_t) (I_SYMBOLIC_LINK | RWX_MODES);
  va.va_uid = caller_uid;
  va.va_gid = caller_gid;
  va.va_atime.tv_sec = va.va_mtime.tv_sec = va.va_ctime.tv_sec = clock_time();

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


/*===========================================================================*
 *				fs_inhibread				     *
 *===========================================================================*/
int fs_inhibread()
{
  return(OK);
}
