/* libbdev - driver endpoint management */

#include <minix/drivers.h>
#include <minix/bdev.h>
#include <minix/ds.h>
#include <assert.h>

#include "const.h"
#include "type.h"
#include "proto.h"

static struct {
  endpoint_t endpt;
  char label[DS_MAX_KEYLEN];
} driver_tab[NR_DEVICES];

void bdev_driver_init(void)
{
/* Initialize the driver table.
 */
  int i;

  for (i = 0; i < NR_DEVICES; i++) {
	driver_tab[i].endpt = NONE;
	driver_tab[i].label[0] = '\0';
  }
}

void bdev_driver_clear(dev_t dev)
{
/* Clear information about a driver.
 */
  int major;

  major = major(dev);

  assert(major >= 0 && major < NR_DEVICES);

  driver_tab[major].endpt = NONE;
  driver_tab[major].label[0] = '\0';
}

endpoint_t bdev_driver_set(dev_t dev, char *label)
{
/* Set the label for a driver, and retrieve the associated endpoint.
 */
  int major;

  major = major(dev);

  assert(major >= 0 && major < NR_DEVICES);
  assert(strlen(label) < sizeof(driver_tab[major].label));

  strlcpy(driver_tab[major].label, label, sizeof(driver_tab[major].label));

  driver_tab[major].endpt = NONE;

  return bdev_driver_update(dev);
}

endpoint_t bdev_driver_get(dev_t dev)
{
/* Return the endpoint for a driver, or NONE if we do not know its endpoint.
 */
  int major;

  major = major(dev);

  assert(major >= 0 && major < NR_DEVICES);

  return driver_tab[major].endpt;
}

endpoint_t bdev_driver_update(dev_t dev)
{
/* Update the endpoint of a driver. The caller of this function already knows
 * that the current endpoint may no longer be valid, and must be updated.
 * Return the new endpoint upon success, and NONE otherwise.
 */
  endpoint_t endpt;
  int r, major, nr_tries;

  major = major(dev);

  assert(major >= 0 && major < NR_DEVICES);
  assert(driver_tab[major].label[0] != '\0');

  /* Repeatedly retrieve the endpoint for the driver label, and see if it is a
   * different, valid endpoint. If retrieval fails at first, we have to wait.
   * We use polling, as opposed to a DS subscription, for a number of reasons:
   * 1) DS supports only one subscription per process, and our main program may
   *    already have a subscription;
   * 2) if we block on receiving a notification from DS, we cannot impose an
   *    upper bound on the retry time;
   * 3) temporarily subscribing and then unsubscribing may cause leftover DS
   *    notifications, which the main program would then have to deal with.
   *    As of writing, unsubscribing from DS is not possible at all, anyway.
   *
   * In the normal case, the driver's label/endpoint mapping entry disappears
   * completely for a short moment, before being replaced with the new mapping.
   * Hence, failure to retrieve the entry at all does not constitute permanent
   * failure. In fact, there is no way to determine reliably that a driver has
   * failed permanently in the current approach. For this we simply rely on the
   * retry limit.
   */
  for (nr_tries = 0; nr_tries < DS_NR_TRIES; nr_tries++) {
	r = ds_retrieve_label_endpt(driver_tab[major].label, &endpt);

	if (r == OK && endpt != NONE && endpt != driver_tab[major].endpt) {
		driver_tab[major].endpt = endpt;

		return endpt;
	}

	if (nr_tries < DS_NR_TRIES - 1)
		micro_delay(DS_DELAY);
  }

  driver_tab[major].endpt = NONE;

  return NONE;
}
