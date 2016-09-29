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
#include <stdlib.h>
#include <minix/if.h>
#include <minix/bpf.h>
#include <assert.h>

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
 * Convert a network interface related IOCTL with pointers to a flat format
 * suitable for MINIX3.  Return a pointer to the new data on success, or zero
 * (with errno set) on failure.  The original request code is given in
 * 'request' and must be replaced by the new request code to be used.
 */
static vir_bytes
ioctl_convert_if_to_minix(void * data, unsigned long * request)
{
	struct minix_ifmediareq *mifm;
	struct ifmediareq *ifm;
	struct minix_if_clonereq *mifcr;
	struct if_clonereq *ifcr;

	switch (*request) {
	case SIOCGIFMEDIA:
		ifm = (struct ifmediareq *)data;

		mifm = (struct minix_ifmediareq *)malloc(sizeof(*mifm));
		if (mifm != NULL) {
			/*
			 * The count may exceed MINIX_IF_MAXMEDIA, and should
			 * be truncated as needed by the IF implementation.
			 */
			memcpy(&mifm->mifm_ifm, ifm, sizeof(*ifm));

			*request = MINIX_SIOCGIFMEDIA;
		} else
			errno = ENOMEM;

		return (vir_bytes)mifm;

	case SIOCIFGCLONERS:
		ifcr = (struct if_clonereq *)data;

		mifcr = (struct minix_if_clonereq *)malloc(sizeof(*mifcr));
		if (mifcr != NULL) {
			/*
			 * The count may exceed MINIX_IF_MAXCLONERS, and should
			 * be truncated as needed by the IF implementation.
			 */
			memcpy(&mifcr->mifcr_ifcr, ifcr, sizeof(*ifcr));

			*request = MINIX_SIOCIFGCLONERS;
		} else
			errno = ENOMEM;

		return (vir_bytes)mifcr;

	default:
		assert(0);

		errno = ENOTTY;
		return 0;
	}
}

/*
 * Convert a the result of a network interface related IOCTL with pointers from
 * the flat format used to make the call to MINIX3.  Called on success only.
 * The given request code is that of the (NetBSD-type) original.
 */
static void
ioctl_convert_if_from_minix(vir_bytes addr, void * data, unsigned long request)
{
	struct minix_ifmediareq *mifm;
	struct ifmediareq *ifm;
	struct minix_if_clonereq *mifcr;
	struct if_clonereq *ifcr;
	int count;

	switch (request) {
	case SIOCGIFMEDIA:
		mifm = (struct minix_ifmediareq *)addr;
		ifm = (struct ifmediareq *)data;

		memcpy(ifm, &mifm->mifm_ifm, sizeof(*ifm));

		if (ifm->ifm_ulist != NULL && ifm->ifm_count > 0)
			memcpy(ifm->ifm_ulist, mifm->mifm_list,
			    ifm->ifm_count * sizeof(ifm->ifm_ulist[0]));

		break;

	case SIOCIFGCLONERS:
		mifcr = (struct minix_if_clonereq *)addr;
		ifcr = (struct if_clonereq *)data;

		memcpy(ifcr, &mifcr->mifcr_ifcr, sizeof(*ifcr));

		count = (ifcr->ifcr_count < ifcr->ifcr_total) ?
		    ifcr->ifcr_count : ifcr->ifcr_total;
		if (ifcr->ifcr_buffer != NULL && count > 0)
			memcpy(ifcr->ifcr_buffer, mifcr->mifcr_buffer,
			    count * IFNAMSIZ);

		break;

	default:
		assert(0);
	}
}

/*
 * Convert a BPF (Berkeley Packet Filter) related IOCTL with pointers to a flat
 * format suitable for MINIX3.  Return a pointer to the new data on success, or
 * zero (with errno set) on failure.  The original request code is given in
 * 'request' and must be replaced by the new request code to be used.
 */
static vir_bytes
ioctl_convert_bpf_to_minix(void * data, unsigned long * request)
{
	struct minix_bpf_program *mbf;
	struct bpf_program *bf;
	struct minix_bpf_dltlist *mbfl;
	struct bpf_dltlist *bfl;

	switch (*request) {
	case BIOCSETF:
		bf = (struct bpf_program *)data;

		if (bf->bf_len > __arraycount(mbf->mbf_insns)) {
			errno = EINVAL;
			return 0;
		}

		mbf = (struct minix_bpf_program *)malloc(sizeof(*mbf));
		if (mbf != NULL) {
			mbf->mbf_len = bf->bf_len;
			memcpy(mbf->mbf_insns, bf->bf_insns,
			    bf->bf_len * sizeof(mbf->mbf_insns[0]));

			*request = MINIX_BIOCSETF;
		} else
			errno = ENOMEM;

		return (vir_bytes)mbf;

	case BIOCGDLTLIST:
		bfl = (struct bpf_dltlist *)data;

		mbfl = (struct minix_bpf_dltlist *)malloc(sizeof(*mbfl));
		if (mbfl != NULL) {
			/*
			 * The length may exceed MINIX_BPF_MAXDLT, and should
			 * be truncated as needed by the BPF implementation.
			 */
			memcpy(&mbfl->mbfl_dltlist, bfl, sizeof(*bfl));

			*request = MINIX_BIOCGDLTLIST;
		} else
			errno = ENOMEM;

		return (vir_bytes)mbfl;

	default:
		assert(0);

		errno = ENOTTY;
		return 0;
	}
}

/*
 * Convert a the result of BPF (Berkeley Packet Filter) related IOCTL with
 * pointers from the flat format used to make the call to MINIX3.  Called on
 * success only.  The given request code is that of the (NetBSD-type) original.
 */
static void
ioctl_convert_bpf_from_minix(vir_bytes addr, void * data,
	unsigned long request)
{
	struct minix_bpf_dltlist *mbfl;
	struct bpf_dltlist *bfl;

	switch (request) {
	case BIOCGDLTLIST:
		mbfl = (struct minix_bpf_dltlist *)addr;
		bfl = (struct bpf_dltlist *)data;

		memcpy(bfl, &mbfl->mbfl_dltlist, sizeof(*bfl));

		if (bfl->bfl_list != NULL && bfl->bfl_len > 0)
			memcpy(bfl->bfl_list, mbfl->mbfl_list,
			    bfl->bfl_len * sizeof(bfl->bfl_list[0]));

		break;

	default:
		assert(0);
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
  minix_i2c_ioctl_exec_t i2c;
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

	case SIOCGIFMEDIA:
	case SIOCIFGCLONERS:
		if ((addr = ioctl_convert_if_to_minix(data, &request)) == 0)
			return -1;	/* errno has already been set */
		break;

	case BIOCSETF:
	case BIOCGDLTLIST:
		if ((addr = ioctl_convert_bpf_to_minix(data, &request)) == 0)
			return -1;	/* errno has already been set */
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

  /*
   * Translate back to original form.  Do this on failure as well, as
   * temporarily allocated resources may have to be freed up again.
   */
  switch (request_save) {
	case I2C_IOCTL_EXEC:
		rewrite_i2c_minix_to_netbsd(data, &i2c);
		break;

	case SIOCGIFMEDIA:
	case SIOCIFGCLONERS:
		if (r == 0)
			ioctl_convert_if_from_minix(addr, data, request_save);
		free((void *)addr);
		break;

	case BIOCGDLTLIST:
		if (r == 0)
			ioctl_convert_bpf_from_minix(addr, data, request_save);
		/* FALLTHROUGH */
	case BIOCSETF:
		free((void *)addr);
		break;

	default:
		/* Nothing to do */
		break;
  }

  return r;
}
