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
  uid_t uid;
  gid_t gid;
  dev_t dev;

  uid = fs_m_in->m_vfs_fs_newnode.uid;
  gid = fs_m_in->m_vfs_fs_newnode.gid;
  bits = fs_m_in->m_vfs_fs_newnode.mode;
  dev = fs_m_in->m_vfs_fs_newnode.device;

  /* Try to allocate the inode */
  if( (rip = alloc_inode(dev, bits, uid, gid) ) == NULL) return(err_code);

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
	fs_m_out->m_fs_vfs_newnode.inode = rip->i_num;
	fs_m_out->m_fs_vfs_newnode.mode = rip->i_mode;
	fs_m_out->m_fs_vfs_newnode.file_size = rip->i_size;
	fs_m_out->m_fs_vfs_newnode.uid = rip->i_uid;
	fs_m_out->m_fs_vfs_newnode.gid = rip->i_gid;
	fs_m_out->m_fs_vfs_newnode.device = dev;
  }

  return(r);
}
