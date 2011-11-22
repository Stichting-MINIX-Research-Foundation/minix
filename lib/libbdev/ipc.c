/* libbdev - IPC and recovery functions */

#include <minix/drivers.h>
#include <minix/bdev.h>
#include <assert.h>

#include "proto.h"

static void bdev_cancel(dev_t dev)
{
/* Recovering the driver for the given device has failed repeatedly. Mark it as
 * permanently unusable, and clean up any associated calls and resources.
 */

  printf("bdev: driver for major %d (endpoint %d) crashed\n",
	major(dev), bdev_driver_get(dev));

  /* Mark the driver as unusable. */
  bdev_driver_clear(dev);
}

void bdev_update(dev_t dev, endpoint_t endpt)
{
/* Set the endpoint for a driver. Perform recovery if necessary.
 */
  endpoint_t old_endpt;

  old_endpt = bdev_driver_get(dev);

  bdev_driver_set(dev, endpt);
}

int bdev_sendrec(dev_t dev, const message *m_orig)
{
/* Send a request to the given device, and wait for the reply.
 */
  static long id = 0;
  endpoint_t endpt;
  message m;
  int r;

  /* If we have no usable driver endpoint, fail instantly. */
  if ((endpt = bdev_driver_get(dev)) == NONE)
	return EDEADSRCDST;

  /* Send the request and block until we receive a reply. */
  m = *m_orig;
  m.BDEV_ID = ++id;

  r = sendrec(endpt, &m);

  /* This version of libbdev does not support recovery. Forget the driver. */
  if (r == EDEADSRCDST) {
	bdev_cancel(dev);

	return EDEADSRCDST;
  }

  if (r != OK) {
	printf("bdev: IPC to driver (%d) failed (%d)\n", endpt, r);
	return r;
  }

  if (m.m_type != BDEV_REPLY) {
	printf("bdev: driver (%d) sent weird response (%d)\n",
		endpt, m.m_type);
	return EIO;
  }

  /* ERESTART signifies a driver restart. Again, we do not support this yet. */
  if (m.BDEV_STATUS == ERESTART) {
	bdev_cancel(dev);

	return EDEADSRCDST;
  }

  if (m.BDEV_ID != id) {
	printf("bdev: driver (%d) sent invalid response (%ld)\n",
		endpt, m.BDEV_ID);
	return EIO;
  }

  /* Return the result of our request. */
  return m.BDEV_STATUS;
}
