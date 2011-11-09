#include "inc.h"
#include <fcntl.h>
#include <minix/vfsif.h>
#include <minix/bdev.h>


/*===========================================================================*
 *				fs_sync					     *
 *===========================================================================*/
PUBLIC int fs_sync()
{
  /* Always mounted read only, so nothing to sync */
  return(OK);		/* sync() can't fail */
}


/*===========================================================================*
 *				fs_new_driver				     *
 *===========================================================================*/
PUBLIC int fs_new_driver()
{
/* Set a new driver endpoint for this device. */
  dev_t dev;
  endpoint_t endpt;

  dev = (dev_t) fs_m_in.REQ_DEV;
  endpt = (endpoint_t) fs_m_in.REQ_DRIVER_E;

  bdev_driver(dev, endpt);

  return(OK);
}
