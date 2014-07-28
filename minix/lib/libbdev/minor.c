/* libbdev - tracking and reopening of opened minor devices */

#include <minix/drivers.h>
#include <minix/bdev.h>
#include <assert.h>

#include "const.h"
#include "type.h"
#include "proto.h"

static struct {
  dev_t dev;
  int count;
  int access;
} open_dev[NR_OPEN_DEVS] = { { NO_DEV, 0, 0 } };

int bdev_minor_reopen(dev_t dev)
{
/* Reopen all minor devices on a major device. This function duplicates some
 * code from elsewhere, because in this case we must avoid performing recovery.
 * FIXME: if reopening fails with a non-IPC error, we should attempt to close
 * all minors that we did manage to reopen so far, or they might stay open
 * forever.
 */
  endpoint_t endpt;
  message m;
  int i, j, r, major;

  major = major(dev);
  endpt = bdev_driver_get(dev);

  assert(endpt != NONE);

  for (i = 0; i < NR_OPEN_DEVS; i++) {
	if (major(open_dev[i].dev) != major)
		continue;

	/* Each minor device may have been opened multiple times. Send an open
	 * request for each time that it was opened before. We could reopen it
	 * just once, but then we'd have to keep a shadow open count as well.
	 */
	for (j = 0; j < open_dev[i].count; j++) {
		memset(&m, 0, sizeof(m));
		m.m_type = BDEV_OPEN;
		m.m_lbdev_lblockdriver_msg.minor = minor(open_dev[i].dev);
		m.m_lbdev_lblockdriver_msg.access = open_dev[i].access;
		m.m_lbdev_lblockdriver_msg.id = NO_ID;

		if ((r = ipc_sendrec(endpt, &m)) != OK) {
			printf("bdev: IPC to driver (%d) failed (%d)\n",
				endpt, r);
			return r;
		}

		if (m.m_type != BDEV_REPLY) {
			printf("bdev: driver (%d) sent weird response (%d)\n",
				endpt, m.m_type);
			return EINVAL;
		}

		if (m.m_lblockdriver_lbdev_reply.id != NO_ID) {
			printf("bdev: driver (%d) sent invalid ID (%ld)\n",
				endpt, m.m_lblockdriver_lbdev_reply.id);
			return EINVAL;
		}

		if ((r = m.m_lblockdriver_lbdev_reply.status) != OK) {
			printf("bdev: driver (%d) failed device reopen (%d)\n",
				endpt, r);
			return r;
		}
	}
  }

  return OK;
}

void bdev_minor_add(dev_t dev, int access)
{
/* Increase the reference count of the given minor device.
 */
  int i, free = -1;

  for (i = 0; i < NR_OPEN_DEVS; i++) {
	if (open_dev[i].dev == dev) {
		open_dev[i].count++;
		open_dev[i].access |= access;

		return;
	}

	if (free < 0 && open_dev[i].dev == NO_DEV)
		free = i;
  }

  if (free < 0) {
	printf("bdev: too many open devices, increase NR_OPEN_DEVS\n");
	return;
  }

  open_dev[free].dev = dev;
  open_dev[free].count = 1;
  open_dev[free].access = access;
}

void bdev_minor_del(dev_t dev)
{
/* Decrease the reference count of the given minor device, if present.
 */
  int i;

  for (i = 0; i < NR_OPEN_DEVS; i++) {
	if (open_dev[i].dev == dev) {
		if (!--open_dev[i].count)
			open_dev[i].dev = NO_DEV;

		break;
	}
  }
}

int bdev_minor_is_open(dev_t dev)
{
/* Return whether any minor is open for the major of the given device.
 */
  int i, major;

  major = major(dev);

  for (i = 0; i < NR_OPEN_DEVS; i++) {
	if (major(open_dev[i].dev) == major)
		return TRUE;
  }

  return FALSE;
}
