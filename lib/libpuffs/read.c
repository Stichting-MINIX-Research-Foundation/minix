/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <minix/vfsif.h>
#include <assert.h>
#include <sys/param.h>

#include "puffs.h"
#include "puffs_priv.h"


#define GETDENTS_BUFSIZ  4096
static char getdents_buf[GETDENTS_BUFSIZ];

#define RW_BUFSIZ	(128 << 10)
static char rw_buf[RW_BUFSIZ];


/*===========================================================================*
 *				fs_readwrite				     *
 *===========================================================================*/
int fs_readwrite(void)
{
  int r = OK, rw_flag;
  cp_grant_id_t gid;
  off_t pos;
  size_t nrbytes, bytes_left, bytes_done = 0;
  struct puffs_node *pn;
  struct vattr va;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if ((pn = puffs_pn_nodewalk(global_pu, 0, &fs_m_in.REQ_INODE_NR)) == NULL) {
  	lpuffs_debug("walk failed...\n");
        return(EINVAL);
  }

  /* Get the values from the request message */
  rw_flag = (fs_m_in.m_type == REQ_READ ? READING : WRITING);
  gid = (cp_grant_id_t) fs_m_in.REQ_GRANT;
  pos = (off_t) fs_m_in.REQ_SEEK_POS_LO;
  nrbytes = bytes_left = (size_t) fs_m_in.REQ_NBYTES;

  if (nrbytes > RW_BUFSIZ)
	nrbytes = bytes_left = RW_BUFSIZ;

  memset(getdents_buf, '\0', GETDENTS_BUFSIZ);  /* Avoid leaking any data */

  if (rw_flag == READING) {
	if (global_pu->pu_ops.puffs_node_read == NULL)
		return(EINVAL);

	r = global_pu->pu_ops.puffs_node_read(global_pu, pn, (uint8_t *)rw_buf,
						pos, &bytes_left, pcr, 0);
	if (r) {
		lpuffs_debug("puffs_node_read failed\n");
		return(EINVAL);
	}

	bytes_done = nrbytes - bytes_left;
	if (bytes_done) {
		r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) 0,
				   (vir_bytes) rw_buf, bytes_done);
		update_timens(pn, ATIME, NULL);
	}
  } else if (rw_flag == WRITING) {
	/* At first try to change vattr */
	if (global_pu->pu_ops.puffs_node_setattr == NULL)
		return(EINVAL);

	puffs_vattr_null(&va);
	if ( (pos + bytes_left) > pn->pn_va.va_size)
		va.va_size = bytes_left + pos;
	va.va_ctime = va.va_mtime = clock_timespec();
	va.va_atime = pn->pn_va.va_atime;

	r = global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr);
	if (r) return(EINVAL);

	r = sys_safecopyfrom(VFS_PROC_NR, gid, (vir_bytes) 0,
			     (vir_bytes) rw_buf, nrbytes);
	if (r != OK) return(EINVAL);

	if (global_pu->pu_ops.puffs_node_write == NULL)
		return(EINVAL);

	r = global_pu->pu_ops.puffs_node_write(global_pu, pn, (uint8_t *)rw_buf,
						pos, &bytes_left, pcr, 0);
	bytes_done = nrbytes - bytes_left;
  }

  if (r != OK) return(EINVAL);

  fs_m_out.RES_SEEK_POS_LO = pos + bytes_done;
  fs_m_out.RES_NBYTES = bytes_done;

  return(r);
}


/*===========================================================================*
 *				fs_breadwrite				     *
 *===========================================================================*/
int fs_breadwrite(void)
{
  /* We do not support breads/writes */
  panic("bread write requested, but FS doesn't support it!\n");
  return(OK);
}


/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
int fs_getdents(void)
{
  int r;
  register struct puffs_node *pn;
  ino_t ino;
  cp_grant_id_t gid;
  size_t size, buf_left;
  off_t pos;
  struct dirent *dent;
  int eofflag = 0;
  size_t written;
  PUFFS_MAKECRED(pcr, &global_kcred);

  ino = (ino_t) fs_m_in.REQ_INODE_NR;
  gid = (gid_t) fs_m_in.REQ_GRANT;
  size = buf_left = (size_t) fs_m_in.REQ_MEM_SIZE;
  pos = (off_t) fs_m_in.REQ_SEEK_POS_LO;

  if ((pn = puffs_pn_nodewalk(global_pu, 0, &ino)) == NULL) {
	lpuffs_debug("walk failed...\n");
        return(EINVAL);
  }

  if (GETDENTS_BUFSIZ < size)
	  size = buf_left = GETDENTS_BUFSIZ;
  memset(getdents_buf, '\0', GETDENTS_BUFSIZ);  /* Avoid leaking any data */

  dent = (struct dirent*) getdents_buf;

  r = global_pu->pu_ops.puffs_node_readdir(global_pu, pn, dent, &pos,
						&buf_left, pcr, &eofflag, 0, 0);
  if (r) {
	lpuffs_debug("puffs_node_readdir returned error\n");
	return(EINVAL);
  }

  assert(buf_left <= size);
  written = size - buf_left;

  if (written == 0 && !eofflag) {
	lpuffs_debug("The user's buffer is too small\n");
	return(EINVAL);
  }

  if (written) {
	r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) 0,
			     (vir_bytes) getdents_buf, written);
	if (r != OK) return(r);
  }

  update_timens(pn, ATIME, NULL);

  fs_m_out.RES_NBYTES = written;
  fs_m_out.RES_SEEK_POS_LO = pos;

  return(OK);
}
