/*
 * This file contains a number of device-type independent device routines.
 *
 * The entry points in this file are:
 *   do_ioctl:          perform the IOCTL system call
 *   make_ioctl_grant:  make a grant for an IOCTL request to a device
 */

#include "fs.h"
#include "vnode.h"
#include "file.h"
#include <sys/ioctl.h>

/*
 * Perform the ioctl(2) system call.
 */
int
do_ioctl(void)
{
	unsigned long request;
	struct filp *f;
	register struct vnode *vp;
	vir_bytes arg;
	int r, fd;

	fd = job_m_in.m_lc_vfs_ioctl.fd;
	request = job_m_in.m_lc_vfs_ioctl.req;
	arg = (vir_bytes)job_m_in.m_lc_vfs_ioctl.arg;

	if ((f = get_filp(fd, VNODE_READ)) == NULL)
		return(err_code);
	vp = f->filp_vno;		/* get vnode pointer */

	switch (vp->v_mode & S_IFMT) {
	case S_IFBLK:
		f->filp_ioctl_fp = fp;

		r = bdev_ioctl(vp->v_sdev, who_e, request, arg);

		f->filp_ioctl_fp = NULL;
		break;

	case S_IFCHR:
		r = cdev_io(CDEV_IOCTL, vp->v_sdev, who_e, arg, 0, request,
		    f->filp_flags);
		break;

	case S_IFSOCK:
		r = sdev_ioctl(vp->v_sdev, request, arg, f->filp_flags);
		break;

	default:
		r = ENOTTY;
	}

	unlock_filp(f);

	return r;
}

/*
 * Create a magic grant for the given IOCTL request.
 */
cp_grant_id_t
make_ioctl_grant(endpoint_t driver_e, endpoint_t user_e, vir_bytes buf,
	unsigned long request)
{
	cp_grant_id_t grant;
	int access;
	size_t size;

	/*
	 * For IOCTLs, the bytes parameter contains the IOCTL request.
	 * This request encodes the requested access method and buffer size.
	 */
	access = 0;
	if (_MINIX_IOCTL_IOR(request)) access |= CPF_WRITE;
	if (_MINIX_IOCTL_IOW(request)) access |= CPF_READ;
	if (_MINIX_IOCTL_BIG(request))
		size = _MINIX_IOCTL_SIZE_BIG(request);
	else
		size = _MINIX_IOCTL_SIZE(request);

	/*
	 * Grant access to the buffer even if no I/O happens with the ioctl,
	 * although now that we no longer identify responses based on grants,
	 * this is not strictly necessary.
	 */
	grant = cpf_grant_magic(driver_e, user_e, buf, size, access);

	if (!GRANT_VALID(grant))
		panic("VFS: cpf_grant_magic failed");

	return grant;
}
