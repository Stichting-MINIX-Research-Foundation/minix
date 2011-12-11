/* libbdev - IPC and recovery functions */

#include <minix/drivers.h>
#include <minix/bdev.h>
#include <assert.h>

#include "const.h"
#include "type.h"
#include "proto.h"

static void bdev_cancel(dev_t dev)
{
/* Recovering the driver for the given device has failed repeatedly. Mark it as
 * permanently unusable, and clean up any associated calls and resources.
 */
  bdev_call_t *call, *next;

  printf("bdev: giving up on major %d\n", major(dev));

  /* Cancel all pending asynchronous requests. */
  call = NULL;

  while ((call = bdev_call_iter_maj(dev, call, &next)) != NULL)
	bdev_callback_asyn(call, EDEADSRCDST);

  /* Mark the driver as unusable. */
  bdev_driver_clear(dev);
}

static int bdev_recover(dev_t dev, int update_endpt)
{
/* The IPC subsystem has signaled an error communicating to the driver
 * associated with the given device. Try to recover. If 'update_endpt' is set,
 * we need to find the new endpoint of the driver first. Return TRUE iff
 * recovery has been successful.
 */
  bdev_call_t *call, *next;
  endpoint_t endpt;
  int r, nr_tries;

  printf("bdev: recovering from a driver restart on major %d\n", major(dev));

  for (nr_tries = 0; nr_tries < RECOVER_TRIES; nr_tries++) {
	/* First update the endpoint, if necessary. */
	if (update_endpt)
		(void) bdev_driver_update(dev);

	if ((endpt = bdev_driver_get(dev)) == NONE)
		break;

	/* If anything goes wrong, update the endpoint again next time. */
	update_endpt = TRUE;

	/* Reopen all minor devices on the new driver. */
	if ((r = bdev_minor_reopen(dev)) != OK) {
		/* If the driver died again, we may give it another try. */
		if (r == EDEADSRCDST)
			continue;

		/* If another error occurred, we cannot continue using the
		 * driver as is, but we also cannot force it to restart.
		 */
		break;
	}

	/* Resend all asynchronous requests. */
	call = NULL;

	while ((call = bdev_call_iter_maj(dev, call, &next)) != NULL) {
		/* It is not strictly necessary that we manage to reissue all
		 * asynchronous requests successfully. We can fail them on an
		 * individual basis here, without affecting the overall
		 * recovery. Note that we will never get new IPC failures here.
		 */
		if ((r = bdev_restart_asyn(call)) != OK)
			bdev_callback_asyn(call, r);
	}

	/* Recovery seems successful. We can now reissue the current
	 * synchronous request (if any), and continue normal operation.
	 */
	printf("bdev: recovery successful, new driver is at %d\n", endpt);

	return TRUE;
  }

  /* Recovery failed repeatedly. Give up on this driver. */
  bdev_cancel(dev);

  return FALSE;
}

void bdev_update(dev_t dev, char *label)
{
/* Set the endpoint for a driver. Perform recovery if necessary.
 */
  endpoint_t endpt, old_endpt;

  old_endpt = bdev_driver_get(dev);

  endpt = bdev_driver_set(dev, label);

  /* If updating the driver causes an endpoint change, we need to perform
   * recovery, but not update the endpoint yet again.
   */
  if (old_endpt != NONE && old_endpt != endpt)
	bdev_recover(dev, FALSE /*update_endpt*/);
}

int bdev_senda(dev_t dev, const message *m_orig, bdev_id_t id)
{
/* Send an asynchronous request for the given device. This function will never
 * get any new IPC errors sending to the driver. If sending an asynchronous
 * request fails, we will find out through other ways later.
 */
  endpoint_t endpt;
  message m;
  int r;

  /* If we have no usable driver endpoint, fail instantly. */
  if ((endpt = bdev_driver_get(dev)) == NONE)
	return EDEADSRCDST;

  m = *m_orig;
  m.BDEV_ID = id;

  r = asynsend(endpt, &m);

  if (r != OK)
	printf("bdev: asynsend to driver (%d) failed (%d)\n", endpt, r);

  return r;
}

int bdev_sendrec(dev_t dev, const message *m_orig)
{
/* Send a synchronous request for the given device, and wait for the reply.
 * Return ERESTART if the caller should try to reissue the request.
 */
  endpoint_t endpt;
  message m;
  int r;

  /* If we have no usable driver endpoint, fail instantly. */
  if ((endpt = bdev_driver_get(dev)) == NONE)
	return EDEADSRCDST;

  /* Send the request and block until we receive a reply. */
  m = *m_orig;
  m.BDEV_ID = NO_ID;

  r = sendrec(endpt, &m);

  /* If communication failed, the driver has died. We assume it will be
   * restarted soon after, so we attempt recovery. Upon success, we let the
   * caller reissue the synchronous request.
   */
  if (r == EDEADSRCDST) {
	if (!bdev_recover(dev, TRUE /*update_endpt*/))
		return EDEADSRCDST;

	return ERESTART;
  }

  if (r != OK) {
	printf("bdev: IPC to driver (%d) failed (%d)\n", endpt, r);
	return r;
  }

  if (m.m_type != BDEV_REPLY) {
	printf("bdev: driver (%d) sent weird response (%d)\n",
		endpt, m.m_type);
	return EINVAL;
  }

  /* The protocol contract states that no asynchronous reply can satisfy a
   * synchronous SENDREC call, so we can never get an asynchronous reply here.
   */
  if (m.BDEV_ID != NO_ID) {
	printf("bdev: driver (%d) sent invalid ID (%ld)\n", endpt, m.BDEV_ID);
	return EINVAL;
  }

  /* Unless the caller is misusing libbdev, we will only get ERESTART if we
   * have managed to resend a raw block I/O request to the driver after a
   * restart, but before VFS has had a chance to reopen the associated device
   * first. This is highly exceptional, and hard to deal with correctly. We
   * take the easiest route: sleep for a while so that VFS can reopen the
   * device, and then resend the request. If the call keeps failing, the caller
   * will eventually give up.
   */
  if (m.BDEV_STATUS == ERESTART) {
	printf("bdev: got ERESTART from driver (%d), sleeping for reopen\n",
		endpt);

	micro_delay(1000);

	return ERESTART;
  }

  /* Return the result of our request. */
  return m.BDEV_STATUS;
}

static int bdev_receive(dev_t dev, message *m)
{
/* Receive one valid message.
 */
  endpoint_t endpt;
  int r, nr_tries = 0;

  for (;;) {
	/* Retrieve and check the driver endpoint on every try, as it will
	 * change with each driver restart.
	 */
	if ((endpt = bdev_driver_get(dev)) == NONE)
		return EDEADSRCDST;

	r = sef_receive(endpt, m);

	if (r == EDEADSRCDST) {
		/* If we reached the maximum number of retries, give up. */
		if (++nr_tries == DRIVER_TRIES)
			break;

		/* Attempt recovery. If successful, all asynchronous requests
		 * will have been resent, and we can retry receiving a reply.
		 */
		if (!bdev_recover(dev, TRUE /*update_endpt*/))
			return EDEADSRCDST;

		continue;
	}

	if (r != OK) {
		printf("bdev: IPC to driver (%d) failed (%d)\n", endpt, r);

		return r;
	}

	if (m->m_type != BDEV_REPLY) {
		printf("bdev: driver (%d) sent weird response (%d)\n",
			endpt, m->m_type);
		return EINVAL;
	}

	/* The caller is responsible for checking the ID and status. */
	return OK;
  }

  /* All tries failed, even though all recovery attempts succeeded. In this
   * case, we let the caller recheck whether it wants to keep calling us,
   * returning ERESTART to indicate we can be called again but did not actually
   * receive a message.
   */
  return ERESTART;
}

void bdev_reply_asyn(message *m)
{
/* A reply has come in from a disk driver.
 */
  bdev_call_t *call;
  endpoint_t endpt;
  bdev_id_t id;
  int r;

  /* This is a requirement for the caller. */
  assert(m->m_type == BDEV_REPLY);

  /* Get the corresponding asynchronous call structure. */
  id = m->BDEV_ID;

  if ((call = bdev_call_get(id)) == NULL) {
	printf("bdev: driver (%d) replied to unknown request (%ld)\n",
		m->m_source, m->BDEV_ID);
	return;
  }

  /* Make sure the reply was sent from the right endpoint. */
  endpt = bdev_driver_get(call->dev);

  if (m->m_source != endpt) {
	/* If the endpoint is NONE, this may be a stray reply. */
	if (endpt != NONE)
		printf("bdev: driver (%d) replied to request not sent to it\n",
			m->m_source);
	return;
  }

  /* See the ERESTART comment in bdev_sendrec(). */
  if (m->BDEV_STATUS == ERESTART) {
	printf("bdev: got ERESTART from driver (%d), sleeping for reopen\n",
		endpt);

	micro_delay(1000);

	if ((r = bdev_restart_asyn(call)) != OK)
		bdev_callback_asyn(call, r);

	return;
  }

  bdev_callback_asyn(call, m->BDEV_STATUS);
}

int bdev_wait_asyn(bdev_id_t id)
{
/* Wait for an asynchronous request to complete.
 */
  bdev_call_t *call;
  dev_t dev;
  message m;
  int r;

  if ((call = bdev_call_get(id)) == NULL)
	return ENOENT;

  dev = call->dev;

  do {
	if ((r = bdev_receive(dev, &m)) != OK && r != ERESTART)
		return r;

	/* Processing the reply will free up the call structure as a side
	 * effect. If we repeatedly get ERESTART, we will repeatedly resend the
	 * asynchronous request, which will then eventually hit the retry limit
	 * and we will break out of the loop.
	 */
	if (r == OK)
		bdev_reply_asyn(&m);

  } while (bdev_call_get(id) != NULL);

  return OK;
}
