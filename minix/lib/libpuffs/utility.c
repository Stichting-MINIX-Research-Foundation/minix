/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"

#include <stdarg.h>

/*
 * Match by inode number in a puffs_pn_nodewalk call.  This should not exist.
 */
void *
find_inode_cb(struct puffs_usermount * __unused pu, struct puffs_node * pn,
	void * arg)
{

	if (pn->pn_va.va_fileid == *(ino_t *)arg)
		return pn;
	else
		return NULL;
}

/*===========================================================================*
 *				update_timens				     *
 *===========================================================================*/
int update_timens(struct puffs_node *pn, int flags, struct timespec *t)
{
  int r;
  struct vattr va;
  struct timespec new_time;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (!flags)
	return 0;

  if (global_pu->pu_ops.puffs_node_setattr == NULL)
	return EINVAL;

  if (t != NULL)
	new_time = *t;
  else
	(void)clock_time(&new_time);

  puffs_vattr_null(&va);
  /* librefuse modifies atime and mtime together,
   * so set old values to avoid setting either one
   * to PUFFS_VNOVAL (set by puffs_vattr_null).
   */
  va.va_atime = pn->pn_va.va_atime;
  va.va_mtime = pn->pn_va.va_mtime;

  if (flags & ATIME)
	va.va_atime = new_time;
  if (flags & MTIME)
	va.va_mtime = new_time;
  if (flags & CTIME)
	va.va_ctime = new_time;

  r = global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr);

  return(r);
}


/*===========================================================================*
 *				lpuffs_debug				     *
 *===========================================================================*/
void lpuffs_debug(const char *format, ...)
{
  char buffer[256];
  va_list args;
  va_start (args, format);
  vsprintf (buffer,format, args);
  printf("%s: %s", fs_name, buffer);
  va_end (args);
}
