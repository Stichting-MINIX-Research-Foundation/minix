
#include "fs.h"
#include <fcntl.h>
#include <signal.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <sys/select.h>
#include <sys/time.h>
#include "inode.h"
#include "super.h"

#include <minix/vfsif.h>



/*===========================================================================*
 *				fs_pipe					     *
 *===========================================================================*/
PUBLIC int fs_pipe(void)
{
  struct inode *rip;
  
  /* Get caller's user and group id from the request */
  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  
  /* Try to allocate the inode */
  if ( (rip = alloc_inode(fs_dev, I_REGULAR) ) == NIL_INODE) {
        return err_code;
  }

  /* Fill in the fields of the inode */
  rip->i_pipe = I_PIPE;
  rip->i_mode &= ~I_REGULAR;
  rip->i_mode |= I_NAMED_PIPE;	/* pipes and FIFOs have this bit set */
  
  /* We'll need it twice, nothing can go wrong here */
  rw_inode(rip, WRITING);	/* mark inode as allocated */
  rip->i_update = ATIME | CTIME | MTIME;
  
  /* Fill in the fields of the response message */
  fs_m_out.m_source = fs_dev;  /* filled with FS endpoint by the system */
  fs_m_out.RES_INODE_NR = rip->i_num;
  fs_m_out.RES_MODE = rip->i_mode;
  fs_m_out.RES_INODE_INDEX = (rip - &inode[0]) / sizeof(struct inode);
  fs_m_out.RES_FILE_SIZE = rip->i_size;
  fs_m_out.RES_UID = rip->i_uid;
  fs_m_out.RES_GID = rip->i_gid;

  return OK;
}


