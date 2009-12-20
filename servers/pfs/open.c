#include "fs.h"
#include <sys/stat.h>
#include <unistd.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include "buf.h"
#include "inode.h"
#include <minix/vfsif.h>


/*===========================================================================*
 *				fs_newnode				     *
 *===========================================================================*/
PUBLIC int fs_newnode()
{
  register int r = OK;
  mode_t bits;
  struct inode *rip;
  dev_t dev;

  caller_uid = fs_m_in.REQ_UID;
  caller_gid = fs_m_in.REQ_GID;
  bits = fs_m_in.REQ_MODE;
  dev = fs_m_in.REQ_DEV;

  /* Try to allocate the inode */
  if( (rip = alloc_inode(dev, bits) ) == NIL_INODE) return(err_code);

  if (bits & S_IFMT != S_IFIFO) {
		r = EIO; /* We only support pipes */
  } else if ((get_block(dev, rip->i_num)) == NIL_BUF)
  		r = EIO; 

  if (r != OK) {
	free_inode(rip);
  } else {
	/* Fill in the fields of the response message */
	fs_m_out.RES_INODE_NR = rip->i_num;
	fs_m_out.RES_MODE = rip->i_mode;
	fs_m_out.RES_FILE_SIZE_LO = rip->i_size;
	fs_m_out.RES_UID = rip->i_uid;
	fs_m_out.RES_GID = rip->i_gid;
	fs_m_out.RES_DEV = dev;
  }

  return(r);
}

