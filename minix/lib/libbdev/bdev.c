/* libbdev - block device interfacing library, by D.C. van Moolenbroek */

#include <minix/drivers.h>
#include <minix/bdev.h>
#include <minix/ioctl.h>
#include <assert.h>

#include "const.h"
#include "type.h"
#include "proto.h"

void bdev_driver(dev_t dev, char *label)
{
/* Associate a driver with the given (major) device, using its endpoint.
 * File system usage note: typically called from mount and newdriver.
 */
  static int first = TRUE;

  if (first) {
	/* Initialize the driver endpoint array. */
	bdev_driver_init();

	first = FALSE;
  }

  bdev_update(dev, label);
}

static int bdev_retry(int *driver_tries, int *transfer_tries, int *result)
{
/* Return TRUE iff the call result implies that we should retry the operation.
 */

  switch (*result) {
  case ERESTART:
	/* We get this error internally if the driver has restarted and the
	 * current operation may now go through. Check the retry count for
	 * driver restarts first, as we don't want to keep trying forever.
	 */
	if (++*driver_tries < DRIVER_TRIES)
		return TRUE;

	*result = EDEADSRCDST;

	break;

  case EIO:
	/* The 'transfer_tries' pointer is non-NULL if this was a transfer
	 * request. If we get back an I/O failure, keep retrying the request
	 * until we hit the transfer retry limit.
	 */
	if (transfer_tries != NULL && ++*transfer_tries < TRANSFER_TRIES)
		return TRUE;

	break;
  }

  return FALSE;
}

static int bdev_opcl(int req, dev_t dev, int access)
{
/* Open or close the given minor device.
 */
  message m;
  int r, driver_tries = 0;

  do {
	memset(&m, 0, sizeof(m));
	m.m_type = req;
	m.m_lbdev_lblockdriver_msg.minor = minor(dev);
	m.m_lbdev_lblockdriver_msg.access = access;

	r = bdev_sendrec(dev, &m);
  } while (bdev_retry(&driver_tries, NULL, &r));

  return r;
}

int bdev_open(dev_t dev, int access)
{
/* Open the given minor device.
 * File system usage note: typically called from mount, after bdev_driver.
 */
  int r;

  r = bdev_opcl(BDEV_OPEN, dev, access);

  if (r == OK)
	bdev_minor_add(dev, access);

  return r;
}

int bdev_close(dev_t dev)
{
/* Close the given minor device.
 * File system usage note: typically called from unmount.
 */
  int r;

  bdev_flush_asyn(dev);

  r = bdev_opcl(BDEV_CLOSE, dev, 0);

  if (r == OK)
	bdev_minor_del(dev);

  return r;
}

static int bdev_rdwt_setup(int req, dev_t dev, u64_t pos, char *buf,
  size_t count, int flags, message *m)
{
/* Set up a single-buffer read/write request.
 */
  endpoint_t endpt;
  cp_grant_id_t grant;
  int access;

  assert((ssize_t) count >= 0);

  if ((endpt = bdev_driver_get(dev)) == NONE)
	return EDEADSRCDST;

  access = (req == BDEV_READ) ? CPF_WRITE : CPF_READ;

  grant = cpf_grant_direct(endpt, (vir_bytes) buf, count, access);

  if (!GRANT_VALID(grant)) {
	printf("bdev: unable to allocate grant!\n");
	return EINVAL;
  }

  memset(m, 0, sizeof(*m));
  m->m_type = req;
  m->m_lbdev_lblockdriver_msg.minor = minor(dev);
  m->m_lbdev_lblockdriver_msg.pos = pos;
  m->m_lbdev_lblockdriver_msg.count = count;
  m->m_lbdev_lblockdriver_msg.grant = grant;
  m->m_lbdev_lblockdriver_msg.flags = flags;

  return OK;
}

static void bdev_rdwt_cleanup(const message *m)
{
/* Clean up a single-buffer read/write request.
 */

  cpf_revoke(m->m_lbdev_lblockdriver_msg.grant);
}

static ssize_t bdev_rdwt(int req, dev_t dev, u64_t pos, char *buf,
  size_t count, int flags)
{
/* Perform a synchronous read or write call using a single buffer.
 */
  message m;
  int r, driver_tries = 0, transfer_tries = 0;

  do {
	if ((r = bdev_rdwt_setup(req, dev, pos, buf, count, flags, &m)) != OK)
		break;

	r = bdev_sendrec(dev, &m);

	bdev_rdwt_cleanup(&m);
  } while (bdev_retry(&driver_tries, &transfer_tries, &r));

  return r;
}

static int bdev_vrdwt_setup(int req, dev_t dev, u64_t pos, iovec_t *vec,
  int count, int flags, message *m, iovec_s_t *gvec)
{
/* Set up a vectored read/write request.
 */
  ssize_t size;
  endpoint_t endpt;
  cp_grant_id_t grant;
  int i, access;

  assert(count <= NR_IOREQS);

  if ((endpt = bdev_driver_get(dev)) == NONE)
	return EDEADSRCDST;

  access = (req == BDEV_GATHER) ? CPF_WRITE : CPF_READ;
  size = 0;

  for (i = 0; i < count; i++) {
	grant = cpf_grant_direct(endpt, vec[i].iov_addr, vec[i].iov_size,
		access);

	if (!GRANT_VALID(grant)) {
		printf("bdev: unable to allocate grant!\n");

		for (i--; i >= 0; i--)
			cpf_revoke(gvec[i].iov_grant);

		return EINVAL;
	}

	gvec[i].iov_grant = grant;
	gvec[i].iov_size = vec[i].iov_size;

	assert(vec[i].iov_size > 0);
	assert((ssize_t) (size + vec[i].iov_size) > size);

	size += vec[i].iov_size;
  }

  grant = cpf_grant_direct(endpt, (vir_bytes) gvec, sizeof(gvec[0]) * count,
	CPF_READ);

  if (!GRANT_VALID(grant)) {
	printf("bdev: unable to allocate grant!\n");

	for (i = count - 1; i >= 0; i--)
		cpf_revoke(gvec[i].iov_grant);

	return EINVAL;
  }

  memset(m, 0, sizeof(*m));
  m->m_type = req;
  m->m_lbdev_lblockdriver_msg.minor = minor(dev);
  m->m_lbdev_lblockdriver_msg.pos = pos;
  m->m_lbdev_lblockdriver_msg.count = count;
  m->m_lbdev_lblockdriver_msg.grant = grant;
  m->m_lbdev_lblockdriver_msg.flags = flags;

  return OK;
}

static void bdev_vrdwt_cleanup(const message *m, iovec_s_t *gvec)
{
/* Clean up a vectored read/write request.
 */
  cp_grant_id_t grant;
  int i;

  grant = m->m_lbdev_lblockdriver_msg.grant;

  cpf_revoke(grant);

  for (i = m->m_lbdev_lblockdriver_msg.count - 1; i >= 0; i--)
	cpf_revoke(gvec[i].iov_grant);
}

static ssize_t bdev_vrdwt(int req, dev_t dev, u64_t pos, iovec_t *vec,
  int count, int flags)
{
/* Perform a synchronous read or write call using a vector of buffers.
 */
  iovec_s_t gvec[NR_IOREQS];
  message m;
  int r, driver_tries = 0, transfer_tries = 0;

  do {
	if ((r = bdev_vrdwt_setup(req, dev, pos, vec, count, flags, &m,
			gvec)) != OK)
		break;

	r = bdev_sendrec(dev, &m);

	bdev_vrdwt_cleanup(&m, gvec);
  } while (bdev_retry(&driver_tries, &transfer_tries, &r));

  return r;
}

ssize_t bdev_read(dev_t dev, u64_t pos, char *buf, size_t count, int flags)
{
/* Perform a synchronous read call into a single buffer.
 */

  return bdev_rdwt(BDEV_READ, dev, pos, buf, count, flags);
}

ssize_t bdev_write(dev_t dev, u64_t pos, char *buf, size_t count, int flags)
{
/* Perform a synchronous write call from a single buffer.
 */

  return bdev_rdwt(BDEV_WRITE, dev, pos, buf, count, flags);
}

ssize_t bdev_gather(dev_t dev, u64_t pos, iovec_t *vec, int count, int flags)
{
/* Perform a synchronous read call into a vector of buffers.
 */

  return bdev_vrdwt(BDEV_GATHER, dev, pos, vec, count, flags);
}

ssize_t bdev_scatter(dev_t dev, u64_t pos, iovec_t *vec, int count, int flags)
{
/* Perform a synchronous write call from a vector of buffers.
 */

  return bdev_vrdwt(BDEV_SCATTER, dev, pos, vec, count, flags);
}

static int bdev_ioctl_setup(dev_t dev, int request, void *buf,
  endpoint_t user_endpt, message *m)
{
/* Set up an I/O control request.
 */
  endpoint_t endpt;
  size_t size;
  cp_grant_id_t grant;
  int access;

  if ((endpt = bdev_driver_get(dev)) == NONE)
	return EDEADSRCDST;

  if (_MINIX_IOCTL_BIG(request))
	size = _MINIX_IOCTL_SIZE_BIG(request);
  else
	size = _MINIX_IOCTL_SIZE(request);

  access = 0;
  if (_MINIX_IOCTL_IOR(request)) access |= CPF_WRITE;
  if (_MINIX_IOCTL_IOW(request)) access |= CPF_READ;

  /* The size may be 0, in which case 'buf' need not be a valid pointer. */
  grant = cpf_grant_direct(endpt, (vir_bytes) buf, size, access);

  if (!GRANT_VALID(grant)) {
	printf("bdev: unable to allocate grant!\n");
	return EINVAL;
  }

  memset(m, 0, sizeof(*m));
  m->m_type = BDEV_IOCTL;
  m->m_lbdev_lblockdriver_msg.minor = minor(dev);
  m->m_lbdev_lblockdriver_msg.request = request;
  m->m_lbdev_lblockdriver_msg.grant = grant;
  m->m_lbdev_lblockdriver_msg.user = user_endpt;

  return OK;
}

static void bdev_ioctl_cleanup(const message *m)
{
/* Clean up an I/O control request.
 */

  cpf_revoke(m->m_lbdev_lblockdriver_msg.grant);
}

int bdev_ioctl(dev_t dev, int request, void *buf, endpoint_t user_endpt)
{
/* Perform a synchronous I/O control request.
 */
  message m;
  int r, driver_tries = 0;

  do {
	if ((r = bdev_ioctl_setup(dev, request, buf, user_endpt, &m)) != OK)
		break;

	r = bdev_sendrec(dev, &m);

	bdev_ioctl_cleanup(&m);
  } while (bdev_retry(&driver_tries, NULL, &r));

  return r;
}

void bdev_flush_asyn(dev_t dev)
{
/* Flush all ongoing asynchronous requests to the given minor device. This
 * involves blocking until all I/O for it has completed.
 * File system usage note: typically called from flush.
 */
  bdev_call_t *call;

  while ((call = bdev_call_find(dev)) != NULL)
	(void) bdev_wait_asyn(call->id);
}

static bdev_id_t bdev_rdwt_asyn(int req, dev_t dev, u64_t pos, char *buf,
	size_t count, int flags, bdev_callback_t callback, bdev_param_t param)
{
/* Perform an asynchronous read or write call using a single buffer.
 */
  bdev_call_t *call;
  int r;

  if ((call = bdev_call_alloc(1)) == NULL)
	return ENOMEM;

  if ((r = bdev_rdwt_setup(req, dev, pos, buf, count, flags, &call->msg)) !=
		OK) {
	bdev_call_free(call);

	return r;
  }

  if ((r = bdev_senda(dev, &call->msg, call->id)) != OK) {
	bdev_rdwt_cleanup(&call->msg);

	bdev_call_free(call);

	return r;
  }

  call->dev = dev;
  call->callback = callback;
  call->param = param;
  call->driver_tries = 0;
  call->transfer_tries = 0;
  call->vec[0].iov_addr = (vir_bytes) buf;
  call->vec[0].iov_size = count;

  return call->id;
}

static bdev_id_t bdev_vrdwt_asyn(int req, dev_t dev, u64_t pos, iovec_t *vec,
	int count, int flags, bdev_callback_t callback, bdev_param_t param)
{
/* Perform an asynchronous read or write call using a vector of buffers.
 */
  bdev_call_t *call;
  int r;

  if ((call = bdev_call_alloc(count)) == NULL)
	return ENOMEM;

  if ((r = bdev_vrdwt_setup(req, dev, pos, vec, count, flags, &call->msg,
		call->gvec)) != OK) {
	bdev_call_free(call);

	return r;
  }

  if ((r = bdev_senda(dev, &call->msg, call->id)) != OK) {
	bdev_vrdwt_cleanup(&call->msg, call->gvec);

	bdev_call_free(call);

	return r;
  }

  call->dev = dev;
  call->callback = callback;
  call->param = param;
  call->driver_tries = 0;
  call->transfer_tries = 0;
  memcpy(call->vec, vec, sizeof(vec[0]) * count);

  return call->id;
}

bdev_id_t bdev_read_asyn(dev_t dev, u64_t pos, char *buf, size_t count,
	int flags, bdev_callback_t callback, bdev_param_t param)
{
/* Perform an asynchronous read call into a single buffer.
 */

  return bdev_rdwt_asyn(BDEV_READ, dev, pos, buf, count, flags, callback,
	param);
}

bdev_id_t bdev_write_asyn(dev_t dev, u64_t pos, char *buf, size_t count,
	int flags, bdev_callback_t callback, bdev_param_t param)
{
/* Perform an asynchronous write call from a single buffer.
 */

  return bdev_rdwt_asyn(BDEV_WRITE, dev, pos, buf, count, flags, callback,
	param);
}

bdev_id_t bdev_gather_asyn(dev_t dev, u64_t pos, iovec_t *vec, int count,
	int flags, bdev_callback_t callback, bdev_param_t param)
{
/* Perform an asynchronous read call into a vector of buffers.
 */

  return bdev_vrdwt_asyn(BDEV_GATHER, dev, pos, vec, count, flags, callback,
	param);
}

bdev_id_t bdev_scatter_asyn(dev_t dev, u64_t pos, iovec_t *vec, int count,
	int flags, bdev_callback_t callback, bdev_param_t param)
{
/* Perform an asynchronous write call into a vector of buffers.
 */

  return bdev_vrdwt_asyn(BDEV_SCATTER, dev, pos, vec, count, flags, callback,
	param);
}

bdev_id_t bdev_ioctl_asyn(dev_t dev, int request, void *buf,
	endpoint_t user_endpt, bdev_callback_t callback, bdev_param_t param)
{
/* Perform an asynchronous I/O control request.
 */
  bdev_call_t *call;
  int r;

  if ((call = bdev_call_alloc(1)) == NULL)
	return ENOMEM;

  if ((r = bdev_ioctl_setup(dev, request, buf, user_endpt,
		&call->msg)) != OK) {
	bdev_call_free(call);

	return r;
  }

  if ((r = bdev_senda(dev, &call->msg, call->id)) != OK) {
	bdev_ioctl_cleanup(&call->msg);

	bdev_call_free(call);

	return r;
  }

  call->dev = dev;
  call->callback = callback;
  call->param = param;
  call->driver_tries = 0;
  call->vec[0].iov_addr = (vir_bytes) buf;

  return call->id;
}

void bdev_callback_asyn(bdev_call_t *call, int result)
{
/* Perform the callback for an asynchronous request, with the given result.
 * Clean up the call structure afterwards.
 */

  /* If this was a transfer request and the result is EIO, we may want to retry
   * the request first.
   */
  switch (call->msg.m_type) {
  case BDEV_READ:
  case BDEV_WRITE:
  case BDEV_GATHER:
  case BDEV_SCATTER:
	if (result == EIO && ++call->transfer_tries < TRANSFER_TRIES) {
		result = bdev_senda(call->dev, &call->msg, call->id);

		if (result == OK)
			return;
	}
  }

  /* Clean up. */
  switch (call->msg.m_type) {
  case BDEV_READ:
  case BDEV_WRITE:
	bdev_rdwt_cleanup(&call->msg);

	break;

  case BDEV_GATHER:
  case BDEV_SCATTER:
	bdev_vrdwt_cleanup(&call->msg, call->gvec);

	break;

  case BDEV_IOCTL:
	bdev_ioctl_cleanup(&call->msg);

	break;

  default:
	assert(0);
  }

  /* Call the callback function. */
  /* FIXME: we assume all reasonable ssize_t values can be stored in an int. */
  call->callback(call->dev, call->id, call->param, result);

  /* Free up the call structure. */
  bdev_call_free(call);
}

int bdev_restart_asyn(bdev_call_t *call)
{
/* The driver for the given call has restarted, and may now have a new
 * endpoint. Recreate and resend the request for the given call.
 */
  int type, r = OK;

  /* Update and check the retry limit for driver restarts first. */
  if (++call->driver_tries >= DRIVER_TRIES)
	return EDEADSRCDST;

  /* Recreate all grants for the new endpoint. */
  type = call->msg.m_type;

  switch (type) {
  case BDEV_READ:
  case BDEV_WRITE:
	bdev_rdwt_cleanup(&call->msg);

	r = bdev_rdwt_setup(type, call->dev,
		call->msg.m_lbdev_lblockdriver_msg.pos,
		(char *) call->vec[0].iov_addr, call->msg.m_lbdev_lblockdriver_msg.count,
		call->msg.m_lbdev_lblockdriver_msg.flags, &call->msg);

	break;

  case BDEV_GATHER:
  case BDEV_SCATTER:
	bdev_vrdwt_cleanup(&call->msg, call->gvec);

	r = bdev_vrdwt_setup(type, call->dev,
		call->msg.m_lbdev_lblockdriver_msg.pos,
		call->vec, call->msg.m_lbdev_lblockdriver_msg.count, call->msg.m_lbdev_lblockdriver_msg.flags,
		&call->msg, call->gvec);

	break;

  case BDEV_IOCTL:
	bdev_ioctl_cleanup(&call->msg);

	r = bdev_ioctl_setup(call->dev, call->msg.m_lbdev_lblockdriver_msg.request,
		(char *) call->vec[0].iov_addr, call->msg.m_lbdev_lblockdriver_msg.user,
		&call->msg);

	break;

  default:
	assert(0);
  }

  if (r != OK)
	return r;

  /* Try to resend the request. */
  return bdev_senda(call->dev, &call->msg, call->id);
}
