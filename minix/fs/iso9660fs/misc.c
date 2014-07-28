#include "inc.h"
#include <fcntl.h>
#include <minix/vfsif.h>
#include <minix/bdev.h>


/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
int fs_sync()
{
  /* Always mounted read only, so nothing to sync */
  return(OK);		/* sync() can't fail */
}


/*===========================================================================*
 *				fs_new_driver				     *
 *===========================================================================*/
int fs_new_driver(void)
{
/* Set a new driver endpoint for this device. */
  dev_t dev;
  cp_grant_id_t label_gid;
  size_t label_len;
  char label[sizeof(fs_dev_label)];
  int r;

  dev = fs_m_in.m_vfs_fs_new_driver.device;
  label_gid = fs_m_in.m_vfs_fs_new_driver.grant;
  label_len = fs_m_in.m_vfs_fs_new_driver.path_len;

  if (label_len > sizeof(label))
	return(EINVAL);

  r = sys_safecopyfrom(fs_m_in.m_source, label_gid, (vir_bytes) 0,
	(vir_bytes) label, label_len);

  if (r != OK) {
	printf("ISOFS: fs_new_driver safecopyfrom failed (%d)\n", r);
	return(EINVAL);
  }

  bdev_driver(dev, label);

  return(OK);
}
