/* libbdev - driver endpoint management */

#include <minix/drivers.h>
#include <minix/bdev.h>
#include <assert.h>

#include "proto.h"

static endpoint_t driver_endpt[NR_DEVICES];

void bdev_driver_init(void)
{
/* Initialize the driver table.
 */
  int i;

  for (i = 0; i < NR_DEVICES; i++)
	driver_endpt[i] = NONE;
}

void bdev_driver_clear(dev_t dev)
{
/* Clear information about a driver.
 */
  int major;

  major = major(dev);

  assert(major >= 0 && major < NR_DEVICES);

  driver_endpt[major] = NONE;
}

void bdev_driver_set(dev_t dev, endpoint_t endpt)
{
/* Set the endpoint for a driver.
 */
  int major;

  major = major(dev);

  assert(major >= 0 && major < NR_DEVICES);

  driver_endpt[major] = endpt;
}

endpoint_t bdev_driver_get(dev_t dev)
{
/* Return the endpoint for a driver, or NONE if we do not know its endpoint.
 */
  int major;

  major = major(dev);

  assert(major >= 0 && major < NR_DEVICES);

  return driver_endpt[major];
}
