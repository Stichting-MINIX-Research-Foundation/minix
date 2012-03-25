#include "fs.h"
#include <sys/stat.h>
#include "buf.h"
#include "inode.h"
#include <minix/vfsif.h>


/*===========================================================================*
 *				fs_newnode				     *
 *===========================================================================*/
int fs_newnode(message *fs_m_in, message *fs_m_out)
{
  register int r = OK;
  mode_t bits;
  struct inode *rip;
  dev_t dev;

  caller_uid = (uid_t) fs_m_in->REQ_UID;
  caller_gid = (gid_t) fs_m_in->REQ_GID;
  bits = (mode_t) fs_m_in->REQ_MODE;
  dev = (dev_t) fs_m_in->REQ_DEV;

  /* Try to allocate the inode */
  if( (rip = alloc_inode(dev, bits) ) == NULL) return(err_code);

  switch (bits & S_IFMT) {
	case S_IFBLK:
	case S_IFCHR:
		rip->i_rdev = dev;		/* Major/minor dev numbers */
		break;
	case S_IFIFO:
		if ((get_block(dev, rip->i_num)) == NULL)
			r = EIO;
		break;
	default:
		r = EIO; /* Unsupported file type */
  }

  if (r != OK) {
	free_inode(rip);
  } else {
	/* Fill in the fields of the response message */
	fs_m_out->RES_INODE_NR = rip->i_num;
	fs_m_out->RES_MODE = rip->i_mode;
	fs_m_out->RES_FILE_SIZE_LO = rip->i_size;
	fs_m_out->RES_UID = rip->i_uid;
	fs_m_out->RES_GID = rip->i_gid;
	fs_m_out->RES_DEV = dev;
  }

  return(r);
}
