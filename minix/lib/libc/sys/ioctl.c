#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>
#include <stdarg.h>

#include <sys/ioctl.h>
#include <minix/i2c.h>
#include <string.h>
#include <sys/ioccom.h>
#include <stdarg.h>
#include <fcntl.h>

static void rewrite_i2c_netbsd_to_minix(minix_i2c_ioctl_exec_t *out,
    i2c_ioctl_exec_t *in);
static void rewrite_i2c_minix_to_netbsd(i2c_ioctl_exec_t *out,
    minix_i2c_ioctl_exec_t *in);

static void rewrite_i2c_netbsd_to_minix(minix_i2c_ioctl_exec_t *out,
	i2c_ioctl_exec_t *in)
{
  memset(out, '\0', sizeof(minix_i2c_ioctl_exec_t));

  out->iie_op = in->iie_op;
  out->iie_addr = in->iie_addr;
  out->iie_cmdlen = I2C_EXEC_MAX_CMDLEN < in->iie_cmdlen ?
  	I2C_EXEC_MAX_CMDLEN : in->iie_cmdlen;
  out->iie_buflen = I2C_EXEC_MAX_BUFLEN < in->iie_buflen ?
  	I2C_EXEC_MAX_BUFLEN : in->iie_buflen;

  if (in->iie_cmdlen > 0 && in->iie_cmd != NULL) {
	memcpy(out->iie_cmd, in->iie_cmd, in->iie_cmdlen);
  }

  if (in->iie_buflen > 0 && in->iie_buf != NULL) {
	memcpy(out->iie_buf, in->iie_buf, in->iie_buflen);
  }
}

static void rewrite_i2c_minix_to_netbsd(i2c_ioctl_exec_t *out,
	minix_i2c_ioctl_exec_t *in)
{
  /* the only field that changes is iie_buf, everything else is the same */
  if (in->iie_buflen > 0 ) {
	memcpy(out->iie_buf, in->iie_buf, in->iie_buflen);
  }
}

/*
 * Library implementation of FIOCLEX and FIONCLEX.
 */
static int
ioctl_to_setfd(int fd, int mask, int val)
{
	int fl;

	if ((fl = fcntl(fd, F_GETFD)) == -1)
		return -1;

	fl = (fl & ~mask) | val;

	return fcntl(fd, F_SETFD, fl);
}

/*
 * Library implementation of FIONBIO and FIOASYNC.
 */
static int
ioctl_to_setfl(int fd, void * data, int sfl)
{
	int arg, fl;

	arg = *(int *)data;

	if ((fl = fcntl(fd, F_GETFL)) == -1)
		return -1;

	if (arg)
		fl |= sfl;
	else
		fl &= ~sfl;

	return fcntl(fd, F_SETFL, fl & ~O_ACCMODE);
}

/*
 * Library implementation of various deprecated IOCTLs.  These particular IOCTL
 * calls change how the file descriptors behave, and have nothing to do with
 * the actual open file.  They should therefore be handled by VFS rather than
 * individual device drivers.  We rewrite them to use fcntl(2) instead here.
 */
static int
ioctl_to_fcntl(int fd, unsigned long request, void * data)
{
	switch (request) {
	case FIOCLEX:
		return ioctl_to_setfd(fd, FD_CLOEXEC, FD_CLOEXEC);
	case FIONCLEX:
		return ioctl_to_setfd(fd, FD_CLOEXEC, 0);
	case FIONBIO:
		return ioctl_to_setfl(fd, data, O_NONBLOCK);
	case FIOASYNC:
		return ioctl_to_setfl(fd, data, O_ASYNC);
	case FIOSETOWN: /* XXX TODO */
	case FIOGETOWN: /* XXX TODO */
	default:
		errno = ENOTTY;
		return -1;
	}
}

int     ioctl(int fd, unsigned long request, ...)
{
  int r, request_save;
  message m;
  vir_bytes addr;
  void *data;
  va_list ap;

  va_start(ap, request);
  data = va_arg(ap, void *);
  va_end(ap);

  /*
   * To support compatibility with interfaces on other systems, certain
   * requests are re-written to flat structures (i.e. without pointers).
   */
  minix_i2c_ioctl_exec_t i2c;

  request_save = request;

  switch (request) {
	case FIOCLEX:
	case FIONCLEX:
	case FIONBIO:
	case FIOASYNC:
	case FIOSETOWN:
	case FIOGETOWN:
		return ioctl_to_fcntl(fd, request, data);

	case I2C_IOCTL_EXEC:
		rewrite_i2c_netbsd_to_minix(&i2c, data);
		addr = (vir_bytes) &i2c;
		request = MINIX_I2C_IOCTL_EXEC;
		break;
	default:
		/* Keep original as-is */
		addr = (vir_bytes)data;
		break;
  }

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_ioctl.fd = fd;
  m.m_lc_vfs_ioctl.req = request;
  m.m_lc_vfs_ioctl.arg = addr;

  r = _syscall(VFS_PROC_NR, VFS_IOCTL, &m);

  /* Translate back to original form */
  switch (request_save) {
	case I2C_IOCTL_EXEC:
		rewrite_i2c_minix_to_netbsd(data, &i2c);
		break;
	default:
		/* Nothing to do */
		break;
  }

  return r;
}
