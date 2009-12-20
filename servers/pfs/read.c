#include "fs.h"
#include "buf.h"
#include <minix/com.h>
#include <string.h>
#include <minix/u64.h>
#include "inode.h"


/*===========================================================================*
 *				fs_readwrite				     *
 *===========================================================================*/
PUBLIC int fs_readwrite(void)
{
  int r, rw_flag;
  block_t b;
  struct buf *bp;
  cp_grant_id_t gid;
  off_t position, f_size;
  unsigned int nrbytes, cum_io;
  mode_t mode_word;
  struct inode *rip;
  ino_t inumb;

  r = OK;
  cum_io = 0;
  inumb = fs_m_in.REQ_INODE_NR;
  rw_flag = (fs_m_in.m_type == REQ_READ ? READING : WRITING);
#if 0  
  printf("PFS: going to %s inode %d\n", (rw_flag == READING? "read from": "write to"), inumb);
#endif

  /* Find the inode referred */
  if ((rip = find_inode(inumb)) == NIL_INODE) return(EINVAL);

  mode_word = rip->i_mode & I_TYPE;
  if (mode_word != I_NAMED_PIPE) return(EIO);
  f_size = rip->i_size;
  
  /* Get the values from the request message */ 
  rw_flag = (fs_m_in.m_type == REQ_READ ? READING : WRITING);
  gid = fs_m_in.REQ_GRANT;
  position = fs_m_in.REQ_SEEK_POS_LO;
  nrbytes = (unsigned) fs_m_in.REQ_NBYTES;
  
  if (rw_flag == WRITING) {
	  /* Check in advance to see if file will grow too big. */
	  if (position > PIPE_BUF - nrbytes) return(EFBIG);
  }

  /* Mark inode in use */
  if ((get_inode(rip->i_dev, rip->i_num)) == NIL_INODE) return(err_code);
  if ((bp = get_block(rip->i_dev, rip->i_num)) == NIL_BUF) return(err_code);

  if (rw_flag == READING) {
	/* Copy a chunk from the block buffer to user space. */
	r = sys_safecopyto(FS_PROC_NR, gid, 0,
		(vir_bytes) (bp->b_data+position), (phys_bytes) nrbytes, D);
  } else {
	/* Copy a chunk from user space to the block buffer. */
	r = sys_safecopyfrom(FS_PROC_NR, gid, 0,
		(vir_bytes) (bp->b_data+position), (phys_bytes) nrbytes, D);
  }

  if (r == OK) {
	position += nrbytes; /* Update position */
	cum_io += nrbytes;
  }

  fs_m_out.RES_SEEK_POS_LO = position; /* It might change later and the VFS
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
  fs_m_out.RES_NBYTES = cum_io;
  put_inode(rip);
  put_block(rip->i_dev, rip->i_num);

  return(r);
}

