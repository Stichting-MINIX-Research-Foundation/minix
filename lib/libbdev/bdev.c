/* libbdev - block device interfacing library, by D.C. van Moolenbroek */

/* This is a preliminary, bare-essentials-only version of this library. */

#include <minix/drivers.h>
#include <minix/bdev.h>
#include <minix/ioctl.h>
#include <assert.h>

#include "proto.h"

void bdev_driver(dev_t dev, endpoint_t endpt)
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

  bdev_update(dev, endpt);
}

static int bdev_opcl(int req, dev_t dev, int access)
{
/* Open or close the given minor device.
 */
  message m;

  memset(&m, 0, sizeof(m));
  m.m_type = req;
  m.BDEV_MINOR = minor(dev);
  m.BDEV_ACCESS = access;

  return bdev_sendrec(dev, &m);
}

int bdev_open(dev_t dev, int access)
{
/* Open the given minor device.
 * File system usage note: typically called from mount, after bdev_driver.
 */

  return bdev_opcl(BDEV_OPEN, dev, access);
}

int bdev_close(dev_t dev)
{
/* Close the given minor device.
 * File system usage note: typically called from unmount.
 */

  return bdev_opcl(BDEV_CLOSE, dev, 0);
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
  m->BDEV_MINOR = minor(dev);
  m->BDEV_POS_LO = ex64lo(pos);
  m->BDEV_POS_HI = ex64hi(pos);
  m->BDEV_COUNT = count;
  m->BDEV_GRANT = grant;
  m->BDEV_FLAGS = flags;

  return OK;
}

static void bdev_rdwt_cleanup(message *m)
{
/* Clean up a single-buffer read/write request.
 */

  cpf_revoke(m->BDEV_GRANT);
}

static ssize_t bdev_rdwt(int req, dev_t dev, u64_t pos, char *buf,
  size_t count, int flags)
{
/* Perform a read or write call using a single buffer.
 */
  message m;
  int r;

  if ((r = bdev_rdwt_setup(req, dev, pos, buf, count, flags, &m)) != OK)
	return r;

  r = bdev_sendrec(dev, &m);

  bdev_rdwt_cleanup(&m);

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
  m->BDEV_MINOR = minor(dev);
  m->BDEV_POS_LO = ex64lo(pos);
  m->BDEV_POS_HI = ex64hi(pos);
  m->BDEV_COUNT = count;
  m->BDEV_GRANT = grant;
  m->BDEV_FLAGS = flags;

  return OK;
}

static void bdev_vrdwt_cleanup(message *m, iovec_s_t *gvec)
{
/* Clean up a vectored read/write request.
 */
  cp_grant_id_t grant;
  int i;

  grant = m->BDEV_GRANT;

  cpf_revoke(grant);

  for (i = m->BDEV_COUNT - 1; i >= 0; i--)
	cpf_revoke(gvec[i].iov_grant);
}

static ssize_t bdev_vrdwt(int req, dev_t dev, u64_t pos, iovec_t *vec,
  int count, int flags)
{
/* Perform a read or write call using a vector of buffers.
 */
  iovec_s_t gvec[NR_IOREQS];
  message m;
  int r;

  if ((r = bdev_vrdwt_setup(req, dev, pos, vec, count, flags, &m, gvec)) != OK)
	return r;

  r = bdev_sendrec(dev, &m);

  bdev_vrdwt_cleanup(&m, gvec);

  return r;
}

ssize_t bdev_read(dev_t dev, u64_t pos, char *buf, size_t count, int flags)
{
/* Perform a read call into a single buffer.
 */

  return bdev_rdwt(BDEV_READ, dev, pos, buf, count, flags);
}

ssize_t bdev_write(dev_t dev, u64_t pos, char *buf, size_t count, int flags)
{
/* Perform a write call from a single buffer.
 */

  return bdev_rdwt(BDEV_WRITE, dev, pos, buf, count, flags);
}

ssize_t bdev_gather(dev_t dev, u64_t pos, iovec_t *vec, int count, int flags)
{
/* Perform a read call into a vector of buffers.
 */

  return bdev_vrdwt(BDEV_GATHER, dev, pos, vec, count, flags);
}

ssize_t bdev_scatter(dev_t dev, u64_t pos, iovec_t *vec, int count, int flags)
{
/* Perform a write call from a vector of buffers.
 */

  return bdev_vrdwt(BDEV_SCATTER, dev, pos, vec, count, flags);
}

static int bdev_ioctl_setup(dev_t dev, int request, void *buf, message *m)
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
  if (_MINIX_IOCTL_IOR(access)) access |= CPF_WRITE;
  if (_MINIX_IOCTL_IOW(access)) access |= CPF_READ;

  /* The size may be 0, in which case 'buf' need not be a valid pointer. */
  grant = cpf_grant_direct(endpt, (vir_bytes) buf, size, access);

  if (!GRANT_VALID(grant)) {
	printf("bdev: unable to allocate grant!\n");
	return EINVAL;
  }

  memset(m, 0, sizeof(*m));
  m->m_type = BDEV_IOCTL;
  m->BDEV_MINOR = minor(dev);
  m->BDEV_REQUEST = request;
  m->BDEV_GRANT = grant;

  return OK;
}

static void bdev_ioctl_cleanup(message *m)
{
/* Clean up an I/O control request.
 */

  cpf_revoke(m->BDEV_GRANT);
}

int bdev_ioctl(dev_t dev, int request, void *buf)
{
/* Perform an I/O control request.
 */
  message m;
  int r;

  if ((r = bdev_ioctl_setup(dev, request, buf, &m)) != OK)
	return r;

  r = bdev_sendrec(dev, &m);

  bdev_ioctl_cleanup(&m);

  return r;
}
