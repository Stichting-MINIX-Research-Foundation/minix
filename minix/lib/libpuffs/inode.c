/*
 * Created (MFS based):
 *   June 2011 (Evgeniy Ivanov)
 */

#include "fs.h"

void release_node(struct puffs_usermount *pu, struct puffs_node *pn)
{
  assert(pn->pn_count == 0);

  /* Required if puffs_node_reclaim() decides to leave node in the list */
  pn->pn_mountpoint = FALSE;

  if (pu->pu_ops.puffs_node_reclaim) {
	if (global_pu->pu_ops.puffs_node_reclaim(global_pu, pn) != 0)
		lpuffs_debug("Warning: reclaim failed\n");
  } else {
	puffs_pn_put(pn);
  }
}


/*===========================================================================*
 *                fs_putnode                                                 *
 *===========================================================================*/
int fs_putnode(ino_t ino_nr, unsigned int count)
{
/* Find the pnode specified by the request message and decrease its counter.
 * Release unused pnode.
 */
  struct puffs_node *pn;

  if ((pn = puffs_pn_nodewalk(global_pu, find_inode_cb, &ino_nr)) == NULL) {
	/* XXX Probably removed from the list, see puffs_pn_remove() */
	struct puffs_node *pn_cur, *pn_next;
	pn_cur = LIST_FIRST(&global_pu->pu_pnode_removed_lst);
	while (pn_cur) {
		pn_next = LIST_NEXT(pn_cur, pn_entries);
		if (pn_cur->pn_va.va_fileid == ino_nr) {
			pn = pn_cur;
			break;
		}
		pn_cur = pn_next;
	}
  }

  if (pn == NULL) {
	lpuffs_debug("%s:%d putnode: pnode #%"PRIu64" not found\n",
	    __FILE__, __LINE__, ino_nr);
	panic("fs_putnode failed");
  }

  if (count <= 0) {
	lpuffs_debug("%s:%d putnode: bad value for count: %d\n", __FILE__,
		__LINE__, count);
	panic("fs_putnode failed");
  } else if (pn->pn_count == 0) {
	/* FUSE fs might store in the list pnodes, which we hasn't
	 * open, this means we got put request for file,
	 * which wasn't opened by VFS.
	 */
	lpuffs_debug("%s:%d putnode: pn_count already zero\n", __FILE__,
		__LINE__);
	panic("fs_putnode failed");
  } else if (count > pn->pn_count) {
	struct puffs_node *pn_cur, *pn_next;
	struct puffs_usermount *pu = global_pu;
	ino_t ino = pn->pn_va.va_fileid;

	pn_cur = LIST_FIRST(&pu->pu_pnodelst);
	lpuffs_debug("inum  count  path  polen  hash\n");
	while (pn_cur) {
		pn_next = LIST_NEXT(pn_cur, pn_entries);
		if (pn_cur->pn_va.va_fileid == ino) {
			lpuffs_debug("%"PRIu64": %d %s %u %u\n",
				ino,
				pn_cur->pn_count,
				(char *)pn_cur->pn_po.po_path,
				pn_cur->pn_po.po_len,
				pn_cur->pn_po.po_hash);
		}
		pn_cur = pn_next;
	}
	lpuffs_debug("%s:%d putnode: count too high: %d > %d\n", __FILE__,
		__LINE__, count, pn->pn_count);
	panic("fs_putnode failed");
  }

  pn->pn_count -= count;

  if (pn->pn_count == 0)
	release_node(global_pu, pn);

  return(OK);
}
