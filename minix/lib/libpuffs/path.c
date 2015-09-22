/*
 * This file contains the procedures that look up path names in the directory
 * system and determine the pnode number that goes with a given path name.
 *
 * Created (based on MFS):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"

#include <sys/cdefs.h>
#include <sys/stat.h>
#include <sys/types.h>

/*===========================================================================*
 *				fs_lookup				     *
 *===========================================================================*/
int fs_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt)
{
  struct puffs_node *pn, *pn_dir;

  /* Find the pnode of the directory node. */
  if ((pn_dir = puffs_pn_nodewalk(global_pu, find_inode_cb, &dir_nr)) == NULL) {
	lpuffs_debug("nodewalk failed\n");
	return(EINVAL);
  }

  if (!S_ISDIR(pn_dir->pn_va.va_mode))
	return ENOTDIR;

  if ((pn = advance(pn_dir, name)) == NULL)
	return err_code;

  pn->pn_count++; /* open pnode */

  node->fn_ino_nr = pn->pn_va.va_fileid;
  node->fn_mode = pn->pn_va.va_mode;
  node->fn_size = pn->pn_va.va_size;
  node->fn_uid = pn->pn_va.va_uid;
  node->fn_gid = pn->pn_va.va_gid;
  node->fn_dev = pn->pn_va.va_rdev;

  *is_mountpt = pn->pn_mountpoint;

  return OK;
}


/*===========================================================================*
 *				advance					     *
 *===========================================================================*/
struct puffs_node *advance(
	struct puffs_node *pn_dir,	/* pnode for directory to be searched */
	char string[NAME_MAX + 1]	/* component name to look for */
)
{
/* Given a directory and a component of a path, look up the component in
 * the directory, find the pnode, open it, and return a pointer to its pnode
 * slot.
 * TODO: instead of string, should get pcn.
 */
  struct puffs_node *pn;

  struct puffs_newinfo pni;

  struct puffs_kcn pkcnp;
  PUFFS_MAKECRED(pcr, &global_kcred);
  struct puffs_cn pcn = {&pkcnp, (struct puffs_cred *) __UNCONST(pcr), {0,0,0}};

  enum vtype node_vtype;
  voff_t size;
  dev_t rdev;
  int error;

  assert(pn_dir != NULL);

  err_code = OK;

  /* If 'string' is empty, return an error. */
  if (string[0] == '\0') {
	err_code = ENOENT;
	return(NULL);
  }

  /* If dir has been removed return ENOENT. */
  if (pn_dir->pn_va.va_nlink == NO_LINK) {
	err_code = ENOENT;
	return(NULL);
  }

  if (strcmp(string, ".") == 0) {
	/* Otherwise we will fall into trouble: path for pnode to be looked up
	 * will be parent path (same pnode as the one to be looked up) +
	 * requested path. E.g. after several lookups we might get advance
	 * for "." with parent path "/././././././././.".
	 * FIXME: how is ".." handled then?
	 *
	 * Another problem is that after lookup pnode will be added
	 * to the pu_pnodelst, which already contains pnode instance for this
	 * pnode. It will cause lot of troubles.
	 * FIXME: check if this is actually correct, because if it is, we are
	 * in lots of trouble; there are many ways to reach already-open pnodes
	 */
	return pn_dir;
  }

  pni.pni_cookie = (void** )&pn;
  pni.pni_vtype = &node_vtype;
  pni.pni_size = &size;
  pni.pni_rdev = &rdev;

  pcn.pcn_namelen = strlen(string);
  assert(pcn.pcn_namelen <= MAXPATHLEN);
  strcpy(pcn.pcn_name, string);

  if (buildpath) {
	if (puffs_path_pcnbuild(global_pu, &pcn, pn_dir) != 0) {
		lpuffs_debug("pathbuild error\n");
		err_code = ENOENT;
		return(NULL);
	}
  }

  /* lookup *must* be present */
  error = global_pu->pu_ops.puffs_node_lookup(global_pu, pn_dir, &pni, &pcn);

  if (buildpath) {
	if (error) {
		global_pu->pu_pathfree(global_pu, &pcn.pcn_po_full);
		err_code = ENOENT;
		return(NULL);
	} else {
		struct puffs_node *_pn;

		/*
		 * did we get a new node or a
		 * recycled node?
		 */
		_pn = PU_CMAP(global_pu, pn);
		if (_pn->pn_po.po_path == NULL)
			_pn->pn_po = pcn.pcn_po_full;
		else
			global_pu->pu_pathfree(global_pu, &pcn.pcn_po_full);
	}
  }

  if (error) {
	err_code = error < 0 ? error : -error;
	return(NULL);
  }

  err_code = OK;

  assert(pn != NULL);

  return(pn);
}
