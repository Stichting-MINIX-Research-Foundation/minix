#include "fs.h"


/*===========================================================================*
 *				fs_newnode				     *
 *===========================================================================*/
int fs_newnode(mode_t mode, uid_t uid, gid_t gid, dev_t dev,
	struct fsdriver_node *node)
{
  register int r = OK;
  struct inode *rip;

  /* Try to allocate the inode */
  if( (rip = alloc_inode(dev, mode, uid, gid) ) == NULL) return(err_code);

  switch (mode & S_IFMT) {
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
	node->fn_ino_nr = rip->i_num;
	node->fn_mode = rip->i_mode;
	node->fn_size = rip->i_size;
	node->fn_uid = rip->i_uid;
	node->fn_gid = rip->i_gid;
	node->fn_dev = dev;
  }

  return(r);
}
