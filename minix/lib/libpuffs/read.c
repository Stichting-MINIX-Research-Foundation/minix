/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <stddef.h>
#include <dirent.h>
#include <sys/param.h>


#define GETDENTS_BUFSIZ  4096
static char getdents_buf[GETDENTS_BUFSIZ];

#define RW_BUFSIZ	(128 * 1024)
static char rw_buf[RW_BUFSIZ];


/*===========================================================================*
 *				fs_read					     *
 *===========================================================================*/
ssize_t fs_read(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call)
{
  int r;
  size_t bytes_left, bytes_done;
  struct puffs_node *pn;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if ((pn = puffs_pn_nodewalk(global_pu, find_inode_cb, &ino_nr)) == NULL) {
	lpuffs_debug("walk failed...\n");
        return(EINVAL);
  }

  if (bytes > sizeof(rw_buf))
	bytes = sizeof(rw_buf);
  bytes_left = bytes;

  if (global_pu->pu_ops.puffs_node_read == NULL)
	return(EINVAL);

  r = global_pu->pu_ops.puffs_node_read(global_pu, pn, (uint8_t *)rw_buf,
						pos, &bytes_left, pcr, 0);
  if (r) {
	lpuffs_debug("puffs_node_read failed\n");
	return(EINVAL);
  }

  bytes_done = bytes - bytes_left;

  if (bytes_done > 0) {
	if ((r = fsdriver_copyout(data, 0, rw_buf, bytes_done)) != OK)
		return r;
	update_timens(pn, ATIME, NULL);
  }

  return (ssize_t)bytes_done;
}


/*===========================================================================*
 *				fs_write				     *
 *===========================================================================*/
ssize_t fs_write(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call)
{
  int r;
  size_t bytes_left;
  struct puffs_node *pn;
  struct vattr va;
  struct timespec cur_time;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if ((pn = puffs_pn_nodewalk(global_pu, find_inode_cb, &ino_nr)) == NULL) {
	lpuffs_debug("walk failed...\n");
        return(EINVAL);
  }

  if (bytes > sizeof(rw_buf))
	bytes = sizeof(rw_buf);
  bytes_left = bytes;

  /* At first try to change vattr */
  if (global_pu->pu_ops.puffs_node_setattr == NULL)
	return(EINVAL);

  (void)clock_time(&cur_time);

  puffs_vattr_null(&va);
  if ((u_quad_t)(pos + bytes_left) > pn->pn_va.va_size)
	va.va_size = bytes_left + pos;
  va.va_ctime = va.va_mtime = cur_time;
  va.va_atime = pn->pn_va.va_atime;

  r = global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr);
  if (r) return(EINVAL);

  if ((r = fsdriver_copyin(data, 0, rw_buf, bytes)) != OK)
	return r;

  if (global_pu->pu_ops.puffs_node_write == NULL)
	return(EINVAL);

  r = global_pu->pu_ops.puffs_node_write(global_pu, pn, (uint8_t *)rw_buf,
						pos, &bytes_left, pcr, 0);
  if (r != OK) return(EINVAL);

  return (ssize_t)(bytes - bytes_left);
}


/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *pos)
{
  int r;
  register struct puffs_node *pn;
  size_t buf_left, written;
  struct dirent *dent;
  int eofflag = 0;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if ((pn = puffs_pn_nodewalk(global_pu, find_inode_cb, &ino_nr)) == NULL) {
	lpuffs_debug("walk failed...\n");
        return(EINVAL);
  }

  if (bytes > sizeof(getdents_buf))
	  bytes = sizeof(getdents_buf);
  memset(getdents_buf, 0, sizeof(getdents_buf)); /* Avoid leaking any data */

  buf_left = bytes;

  dent = (struct dirent*) getdents_buf;

  r = global_pu->pu_ops.puffs_node_readdir(global_pu, pn, dent, pos,
						&buf_left, pcr, &eofflag, 0, 0);
  if (r) {
	lpuffs_debug("puffs_node_readdir returned error\n");
	return(EINVAL);
  }

  assert(buf_left <= bytes);
  written = bytes - buf_left;

  if (written == 0 && !eofflag) {
	lpuffs_debug("The user's buffer is too small\n");
	return(EINVAL);
  }

  if (written) {
	if ((r = fsdriver_copyout(data, 0, getdents_buf, written)) != OK)
		return r;
  }

  update_timens(pn, ATIME, NULL);

  /* The puffs readdir call has already updated the position. */
  return written;
}
