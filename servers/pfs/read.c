#include "fs.h"
#include "buf.h"
#include <minix/com.h>
#include "inode.h"


/*===========================================================================*
 *				fs_readwrite				     *
 *===========================================================================*/
PUBLIC int fs_readwrite(message *fs_m_in, message *fs_m_out)
{
  int r, rw_flag;
  struct buf *bp;
  cp_grant_id_t gid;
  off_t position, f_size;
  unsigned int nrbytes, cum_io;
  mode_t mode_word;
  struct inode *rip;
  ino_t inumb;

  r = OK;
  cum_io = 0;
  inumb = (ino_t) fs_m_in->REQ_INODE_NR;

  /* Find the inode referred */
  if ((rip = find_inode(inumb)) == NULL) return(EINVAL);

  mode_word = rip->i_mode & I_TYPE;
  if (mode_word != I_NAMED_PIPE) return(EIO);
  f_size = rip->i_size;
  
  /* Get the values from the request message */ 
  rw_flag = (fs_m_in->m_type == REQ_READ ? READING : WRITING);
  gid = (cp_grant_id_t) fs_m_in->REQ_GRANT;
  position = fs_m_in->REQ_SEEK_POS_LO;
  nrbytes = (unsigned) fs_m_in->REQ_NBYTES;

  /* We can't read beyond the max file position */
  if (nrbytes > MAX_FILE_POS) return(EFBIG);
  
  if (rw_flag == WRITING) {
	  /* Check in advance to see if file will grow too big. */
	  /* Casting nrbytes to signed is safe, because it's guaranteed not to
	     be beyond max signed value (i.e., MAX_FILE_POS). */
	  if (position > PIPE_BUF - (signed) nrbytes) return(EFBIG);
  }

  /* Mark inode in use */
  if ((get_inode(rip->i_dev, rip->i_num)) == NULL) return(err_code);
  if ((bp = get_block(rip->i_dev, rip->i_num)) == NULL) return(err_code);

  if (rw_flag == READING) {
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(VFS_PROC_NR, gid, (vir_bytes) 0,
		(vir_bytes) (bp->b_data+position), (size_t) nrbytes, D);
  } else {
	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(VFS_PROC_NR, gid, (vir_bytes) 0,
		(vir_bytes) (bp->b_data+position), (size_t) nrbytes, D);
  }

  if (r == OK) {
	position += (signed) nrbytes; /* Update position */
	cum_io += nrbytes;
  }

  fs_m_out->RES_SEEK_POS_LO = position; /* It might change later and the VFS
					   has to know this value */
  
  /* On write, update file size and access time. */
  if (rw_flag == WRITING) {
	  if (position > f_size) rip->i_size = position;
  } else {
	if(position >= rip->i_size) {
		/* All data in the pipe is read, so reset pipe pointers */
		rip->i_size = 0;	/* no data left */
		position = 0;		/* reset reader(s) */
	}
  }

  bp->b_bytes = position;
  if (rw_flag == READING) rip->i_update |= ATIME;
  if (rw_flag == WRITING) rip->i_update |= CTIME | MTIME;
  fs_m_out->RES_NBYTES = (size_t) cum_io;
  put_inode(rip);
  put_block(rip->i_dev, rip->i_num);

  return(r);
}

