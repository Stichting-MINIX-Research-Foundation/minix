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

  m.m_type = req;
  m.DEVICE = minor(dev);
  m.COUNT = access;

  return bdev_sendrec(dev, &m);
}

int bdev_open(dev_t dev, int access)
{
/* Open the given minor device.
 * File system usage note: typically called from mount, after bdev_driver.
 */

  return bdev_opcl(DEV_OPEN, dev, access);
}

int bdev_close(dev_t dev)
{
/* Close the given minor device.
 * File system usage note: typically called from unmount.
 */

  return bdev_opcl(DEV_CLOSE, dev, 0);
}

static int bdev_rdwt_setup(int req, dev_t dev, u64_t pos, char *buf, int count,
  int UNUSED(flags), message *m)
{
/* Set up a single-buffer read/write request.
 */
  endpoint_t endpt;
  cp_grant_id_t grant;
  int access;

  if ((endpt = bdev_driver_get(dev)) == NONE)
	return EDEADSRCDST;

  access = (req == DEV_READ_S) ? CPF_WRITE : CPF_READ;

  grant = cpf_grant_direct(endpt, (vir_bytes) buf, count, access);

  if (!GRANT_VALID(grant)) {
	printf("bdev: unable to allocate grant!\n");
	return EINVAL;
  }

  m->m_type = req;
  m->DEVICE = minor(dev);
  m->POSITION = ex64lo(pos);
  m->HIGHPOS = ex64hi(pos);
  m->COUNT = count;
  m->IO_GRANT = (void *) grant;

  return OK;
}

static void bdev_rdwt_cleanup(message *m)
{
/* Clean up a single-buffer read/write request.
 */
  cp_grant_id_t grant;

  grant = (cp_grant_id_t) m->IO_GRANT;

  cpf_revoke(grant);
}

static int bdev_rdwt(int req, dev_t dev, u64_t pos, char *buf, int count,
  int flags)
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
  int count, int UNUSED(flags), message *m, iovec_s_t *gvec,
  cp_grant_id_t *grants, vir_bytes *size)
{
/* Set up a vectored read/write request.
 */
  endpoint_t endpt;
  cp_grant_id_t grant;
  int i, access;

  assert(count <= NR_IOREQS);

  if ((endpt = bdev_driver_get(dev)) == NONE)
	return EDEADSRCDST;

  access = (req == DEV_GATHER_S) ? CPF_WRITE : CPF_READ;
  *size = 0;

  for (i = 0; i < count; i++) {
	grants[i] = cpf_grant_direct(endpt, vec[i].iov_addr, vec[i].iov_size,
		access);

	if (!GRANT_VALID(grants[i])) {
		printf("bdev: unable to allocate grant!\n");

		for (i--; i >= 0; i--)
			cpf_revoke(grants[i]);

		return EINVAL;
	}

	/* We keep a separate grants array to prevent local leaks if the driver
	 * ends up clobbering the grant vector. Future protocol updates should
	 * make the grant for the vector read-only.
	 */
	gvec[i].iov_grant = grants[i];
	gvec[i].iov_size = vec[i].iov_size;

	assert(*size + vec[i].iov_size > *size);

	*size += vec[i].iov_size;
  }

  grant = cpf_grant_direct(endpt, (vir_bytes) gvec, sizeof(gvec[0]) * count,
	CPF_READ | CPF_WRITE);

  if (!GRANT_VALID(grant)) {
	printf("bdev: unable to allocate grant!\n");

	for (i = count - 1; i >= 0; i--)
		cpf_revoke(grants[i]);

	return EINVAL;
  }

  m->m_type = req;
  m->DEVICE = minor(dev);
  m->POSITION = ex64lo(pos);
  m->HIGHPOS = ex64hi(pos);
  m->COUNT = count;
  m->IO_GRANT = (void *) grant;

  return OK;
}

static void bdev_vrdwt_cleanup(message *m, cp_grant_id_t *grants)
{
/* Clean up a vectored read/write request.
 */
  cp_grant_id_t grant;
  int i;

  grant = (cp_grant_id_t) m->IO_GRANT;

  cpf_revoke(grant);

  for (i = m->COUNT - 1; i >= 0; i--)
	cpf_revoke(grants[i]);
}

static int bdev_vrdwt_adjust(dev_t dev, iovec_s_t *gvec, int count,
  vir_bytes *size)
{
/* Adjust the number of bytes transferred, by subtracting from it the number of
 * bytes *not* transferred according to the result vector.
 */
  int i;

  for (i = 0; i < count; i++) {
	if (*size < gvec[i].iov_size) {
		printf("bdev: driver (%d) returned bad vector\n",
			bdev_driver_get(dev));

		return FALSE;
	}

	*size -= gvec[i].iov_size;
  }

  return TRUE;
}

static int bdev_vrdwt(int req, dev_t dev, u64_t pos, iovec_t *vec, int count,
  int flags, vir_bytes *size)
{
/* Perform a read or write call using a vector of buffer.
 */
  iovec_s_t gvec[NR_IOREQS];
  cp_grant_id_t grants[NR_IOREQS];
  message m;
  int r;

  if ((r = bdev_vrdwt_setup(req, dev, pos, vec, count, flags, &m, gvec,
		grants, size)) != OK) {
	*size = 0;
	return r;
  }

  r = bdev_sendrec(dev, &m);

  bdev_vrdwt_cleanup(&m, grants);

  /* Also return the number of bytes transferred. */
  if (!bdev_vrdwt_adjust(dev, gvec, count, size)) {
	*size = 0;
	r = EIO;
  }

  return r;
}

int bdev_read(dev_t dev, u64_t pos, char *buf, int count, int flags)
{
/* Perform a read call into a single buffer.
 */

  return bdev_rdwt(DEV_READ_S, dev, pos, buf, count, flags);
}

int bdev_write(dev_t dev, u64_t pos, char *buf, int count, int flags)
{
/* Perform a write call from a single buffer.
 */

  return bdev_rdwt(DEV_WRITE_S, dev, pos, buf, count, flags);
}

int bdev_gather(dev_t dev, u64_t pos, iovec_t *vec, int count, int flags,
  vir_bytes *size)
{
/* Perform a read call into a vector of buffers.
 */

  return bdev_vrdwt(DEV_GATHER_S, dev, pos, vec, count, flags, size);
}

int bdev_scatter(dev_t dev, u64_t pos, iovec_t *vec, int count, int flags,
  vir_bytes *size)
{
/* Perform a write call from a vector of buffers.
 */

  return bdev_vrdwt(DEV_SCATTER_S, dev, pos, vec, count, flags, size);
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

  m->m_type = DEV_IOCTL_S;
  m->DEVICE = minor(dev);
  m->REQUEST = request;
  m->IO_GRANT = (void *) grant;

  return OK;
}

static void bdev_ioctl_cleanup(message *m)
{
/* Clean up an I/O control request.
 */
  cp_grant_id_t grant;

  grant = (cp_grant_id_t) m->IO_GRANT;

  cpf_revoke(grant);
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
