#include "fs.h"
#include "buf.h"
#include "inode.h"
#include <minix/com.h>
#include <string.h>


/*===========================================================================*
 *				fs_readwrite				     *
 *===========================================================================*/
int fs_readwrite(message *fs_m_in, message *fs_m_out)
{
  int r, rw_flag;
  struct buf *bp;
  cp_grant_id_t gid;
  off_t position, f_size;
  size_t nrbytes, cum_io;
  mode_t mode_word;
  struct inode *rip;
  ino_t inumb;

  r = OK;
  cum_io = 0;
  inumb = fs_m_in->m_vfs_fs_readwrite.inode;

  /* Find the inode referred */
  if ((rip = find_inode(inumb)) == NULL) return(EINVAL);

  mode_word = rip->i_mode & I_TYPE;
  if (mode_word != I_NAMED_PIPE) return(EIO);
  f_size = rip->i_size;

  /* Get the values from the request message */
  rw_flag = (fs_m_in->m_type == REQ_READ ? READING : WRITING);
  gid = fs_m_in->m_vfs_fs_readwrite.grant;
  nrbytes = fs_m_in->m_vfs_fs_readwrite.nbytes;

  /* We can't read beyond the max file position */
  if (nrbytes > PIPE_BUF) return(EFBIG);

  /* Mark inode in use */
  if ((get_inode(rip->i_dev, rip->i_num)) == NULL) return(err_code);
  if ((bp = get_block(rip->i_dev, rip->i_num)) == NULL) return(err_code);

  if (rw_flag == WRITING) {
	/* Check in advance to see if file will grow too big. */
	/* Casting nrbytes to signed is safe, because it's guaranteed not to
	 * be beyond max signed value (i.e., MAX_FILE_POS).
	 */
	position = rip->i_size;
	if ((unsigned) position + nrbytes > PIPE_BUF) {
		put_inode(rip);
		put_block(rip->i_dev, rip->i_num);
		return(EFBIG);
	}
  } else {
	position = 0;
	if (nrbytes > rip->i_size) {
		/* There aren't that many bytes to read */
		nrbytes = rip->i_size;
	}
  }

  if (rw_flag == READING) {
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(fs_m_in->m_source, gid, (vir_bytes) 0,
		(vir_bytes) (bp->b_data+position), (size_t) nrbytes);
  } else {
	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(fs_m_in->m_source, gid, (vir_bytes) 0,
		(vir_bytes) (bp->b_data+position), (size_t) nrbytes);
  }

  if (r == OK) {
	position += (signed) nrbytes; /* Update position */
	cum_io += nrbytes;

	/* On write, update file size and access time. */
	if (rw_flag == WRITING) {
		rip->i_size = position;
	} else {
		memmove(bp->b_data, bp->b_data+nrbytes, rip->i_size - nrbytes);
		rip->i_size -= nrbytes;
	}

	if (rw_flag == READING) rip->i_update |= ATIME;
	if (rw_flag == WRITING) rip->i_update |= CTIME | MTIME;
  }

  fs_m_out->m_fs_vfs_readwrite.nbytes = cum_io;
  fs_m_out->m_fs_vfs_readwrite.seek_pos = rip->i_size;

  put_inode(rip);
  put_block(rip->i_dev, rip->i_num);

  return(r);
}
