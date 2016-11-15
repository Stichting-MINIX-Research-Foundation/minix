/*	$NetBSD: smb_dev.c,v 1.44 2015/08/20 14:40:19 christos Exp $	*/

/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/netsmb/smb_dev.c,v 1.4 2001/12/02 08:47:29 bp Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smb_dev.c,v 1.44 2015/08/20 14:40:19 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/ioccom.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* Must come after sys/malloc.h */
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <miscfs/specfs/specdev.h> /* XXX */

#include <net/if.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_rq.h>

#include "ioconf.h"

static struct smb_dev **smb_devtbl; /* indexed by minor */
#define SMB_GETDEV(dev) (smb_devtbl[minor(dev)])
#define NSMB_DEFNUM	4


MALLOC_DEFINE(M_NSMBDEV, "NETSMBDEV", "NET/SMB device");


/*
int smb_dev_queue(struct smb_dev *ndp, struct smb_rq *rqp, int prio);
*/

dev_type_open(nsmb_dev_open);
dev_type_close(nsmb_dev_close);
dev_type_ioctl(nsmb_dev_ioctl);

const struct cdevsw nsmb_cdevsw = {
	.d_open = nsmb_dev_open,
	.d_close = nsmb_dev_close,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = nsmb_dev_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER,
};


static bool nsmb_inited = false;

void
nsmbattach(int num)
{

	if (nsmb_inited)
		return;
	nsmb_inited = true;

	if (num <= 0) {
#ifdef DIAGNOSTIC
		panic("nsmbattach: count <= 0");
#endif
		return;
	}

	if (num == 1)
		num = NSMB_DEFNUM;

	smb_devtbl = malloc(num * sizeof(void *), M_NSMBDEV, M_WAITOK|M_ZERO);

	if (smb_sm_init()) {
#ifdef DIAGNOSTIC
		panic("netsmbattach: smb_sm_init failed");
#endif
		return;
	}

	if (smb_iod_init()) {
#ifdef DIAGNOSTIC
		panic("netsmbattach: smb_iod_init failed");
#endif
		smb_sm_done();
		return;
	}
	smb_rqpool_init();
}

static void
nsmbdetach(void)
{

	smb_iod_done();
	smb_sm_done();
	smb_rqpool_fini();
	free(smb_devtbl, M_NSMBDEV);
	nsmb_inited = false;
}

int
nsmb_dev_open(dev_t dev, int oflags, int devtype,
    struct lwp *l)
{
	struct smb_dev *sdp;
	int s;

	sdp = SMB_GETDEV(dev);
	if (sdp && (sdp->sd_flags & NSMBFL_OPEN))
		return EBUSY;
	if (sdp == NULL) {
		sdp = malloc(sizeof(*sdp), M_NSMBDEV, M_WAITOK);
		smb_devtbl[minor(dev)] = (void*)sdp;
	}


	memset(sdp, 0, sizeof(*sdp));
/*
	STAILQ_INIT(&sdp->sd_rqlist);
	STAILQ_INIT(&sdp->sd_rplist);
	selinit(&sdp->sd_pollinfo);
*/
	s = splnet();
	sdp->sd_level = -1;
	sdp->sd_flags |= NSMBFL_OPEN;
	splx(s);
	return 0;
}

int
nsmb_dev_close(dev_t dev, int flag, int fmt, struct lwp *l)
{
	struct smb_dev *sdp;
	struct smb_vc *vcp;
	struct smb_share *ssp;
	struct smb_cred scred;
	int s;

	sdp = SMB_GETDEV(dev);
	if (!sdp)
		return (ENXIO);

	s = splnet();
	if ((sdp->sd_flags & NSMBFL_OPEN) == 0) {
		splx(s);
		return EBADF;
	}
	smb_makescred(&scred, l, NULL);
	ssp = sdp->sd_share;
	if (ssp != NULL) {
		smb_share_lock(ssp);
		smb_share_rele(ssp, &scred);
	}
	vcp = sdp->sd_vc;
	if (vcp != NULL) {
		smb_vc_lock(vcp);
		smb_vc_rele(vcp, &scred);
	}
/*
	smb_flushq(&sdp->sd_rqlist);
	smb_flushq(&sdp->sd_rplist);
	seldestroy(&sdp->sd_pollinfo);
*/
	smb_devtbl[minor(dev)] = NULL;
	free(sdp, M_NSMBDEV);
	splx(s);
	return 0;
}


int
nsmb_dev_ioctl(dev_t dev, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct smb_dev *sdp;
	struct smb_vc *vcp;
	struct smb_share *ssp;
	struct smb_cred scred;
	int error = 0;

	sdp = SMB_GETDEV(dev);
	if (!sdp)
		return (ENXIO);
	if ((sdp->sd_flags & NSMBFL_OPEN) == 0)
		return EBADF;

	smb_makescred(&scred, l, NULL);
	switch (cmd) {
	    case SMBIOC_OPENSESSION:
		if (sdp->sd_vc)
			return EISCONN;
		error = smb_usr_opensession((struct smbioc_ossn*)data,
		    &scred, &vcp);
		if (error)
			break;
		sdp->sd_vc = vcp;
		smb_vc_unlock(vcp);
		sdp->sd_level = SMBL_VC;
		break;
	    case SMBIOC_OPENSHARE:
		if (sdp->sd_share)
			return EISCONN;
		if (sdp->sd_vc == NULL)
			return ENOTCONN;
		error = smb_usr_openshare(sdp->sd_vc,
		    (struct smbioc_oshare*)data, &scred, &ssp);
		if (error)
			break;
		sdp->sd_share = ssp;
		smb_share_unlock(ssp);
		sdp->sd_level = SMBL_SHARE;
		break;
	    case SMBIOC_REQUEST:
		if (sdp->sd_share == NULL)
			return ENOTCONN;
		error = smb_usr_simplerequest(sdp->sd_share,
		    (struct smbioc_rq*)data, &scred);
		break;
	    case SMBIOC_T2RQ:
		if (sdp->sd_share == NULL)
			return ENOTCONN;
		error = smb_usr_t2request(sdp->sd_share,
		    (struct smbioc_t2rq*)data, &scred);
		break;
	    case SMBIOC_SETFLAGS: {
		struct smbioc_flags *fl = (struct smbioc_flags*)data;
		int on;

		if (fl->ioc_level == SMBL_VC) {
			if (fl->ioc_mask & SMBV_PERMANENT) {
				on = fl->ioc_flags & SMBV_PERMANENT;
				if ((vcp = sdp->sd_vc) == NULL)
					return ENOTCONN;
				error = smb_vc_get(vcp, &scred);
				if (error)
					break;
				if (on && (vcp->obj.co_flags & SMBV_PERMANENT) == 0) {
					vcp->obj.co_flags |= SMBV_PERMANENT;
					smb_vc_ref(vcp);
				} else if (!on && (vcp->obj.co_flags & SMBV_PERMANENT)) {
					vcp->obj.co_flags &= ~SMBV_PERMANENT;
					smb_vc_rele(vcp, &scred);
				}
				smb_vc_put(vcp, &scred);
			} else
				error = EINVAL;
		} else if (fl->ioc_level == SMBL_SHARE) {
			if (fl->ioc_mask & SMBS_PERMANENT) {
				on = fl->ioc_flags & SMBS_PERMANENT;
				if ((ssp = sdp->sd_share) == NULL)
					return ENOTCONN;
				error = smb_share_get(ssp, &scred);
				if (error)
					break;
				if (on && (ssp->obj.co_flags & SMBS_PERMANENT) == 0) {
					ssp->obj.co_flags |= SMBS_PERMANENT;
					smb_share_ref(ssp);
				} else if (!on && (ssp->obj.co_flags & SMBS_PERMANENT)) {
					ssp->obj.co_flags &= ~SMBS_PERMANENT;
					smb_share_rele(ssp, &scred);
				}
				smb_share_put(ssp, &scred);
			} else
				error = EINVAL;
			break;
		} else
			error = EINVAL;
		break;
	    }
	    case SMBIOC_LOOKUP:
		if (sdp->sd_vc || sdp->sd_share)
			return EISCONN;
		vcp = NULL;
		ssp = NULL;
		error = smb_usr_lookup((struct smbioc_lookup*)data, &scred, &vcp, &ssp);
		if (error)
			break;
		if (vcp) {
			sdp->sd_vc = vcp;
			smb_vc_unlock(vcp);
			sdp->sd_level = SMBL_VC;
		}
		if (ssp) {
			sdp->sd_share = ssp;
			smb_share_unlock(ssp);
			sdp->sd_level = SMBL_SHARE;
		}
		break;
	    case SMBIOC_READ: case SMBIOC_WRITE: {
		struct smbioc_rw *rwrq = (struct smbioc_rw*)data;
		struct uio auio;
		struct iovec iov;

		if ((ssp = sdp->sd_share) == NULL)
			return ENOTCONN;
		iov.iov_base = rwrq->ioc_base;
		iov.iov_len = rwrq->ioc_cnt;
		auio.uio_iov = &iov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = rwrq->ioc_offset;
		auio.uio_resid = rwrq->ioc_cnt;
		auio.uio_rw = (cmd == SMBIOC_READ) ? UIO_READ : UIO_WRITE;
		auio.uio_vmspace = l->l_proc->p_vmspace;
		if (cmd == SMBIOC_READ)
			error = smb_read(ssp, rwrq->ioc_fh, &auio, &scred);
		else
			error = smb_write(ssp, rwrq->ioc_fh, &auio, &scred);
		rwrq->ioc_cnt -= auio.uio_resid;
		break;
	    }
	    default:
		error = ENODEV;
	}
	return error;
}

MODULE(MODULE_CLASS_DRIVER, nsmb, NULL);
static int
nsmb_modcmd(modcmd_t cmd, void *arg)
{
	devmajor_t cmajor = NODEVMAJOR, bmajor = NODEVMAJOR;
	int error = 0;

	switch (cmd) {
	    case MODULE_CMD_INIT:
		nsmbattach(1);
		error =
		    devsw_attach("nsmb", NULL, &bmajor, &nsmb_cdevsw, &cmajor);
		if (error == EEXIST) /* builtin */
			error = 0;
		if (error) {
			nsmbdetach();
		}

		break;
	    case MODULE_CMD_FINI:
		error = devsw_detach(NULL, &nsmb_cdevsw);
		if (error)
			break;
		nsmbdetach();
		break;
	    default:
		error = ENOTTY;
		break;
	}

	return error;
}

/*
 * Convert a file descriptor to appropriate smb_share pointer
 */
int
smb_dev2share(int fd, int mode, struct smb_cred *scred,
    struct smb_share **sspp)
{
	file_t *fp;
	struct vnode *vp;
	struct smb_dev *sdp;
	struct smb_share *ssp;
	dev_t dev;
	int error;

	if ((fp = fd_getfile(fd)) == NULL)
		return (EBADF);

	vp = fp->f_vnode;
	if (fp->f_type != DTYPE_VNODE
	    || (fp->f_flag & (FREAD|FWRITE)) == 0
	    || vp->v_type != VCHR
	    || vp->v_rdev == NODEV) {
		fd_putfile(fd);
		return (EBADF);
	}

	dev = vp->v_rdev;

	fd_putfile(fd);

	sdp = SMB_GETDEV(dev);
	if (!sdp)
		return (ENXIO);
	ssp = sdp->sd_share;
	if (ssp == NULL)
		return ENOTCONN;

	error = smb_share_get(ssp, scred);
	if (error)
		return error;

	*sspp = ssp;
	return 0;
}
