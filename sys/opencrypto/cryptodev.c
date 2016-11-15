/*	$NetBSD: cryptodev.c,v 1.84 2015/08/20 14:40:19 christos Exp $ */
/*	$FreeBSD: src/sys/opencrypto/cryptodev.c,v 1.4.2.4 2003/06/03 00:09:02 sam Exp $	*/
/*	$OpenBSD: cryptodev.c,v 1.53 2002/07/10 22:21:30 mickey Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2001 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cryptodev.c,v 1.84 2015/08/20 14:40:19 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/pool.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/errno.h>
#include <sys/md5.h>
#include <sys/sha1.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kauth.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/atomic.h>
#include <sys/stat.h>
#include <sys/module.h>

#ifdef _KERNEL_OPT
#include "opt_ocf.h"
#include "opt_compat_netbsd.h"
#endif

#include <opencrypto/cryptodev.h>
#include <opencrypto/cryptodev_internal.h>
#include <opencrypto/xform.h>

#include "ioconf.h"

struct csession {
	TAILQ_ENTRY(csession) next;
	u_int64_t	sid;
	u_int32_t	ses;

	u_int32_t	cipher;		/* note: shares name space in crd_alg */
	const struct enc_xform *txform;
	u_int32_t	mac;		/* note: shares name space in crd_alg */
	const struct auth_hash *thash;
	u_int32_t	comp_alg;	/* note: shares name space in crd_alg */
	const struct comp_algo *tcomp;

	void *		key;
	int		keylen;
	u_char		tmp_iv[EALG_MAX_BLOCK_LEN];

	void *		mackey;
	int		mackeylen;
	u_char		tmp_mac[CRYPTO_MAX_MAC_LEN];

	struct iovec	iovec[1];	/* user requests never have more */
	struct uio	uio;
	int		error;
};

struct fcrypt {
	TAILQ_HEAD(csessionlist, csession) csessions;
	TAILQ_HEAD(crprethead, cryptop) crp_ret_mq;
	TAILQ_HEAD(krprethead, cryptkop) crp_ret_mkq;
	int		sesn;
	struct selinfo	sinfo;
	u_int32_t	requestid;
	struct timespec atime;
	struct timespec mtime;
	struct timespec btime;
};

/* For our fixed-size allocations */
static struct pool fcrpl;
static struct pool csepl;

/* Declaration of master device (fd-cloning/ctxt-allocating) entrypoints */
static int	cryptoopen(dev_t dev, int flag, int mode, struct lwp *l);
static int	cryptoread(dev_t dev, struct uio *uio, int ioflag);
static int	cryptowrite(dev_t dev, struct uio *uio, int ioflag);
static int	cryptoselect(dev_t dev, int rw, struct lwp *l);

static int	crypto_refcount = 0;	/* Prevent detaching while in use */

/* Declaration of cloned-device (per-ctxt) entrypoints */
static int	cryptof_read(struct file *, off_t *, struct uio *,
    kauth_cred_t, int);
static int	cryptof_write(struct file *, off_t *, struct uio *,
    kauth_cred_t, int);
static int	cryptof_ioctl(struct file *, u_long, void *);
static int	cryptof_close(struct file *);
static int 	cryptof_poll(struct file *, int);
static int 	cryptof_stat(struct file *, struct stat *);

static const struct fileops cryptofops = {
	.fo_read = cryptof_read,
	.fo_write = cryptof_write,
	.fo_ioctl = cryptof_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = cryptof_poll,
	.fo_stat = cryptof_stat,
	.fo_close = cryptof_close,
	.fo_kqfilter = fnullop_kqfilter,
	.fo_restart = fnullop_restart,
};

struct csession *cryptodev_csefind(struct fcrypt *, u_int);
static struct	csession *csefind(struct fcrypt *, u_int);
static int	csedelete(struct fcrypt *, struct csession *);
static struct	csession *cseadd(struct fcrypt *, struct csession *);
static struct	csession *csecreate(struct fcrypt *, u_int64_t, void *,
    u_int64_t, void *, u_int64_t, u_int32_t, u_int32_t, u_int32_t,
    const struct enc_xform *, const struct auth_hash *,
    const struct comp_algo *);
static int	csefree(struct csession *);

static int	cryptodev_key(struct crypt_kop *);
static int	cryptodev_mkey(struct fcrypt *, struct crypt_n_kop *, int);
static int	cryptodev_msessionfin(struct fcrypt *, int, u_int32_t *);

static int	cryptodev_cb(void *);
static int	cryptodevkey_cb(void *);

static int	cryptodev_mcb(void *);
static int	cryptodevkey_mcb(void *);

static int 	cryptodev_getmstatus(struct fcrypt *, struct crypt_result *,
    int);
static int	cryptodev_getstatus(struct fcrypt *, struct crypt_result *);

#ifdef COMPAT_50
extern int	ocryptof_ioctl(struct file *, u_long, void *);
#endif

/*
 * sysctl-able control variables for /dev/crypto now defined in crypto.c:
 * crypto_usercrypto, crypto_userasmcrypto, crypto_devallowsoft.
 */

/* ARGSUSED */
int
cryptof_read(file_t *fp, off_t *poff,
    struct uio *uio, kauth_cred_t cred, int flags)
{
	return EIO;
}

/* ARGSUSED */
int
cryptof_write(file_t *fp, off_t *poff,
    struct uio *uio, kauth_cred_t cred, int flags)
{
	return EIO;
}

/* ARGSUSED */
int
cryptof_ioctl(struct file *fp, u_long cmd, void *data)
{
	struct fcrypt *fcr = fp->f_fcrypt;
	struct csession *cse;
	struct session_op *sop;
	struct session_n_op *snop;
	struct crypt_op *cop;
	struct crypt_mop *mop;
	struct crypt_mkop *mkop;
	struct crypt_n_op *cnop;
	struct crypt_n_kop *knop;
	struct crypt_sgop *sgop;
	struct crypt_sfop *sfop;
	struct cryptret *crypt_ret;
	struct crypt_result *crypt_res;
	u_int32_t ses;
	u_int32_t *sesid;
	int error = 0;
	size_t count;

	/* backwards compatibility */
        file_t *criofp;
	struct fcrypt *criofcr;
	int criofd;

	mutex_enter(&crypto_mtx);
	getnanotime(&fcr->atime);
	mutex_exit(&crypto_mtx);

	switch (cmd) {
        case CRIOGET:   /* XXX deprecated, remove after 5.0 */
		if ((error = fd_allocfile(&criofp, &criofd)) != 0)
			return error;
		criofcr = pool_get(&fcrpl, PR_WAITOK);
		mutex_enter(&crypto_mtx);
		TAILQ_INIT(&criofcr->csessions);
		TAILQ_INIT(&criofcr->crp_ret_mq);
		TAILQ_INIT(&criofcr->crp_ret_mkq);
		selinit(&criofcr->sinfo);

                /*
                 * Don't ever return session 0, to allow detection of
                 * failed creation attempts with multi-create ioctl.
                 */
		criofcr->sesn = 1;
		criofcr->requestid = 1;
		crypto_refcount++;
		mutex_exit(&crypto_mtx);
		(void)fd_clone(criofp, criofd, (FREAD|FWRITE),
			      &cryptofops, criofcr);
		*(u_int32_t *)data = criofd;
		return error;
		break;
	case CIOCGSESSION:
		sop = (struct session_op *)data;
		error = cryptodev_session(fcr, sop);
		break;
	case CIOCNGSESSION:
		sgop = (struct crypt_sgop *)data;
		snop = kmem_alloc((sgop->count *
				  sizeof(struct session_n_op)), KM_SLEEP);
		error = copyin(sgop->sessions, snop, sgop->count *
			       sizeof(struct session_n_op));
		if (error) {
			goto mbail;
		}

		mutex_enter(&crypto_mtx);
		fcr->mtime = fcr->atime;
		mutex_exit(&crypto_mtx);
		error = cryptodev_msession(fcr, snop, sgop->count);
		if (error) {
			goto mbail;
		}

		error = copyout(snop, sgop->sessions, sgop->count *
		    sizeof(struct session_n_op));
mbail:
		kmem_free(snop, sgop->count * sizeof(struct session_n_op));
		break;
	case CIOCFSESSION:
		mutex_enter(&crypto_mtx);
		fcr->mtime = fcr->atime;
		ses = *(u_int32_t *)data;
		cse = csefind(fcr, ses);
		if (cse == NULL) {
			mutex_exit(&crypto_mtx);
			return EINVAL;
		}
		csedelete(fcr, cse);
		mutex_exit(&crypto_mtx);
		error = csefree(cse);
		break;
	case CIOCNFSESSION:
		mutex_enter(&crypto_mtx);
		fcr->mtime = fcr->atime;
		mutex_exit(&crypto_mtx);
		sfop = (struct crypt_sfop *)data;
		sesid = kmem_alloc((sfop->count * sizeof(u_int32_t)), 
		    KM_SLEEP);
		error = copyin(sfop->sesid, sesid,
		    (sfop->count * sizeof(u_int32_t)));
		if (!error) {
			error = cryptodev_msessionfin(fcr, sfop->count, sesid);
		}
		kmem_free(sesid, (sfop->count * sizeof(u_int32_t)));
		break;
	case CIOCCRYPT:
		mutex_enter(&crypto_mtx);
		fcr->mtime = fcr->atime;
		cop = (struct crypt_op *)data;
		cse = csefind(fcr, cop->ses);
		mutex_exit(&crypto_mtx);
		if (cse == NULL) {
			DPRINTF(("csefind failed\n"));
			return EINVAL;
		}
		error = cryptodev_op(cse, cop, curlwp);
		DPRINTF(("cryptodev_op error = %d\n", error));
		break;
	case CIOCNCRYPTM:
		mutex_enter(&crypto_mtx);
		fcr->mtime = fcr->atime;
		mutex_exit(&crypto_mtx);
		mop = (struct crypt_mop *)data;
		cnop = kmem_alloc((mop->count * sizeof(struct crypt_n_op)),
		    KM_SLEEP);
		error = copyin(mop->reqs, cnop,
		    (mop->count * sizeof(struct crypt_n_op)));
		if(!error) {
			error = cryptodev_mop(fcr, cnop, mop->count, curlwp);
			if (!error) {
				error = copyout(cnop, mop->reqs, 
				    (mop->count * sizeof(struct crypt_n_op)));
			}
		}
		kmem_free(cnop, (mop->count * sizeof(struct crypt_n_op)));
		break;		
	case CIOCKEY:
		error = cryptodev_key((struct crypt_kop *)data);
		DPRINTF(("cryptodev_key error = %d\n", error));
		break;
	case CIOCNFKEYM:
		mutex_enter(&crypto_mtx);
		fcr->mtime = fcr->atime;
		mutex_exit(&crypto_mtx);
		mkop = (struct crypt_mkop *)data;
		knop = kmem_alloc((mkop->count * sizeof(struct crypt_n_kop)),
		    KM_SLEEP);
		error = copyin(mkop->reqs, knop,
		    (mkop->count * sizeof(struct crypt_n_kop)));
		if (!error) {
			error = cryptodev_mkey(fcr, knop, mkop->count);
			if (!error)
				error = copyout(knop, mkop->reqs,
				    (mkop->count * sizeof(struct crypt_n_kop)));
		}
		kmem_free(knop, (mkop->count * sizeof(struct crypt_n_kop)));
		break;
	case CIOCASYMFEAT:
		error = crypto_getfeat((int *)data);
		break;
	case CIOCNCRYPTRETM:
		mutex_enter(&crypto_mtx);
		fcr->mtime = fcr->atime;
		mutex_exit(&crypto_mtx);
		crypt_ret = (struct cryptret *)data;
		count = crypt_ret->count;
		crypt_res = kmem_alloc((count * sizeof(struct crypt_result)),  
		    KM_SLEEP);
		error = copyin(crypt_ret->results, crypt_res,
		    (count * sizeof(struct crypt_result)));
		if (error)
			goto reterr;
		crypt_ret->count = cryptodev_getmstatus(fcr, crypt_res,
		    crypt_ret->count);
		/* sanity check count */
		if (crypt_ret->count > count) {
			printf("%s.%d: error returned count %zd > original "
			    " count %zd\n",
			    __FILE__, __LINE__, crypt_ret->count, count);
			crypt_ret->count = count;

		}
		error = copyout(crypt_res, crypt_ret->results,
		    (crypt_ret->count * sizeof(struct crypt_result)));
reterr:
		kmem_free(crypt_res, (count * sizeof(struct crypt_result)));
		break;
	case CIOCNCRYPTRET:
		error = cryptodev_getstatus(fcr, (struct crypt_result *)data); 
		break;
	default:
#ifdef COMPAT_50
		/* Check for backward compatible commands */
		error = ocryptof_ioctl(fp, cmd, data);
#else
		return EINVAL;
#endif
	}
	return error;
}

int
cryptodev_op(struct csession *cse, struct crypt_op *cop, struct lwp *l)
{
	struct cryptop *crp = NULL;
	struct cryptodesc *crde = NULL, *crda = NULL, *crdc = NULL;
	int error;
	int iov_len = cop->len;
	int flags=0;
	int dst_len;	/* copyout size */

	if (cop->len > 256*1024-4)
		return E2BIG;

	if (cse->txform) {
		if (cop->len < cse->txform->blocksize
		    + (cop->iv ? 0 : cse->txform->ivsize) ||
		    (cop->len - (cop->iv ? 0 : cse->txform->ivsize))
		    % cse->txform->blocksize != 0)
			return EINVAL;
	}

	DPRINTF(("cryptodev_op[%u]: iov_len %d\n",
		CRYPTO_SESID2LID(cse->sid), iov_len));
	if ((cse->tcomp) && cop->dst_len) {
		if (iov_len < cop->dst_len) {
			/* Need larger iov to deal with decompress */
			iov_len = cop->dst_len;
		}
		DPRINTF(("cryptodev_op: iov_len -> %d for decompress\n", iov_len));
	}

	(void)memset(&cse->uio, 0, sizeof(cse->uio));
	cse->uio.uio_iovcnt = 1;
	cse->uio.uio_resid = 0;
	cse->uio.uio_rw = UIO_WRITE;
	cse->uio.uio_iov = cse->iovec;
	UIO_SETUP_SYSSPACE(&cse->uio);
	memset(&cse->iovec, 0, sizeof(cse->iovec));

	/* the iov needs to be big enough to handle the uncompressed
	 * data.... */
	cse->uio.uio_iov[0].iov_len = iov_len;
	if (iov_len > 0)
		cse->uio.uio_iov[0].iov_base = kmem_alloc(iov_len, KM_SLEEP);
	cse->uio.uio_resid = cse->uio.uio_iov[0].iov_len;
	DPRINTF(("cryptodev_op[%u]: uio.iov_base %p malloced %d bytes\n",
		CRYPTO_SESID2LID(cse->sid),
		cse->uio.uio_iov[0].iov_base, iov_len));

	crp = crypto_getreq((cse->tcomp != NULL) + (cse->txform != NULL) + (cse->thash != NULL));
	if (crp == NULL) {
		error = ENOMEM;
		goto bail;
	}
	DPRINTF(("cryptodev_op[%u]: crp %p\n",
		CRYPTO_SESID2LID(cse->sid), crp));

	/* crds are always ordered tcomp, thash, then txform */
	/* with optional missing links */

	/* XXX: If we're going to compress then hash or encrypt, we need
	 * to be able to pass on the new size of the data.
	 */

	if (cse->tcomp) {
		crdc = crp->crp_desc;
	}

	if (cse->thash) {
		crda = crdc ? crdc->crd_next : crp->crp_desc;
		if (cse->txform && crda)
			crde = crda->crd_next;
	} else {
		if (cse->txform) {
			crde = crdc ? crdc->crd_next : crp->crp_desc;
		} else if (!cse->tcomp) {
			error = EINVAL;
			goto bail;
		}
	}

	DPRINTF(("ocf[%u]: iov_len %zu, cop->len %u\n",
			CRYPTO_SESID2LID(cse->sid),
			cse->uio.uio_iov[0].iov_len, 
			cop->len));

	if ((error = copyin(cop->src, cse->uio.uio_iov[0].iov_base, cop->len)))
	{
		printf("copyin failed %s %d \n", (char *)cop->src, error);
		goto bail;
	}

	if (crdc) {
		switch (cop->op) {
		case COP_COMP:
			crdc->crd_flags |= CRD_F_COMP;
			break;
		case COP_DECOMP:
			crdc->crd_flags &= ~CRD_F_COMP;
			break;
		default:
			break;
		}
		/* more data to follow? */
		if (cop->flags & COP_F_MORE) {
			flags |= CRYPTO_F_MORE;
		}
		crdc->crd_len = cop->len;
		crdc->crd_inject = 0;

		crdc->crd_alg = cse->comp_alg;
		crdc->crd_key = NULL;
		crdc->crd_klen = 0;
		DPRINTF(("cryptodev_op[%u]: crdc setup for comp_alg %d.\n",
			CRYPTO_SESID2LID(cse->sid), crdc->crd_alg));
	}

	if (crda) {
		crda->crd_skip = 0;
		crda->crd_len = cop->len;
		crda->crd_inject = 0;	/* ??? */

		crda->crd_alg = cse->mac;
		crda->crd_key = cse->mackey;
		crda->crd_klen = cse->mackeylen * 8;
		DPRINTF(("cryptodev_op: crda setup for mac %d.\n", crda->crd_alg));
	}

	if (crde) {
		switch (cop->op) {
		case COP_ENCRYPT:
			crde->crd_flags |= CRD_F_ENCRYPT;
			break;
		case COP_DECRYPT:
			crde->crd_flags &= ~CRD_F_ENCRYPT;
			break;
		default:
			break;
		}
		crde->crd_len = cop->len;
		crde->crd_inject = 0;

		if (cse->cipher == CRYPTO_AES_GCM_16 && crda)
			crda->crd_len = 0;
		else if (cse->cipher == CRYPTO_AES_GMAC)
			crde->crd_len = 0;

		crde->crd_alg = cse->cipher;
		crde->crd_key = cse->key;
		crde->crd_klen = cse->keylen * 8;
		DPRINTF(("cryptodev_op: crde setup for cipher %d.\n", crde->crd_alg));
	}


	crp->crp_ilen = cop->len;
	/* The reqest is flagged as CRYPTO_F_USER as long as it is running
	 * in the user IOCTL thread.  This flag lets us skip using the retq for
	 * the request if it completes immediately. If the request ends up being
	 * delayed or is not completed immediately the flag is removed.
	 */
	crp->crp_flags = CRYPTO_F_IOV | (cop->flags & COP_F_BATCH) | CRYPTO_F_USER |
			flags;
	crp->crp_buf = (void *)&cse->uio;
	crp->crp_callback = (int (*) (struct cryptop *)) cryptodev_cb;
	crp->crp_sid = cse->sid;
	crp->crp_opaque = (void *)cse;

	if (cop->iv) {
		if (crde == NULL) {
			error = EINVAL;
			goto bail;
		}
		if (cse->txform->ivsize == 0) {
			error = EINVAL;
			goto bail;
		}
		if ((error = copyin(cop->iv, cse->tmp_iv,
		    cse->txform->ivsize)))
			goto bail;
		(void)memcpy(crde->crd_iv, cse->tmp_iv, cse->txform->ivsize);
		crde->crd_flags |= CRD_F_IV_EXPLICIT | CRD_F_IV_PRESENT;
		crde->crd_skip = 0;
	} else if (crde) {
		if (cse->txform->ivsize == 0) {
			crde->crd_skip = 0;
		} else {
			if (!(crde->crd_flags & CRD_F_ENCRYPT))
				crde->crd_flags |= CRD_F_IV_PRESENT;
			crde->crd_skip = cse->txform->ivsize;
			crde->crd_len -= cse->txform->ivsize;
		}
	}

	if (cop->mac) {
		if (crda == NULL) {
			error = EINVAL;
			goto bail;
		}
		crp->crp_mac=cse->tmp_mac;
	}

	cv_init(&crp->crp_cv, "crydev");

	/*
	 * XXX there was a comment here which said that we went to
	 * XXX splcrypto() but needed to only if CRYPTO_F_CBIMM,
	 * XXX disabled on NetBSD since 1.6O due to a race condition.
	 * XXX But crypto_dispatch went to splcrypto() itself!  (And
	 * XXX now takes the crypto_mtx mutex itself).  We do, however,
	 * XXX need to hold the mutex across the call to cv_wait().
	 * XXX     (should we arrange for crypto_dispatch to return to
	 * XXX      us with it held?  it seems quite ugly to do so.)
	 */
#ifdef notyet
eagain:
#endif
	error = crypto_dispatch(crp);
	mutex_enter(&crypto_mtx);

	/* 
	 * If the request was going to be completed by the
	 * ioctl thread then it would have been done by now.
	 * Remove the F_USER flag so crypto_done() is not confused
	 * if the crypto device calls it after this point.
	 */
	crp->crp_flags &= ~(CRYPTO_F_USER);

	switch (error) {
#ifdef notyet	/* don't loop forever -- but EAGAIN not possible here yet */
	case EAGAIN:
		mutex_exit(&crypto_mtx);
		goto eagain;
		break;
#endif
	case 0:
		break;
	default:
		DPRINTF(("cryptodev_op: not waiting, error.\n"));
		mutex_exit(&crypto_mtx);
		cv_destroy(&crp->crp_cv);
		goto bail;
	}

	while (!(crp->crp_flags & CRYPTO_F_DONE)) {
		DPRINTF(("cryptodev_op[%d]: sleeping on cv %p for crp %p\n",
			(uint32_t)cse->sid, &crp->crp_cv, crp));
		cv_wait(&crp->crp_cv, &crypto_mtx);	/* XXX cv_wait_sig? */
	}
	if (crp->crp_flags & CRYPTO_F_ONRETQ) {
		/* XXX this should never happen now with the CRYPTO_F_USER flag
		 * changes.
		 */
		DPRINTF(("cryptodev_op: DONE, not woken by cryptoret.\n"));
		(void)crypto_ret_q_remove(crp);
	}
	mutex_exit(&crypto_mtx);
	cv_destroy(&crp->crp_cv);

	if (crp->crp_etype != 0) {
		DPRINTF(("cryptodev_op: crp_etype %d\n", crp->crp_etype));
		error = crp->crp_etype;
		goto bail;
	}

	if (cse->error) {
		DPRINTF(("cryptodev_op: cse->error %d\n", cse->error));
		error = cse->error;
		goto bail;
	}

	dst_len = crp->crp_ilen;
	/* let the user know how much data was returned */
	if (crp->crp_olen) {
		if (crp->crp_olen > (cop->dst_len ? cop->dst_len : cop->len)) {
			error = ENOSPC;
			goto bail;
		}
		dst_len = cop->dst_len = crp->crp_olen;
	}

	if (cop->dst) {
		DPRINTF(("cryptodev_op: copyout %d bytes to %p\n", dst_len, cop->dst));
	}
	if (cop->dst &&
	    (error = copyout(cse->uio.uio_iov[0].iov_base, cop->dst, dst_len)))
	{
		DPRINTF(("cryptodev_op: copyout error %d\n", error));
		goto bail;
	}

	if (cop->mac &&
	    (error = copyout(crp->crp_mac, cop->mac, cse->thash->authsize))) {
		DPRINTF(("cryptodev_op: mac copyout error %d\n", error));
		goto bail;
	}


bail:
	if (crp) {
		crypto_freereq(crp);
	}
	if (cse->uio.uio_iov[0].iov_base) {
		kmem_free(cse->uio.uio_iov[0].iov_base,iov_len);
	}

	return error;
}

static int
cryptodev_cb(void *op)
{
	struct cryptop *crp = (struct cryptop *) op;
	struct csession *cse = (struct csession *)crp->crp_opaque;
	int error = 0;

	mutex_enter(&crypto_mtx);
	cse->error = crp->crp_etype;
	if (crp->crp_etype == EAGAIN) {
		/* always drop mutex to call dispatch routine */
		mutex_exit(&crypto_mtx);
		error = crypto_dispatch(crp);
		mutex_enter(&crypto_mtx);
	}
	if (error != 0 || (crp->crp_flags & CRYPTO_F_DONE)) {
		cv_signal(&crp->crp_cv);
	}
	mutex_exit(&crypto_mtx);
	return 0;
}

static int
cryptodev_mcb(void *op)
{
	struct cryptop *crp = (struct cryptop *) op;
	struct csession *cse = (struct csession *)crp->crp_opaque;
	int  error=0;

	mutex_enter(&crypto_mtx);
	cse->error = crp->crp_etype;
	if (crp->crp_etype == EAGAIN) {
		mutex_exit(&crypto_mtx);
		error = crypto_dispatch(crp);
		mutex_enter(&crypto_mtx);
	}
	if (error != 0 || (crp->crp_flags & CRYPTO_F_DONE)) {
		cv_signal(&crp->crp_cv);
	}

	TAILQ_INSERT_TAIL(&crp->fcrp->crp_ret_mq, crp, crp_next);
	selnotify(&crp->fcrp->sinfo, 0, 0);
	mutex_exit(&crypto_mtx);
	return 0;
}

static int
cryptodevkey_cb(void *op)
{
	struct cryptkop *krp = op;
	
	mutex_enter(&crypto_mtx);
	cv_signal(&krp->krp_cv);
	mutex_exit(&crypto_mtx);
	return 0;
}

static int
cryptodevkey_mcb(void *op)
{
	struct cryptkop *krp = op;

	mutex_enter(&crypto_mtx);
	cv_signal(&krp->krp_cv);
	TAILQ_INSERT_TAIL(&krp->fcrp->crp_ret_mkq, krp, krp_next);
	selnotify(&krp->fcrp->sinfo, 0, 0);
	mutex_exit(&crypto_mtx);
	return 0;
}

static int
cryptodev_key(struct crypt_kop *kop)
{
	struct cryptkop *krp = NULL;
	int error = EINVAL;
	int in, out, size, i;

	if (kop->crk_iparams + kop->crk_oparams > CRK_MAXPARAM)
		return EFBIG;

	in = kop->crk_iparams;
	out = kop->crk_oparams;
	switch (kop->crk_op) {
	case CRK_MOD_EXP:
		if (in == 3 && out == 1)
			break;
		return EINVAL;
	case CRK_MOD_EXP_CRT:
		if (in == 6 && out == 1)
			break;
		return EINVAL;
	case CRK_DSA_SIGN:
		if (in == 5 && out == 2)
			break;
		return EINVAL;
	case CRK_DSA_VERIFY:
		if (in == 7 && out == 0)
			break;
		return EINVAL;
	case CRK_DH_COMPUTE_KEY:
		if (in == 3 && out == 1)
			break;
		return EINVAL;
	case CRK_MOD_ADD:
		if (in == 3 && out == 1)
			break;
		return EINVAL;
	case CRK_MOD_ADDINV:
		if (in == 2 && out == 1)
			break;
		return EINVAL;
	case CRK_MOD_SUB:
		if (in == 3 && out == 1)
			break;
		return EINVAL;
	case CRK_MOD_MULT:
		if (in == 3 && out == 1)
			break;
		return EINVAL;
	case CRK_MOD_MULTINV:
		if (in == 2 && out == 1)
			break;
		return EINVAL;
	case CRK_MOD:
		if (in == 2 && out == 1)
			break;
		return EINVAL;
	default:
		return EINVAL;
	}

	krp = pool_get(&cryptkop_pool, PR_WAITOK);
	(void)memset(krp, 0, sizeof *krp);
	cv_init(&krp->krp_cv, "crykdev");
	krp->krp_op = kop->crk_op;
	krp->krp_status = kop->crk_status;
	krp->krp_iparams = kop->crk_iparams;
	krp->krp_oparams = kop->crk_oparams;
	krp->krp_status = 0;
	krp->krp_callback = (int (*) (struct cryptkop *)) cryptodevkey_cb;

	for (i = 0; i < CRK_MAXPARAM; i++)
		krp->krp_param[i].crp_nbits = kop->crk_param[i].crp_nbits;
	for (i = 0; i < krp->krp_iparams + krp->krp_oparams; i++) {
		size = (krp->krp_param[i].crp_nbits + 7) / 8;
		if (size == 0)
			continue;
		krp->krp_param[i].crp_p = kmem_alloc(size, KM_SLEEP);
		if (i >= krp->krp_iparams)
			continue;
		error = copyin(kop->crk_param[i].crp_p,
		    krp->krp_param[i].crp_p, size);
		if (error)
			goto fail;
	}

	error = crypto_kdispatch(krp);
	if (error != 0) {
		goto fail;
	}

	mutex_enter(&crypto_mtx);
	while (!(krp->krp_flags & CRYPTO_F_DONE)) {
		cv_wait(&krp->krp_cv, &crypto_mtx);	/* XXX cv_wait_sig? */
	}
	if (krp->krp_flags & CRYPTO_F_ONRETQ) {
		DPRINTF(("cryptodev_key: DONE early, not via cryptoret.\n"));
		(void)crypto_ret_kq_remove(krp);
	}
	mutex_exit(&crypto_mtx);

	if (krp->krp_status != 0) {
		DPRINTF(("cryptodev_key: krp->krp_status 0x%08x\n",
		    krp->krp_status));
		error = krp->krp_status;
		goto fail;
	}

	for (i = krp->krp_iparams; i < krp->krp_iparams + krp->krp_oparams;
	    i++) {
		size = (krp->krp_param[i].crp_nbits + 7) / 8;
		if (size == 0)
			continue;
		error = copyout(krp->krp_param[i].crp_p,
		    kop->crk_param[i].crp_p, size);
		if (error) {
			DPRINTF(("cryptodev_key: copyout oparam %d failed, "
			    "error=%d\n", i-krp->krp_iparams, error));
			goto fail;
		}
	}

fail:
	kop->crk_status = krp->krp_status;
	for (i = 0; i < CRK_MAXPARAM; i++) {
		struct crparam *kp = &(krp->krp_param[i]);
		if (krp->krp_param[i].crp_p) {
			size = (kp->crp_nbits + 7)  / 8;
			KASSERT(size > 0);
			(void)memset(kp->crp_p, 0, size);
			kmem_free(kp->crp_p, size);
		}
	}
	cv_destroy(&krp->krp_cv);
	pool_put(&cryptkop_pool, krp);
	DPRINTF(("cryptodev_key: error=0x%08x\n", error));
	return error;
}

/* ARGSUSED */
static int
cryptof_close(struct file *fp)
{
	struct fcrypt *fcr = fp->f_fcrypt;
	struct csession *cse;

	mutex_enter(&crypto_mtx);
	while ((cse = TAILQ_FIRST(&fcr->csessions))) {
		TAILQ_REMOVE(&fcr->csessions, cse, next);
		mutex_exit(&crypto_mtx);
		(void)csefree(cse);
		mutex_enter(&crypto_mtx);
	}
	seldestroy(&fcr->sinfo);
	fp->f_fcrypt = NULL;
	crypto_refcount--;
	mutex_exit(&crypto_mtx);

	pool_put(&fcrpl, fcr);
	return 0;
}

/* needed for compatibility module */
struct	csession *cryptodev_csefind(struct fcrypt *fcr, u_int ses)
{
	return csefind(fcr, ses);
}

/* csefind: call with crypto_mtx held. */
static struct csession *
csefind(struct fcrypt *fcr, u_int ses)
{
	struct csession *cse, *cnext, *ret = NULL;

	KASSERT(mutex_owned(&crypto_mtx));
	TAILQ_FOREACH_SAFE(cse, &fcr->csessions, next, cnext)
		if (cse->ses == ses)
			ret = cse;
	
	return ret;
}

/* csedelete: call with crypto_mtx held. */
static int
csedelete(struct fcrypt *fcr, struct csession *cse_del)
{
	struct csession *cse, *cnext;
	int ret = 0;

	KASSERT(mutex_owned(&crypto_mtx));
	TAILQ_FOREACH_SAFE(cse, &fcr->csessions, next, cnext) {
		if (cse == cse_del) {
			TAILQ_REMOVE(&fcr->csessions, cse, next);
			ret = 1;
		}
	}
	return ret;
}

static struct csession *
cseadd(struct fcrypt *fcr, struct csession *cse)
{
	mutex_enter(&crypto_mtx);
	/* don't let session ID wrap! */
	if (fcr->sesn + 1 == 0) return NULL;
	TAILQ_INSERT_TAIL(&fcr->csessions, cse, next);
	cse->ses = fcr->sesn++;
	mutex_exit(&crypto_mtx);
	return cse;
}

static struct csession *
csecreate(struct fcrypt *fcr, u_int64_t sid, void *key, u_int64_t keylen,
    void *mackey, u_int64_t mackeylen, u_int32_t cipher, u_int32_t mac,
    u_int32_t comp_alg, const struct enc_xform *txform,
    const struct auth_hash *thash, const struct comp_algo *tcomp)
{
	struct csession *cse;

	cse = pool_get(&csepl, PR_NOWAIT);
	if (cse == NULL)
		return NULL;
	cse->key = key;
	cse->keylen = keylen/8;
	cse->mackey = mackey;
	cse->mackeylen = mackeylen/8;
	cse->sid = sid;
	cse->cipher = cipher;
	cse->mac = mac;
	cse->comp_alg = comp_alg;
	cse->txform = txform;
	cse->thash = thash;
	cse->tcomp = tcomp;
	cse->error = 0;
	if (cseadd(fcr, cse))
		return cse;
	else {
		pool_put(&csepl, cse);
		return NULL;
	}
}

/* csefree: call with crypto_mtx held. */
static int
csefree(struct csession *cse)
{
	int error;

	error = crypto_freesession(cse->sid);
	if (cse->key)
		free(cse->key, M_XDATA);
	if (cse->mackey)
		free(cse->mackey, M_XDATA);
	pool_put(&csepl, cse);
	return error;
}

static int
cryptoopen(dev_t dev, int flag, int mode,
    struct lwp *l)
{
	file_t *fp;
        struct fcrypt *fcr;
        int fd, error;

	if (crypto_usercrypto == 0)
		return ENXIO;

	if ((error = fd_allocfile(&fp, &fd)) != 0)
		return error;

	fcr = pool_get(&fcrpl, PR_WAITOK);
	getnanotime(&fcr->btime);
	fcr->atime = fcr->mtime = fcr->btime;
	mutex_enter(&crypto_mtx);
	TAILQ_INIT(&fcr->csessions);
	TAILQ_INIT(&fcr->crp_ret_mq);
	TAILQ_INIT(&fcr->crp_ret_mkq);
	selinit(&fcr->sinfo);
	/*
	 * Don't ever return session 0, to allow detection of
	 * failed creation attempts with multi-create ioctl.
	 */
	fcr->sesn = 1;
	fcr->requestid = 1;
	crypto_refcount++;
	mutex_exit(&crypto_mtx);
	return fd_clone(fp, fd, flag, &cryptofops, fcr);
}

static int
cryptoread(dev_t dev, struct uio *uio, int ioflag)
{
	return EIO;
}

static int
cryptowrite(dev_t dev, struct uio *uio, int ioflag)
{
	return EIO;
}

int
cryptoselect(dev_t dev, int rw, struct lwp *l)
{
	return 0;
}

/*static*/
struct cdevsw crypto_cdevsw = {
	.d_open = cryptoopen,
	.d_close = noclose,
	.d_read = cryptoread,
	.d_write = cryptowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = cryptoselect /*nopoll*/,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

int 
cryptodev_mop(struct fcrypt *fcr, 
              struct crypt_n_op * cnop,
              int count, struct lwp *l)
{
	struct cryptop *crp = NULL;
	struct cryptodesc *crde = NULL, *crda = NULL, *crdc = NULL;
	int req, error=0;
	struct csession *cse;
	int flags=0;
	int iov_len;

	for (req = 0; req < count; req++) {
		mutex_enter(&crypto_mtx);
		cse = csefind(fcr, cnop[req].ses);
		if (cse == NULL) {
			DPRINTF(("csefind failed\n"));
			cnop[req].status = EINVAL;
			mutex_exit(&crypto_mtx);
			continue;
		}
		mutex_exit(&crypto_mtx);
	
		if (cnop[req].len > 256*1024-4) {
			DPRINTF(("length failed\n"));
			cnop[req].status = EINVAL;
			continue;
		}
		if (cse->txform) {
			if (cnop[req].len < cse->txform->blocksize -
			    (cnop[req].iv ? 0 : cse->txform->ivsize) ||
			    (cnop[req].len -
			     (cnop[req].iv ? 0 : cse->txform->ivsize))
			    % cse->txform->blocksize) {
				cnop[req].status = EINVAL;
				continue;
			}
		}

		crp = crypto_getreq((cse->txform != NULL) +
				    (cse->thash != NULL) +
				    (cse->tcomp != NULL));
		if (crp == NULL) {
			cnop[req].status = ENOMEM;
			goto bail;
		}

		iov_len = cnop[req].len;
		/* got a compression/decompression max size? */
		if ((cse->tcomp) && cnop[req].dst_len) {
			if (iov_len < cnop[req].dst_len) {
				/* Need larger iov to deal with decompress */
				iov_len = cnop[req].dst_len;
			}
			DPRINTF(("cryptodev_mop: iov_len -> %d for decompress\n", iov_len));
		}

		(void)memset(&crp->uio, 0, sizeof(crp->uio));
		crp->uio.uio_iovcnt = 1;
		crp->uio.uio_resid = 0;
		crp->uio.uio_rw = UIO_WRITE;
		crp->uio.uio_iov = crp->iovec;
		UIO_SETUP_SYSSPACE(&crp->uio);
		memset(&crp->iovec, 0, sizeof(crp->iovec));
		crp->uio.uio_iov[0].iov_len = iov_len;
		DPRINTF(("cryptodev_mop: kmem_alloc(%d) for iov \n", iov_len));
		crp->uio.uio_iov[0].iov_base = kmem_alloc(iov_len, KM_SLEEP);
		crp->uio.uio_resid = crp->uio.uio_iov[0].iov_len;

		if (cse->tcomp) {
			crdc = crp->crp_desc;
		}

		if (cse->thash) {
			crda = crdc ? crdc->crd_next : crp->crp_desc;
			if (cse->txform && crda)
				crde = crda->crd_next;
		} else {
			if (cse->txform) {
				crde = crdc ? crdc->crd_next : crp->crp_desc;
			} else if (!cse->tcomp) {
				error = EINVAL;
				goto bail;
			}
		}

		if ((copyin(cnop[req].src, 
		    crp->uio.uio_iov[0].iov_base, cnop[req].len))) {
			cnop[req].status = EINVAL;
			goto bail;
		}

		if (crdc) {
			switch (cnop[req].op) {
			case COP_COMP:
				crdc->crd_flags |= CRD_F_COMP;
				break;
			case COP_DECOMP:
				crdc->crd_flags &= ~CRD_F_COMP;
				break;
			default:
				break;
			}
			/* more data to follow? */
			if (cnop[req].flags & COP_F_MORE) {
				flags |= CRYPTO_F_MORE;
			}
			crdc->crd_len = cnop[req].len;
			crdc->crd_inject = 0;

			crdc->crd_alg = cse->comp_alg;
			crdc->crd_key = NULL;
			crdc->crd_klen = 0;
			DPRINTF(("cryptodev_mop[%d]: crdc setup for comp_alg %d"
				 " len %d.\n",
				(uint32_t)cse->sid, crdc->crd_alg,
				crdc->crd_len));
		}
	
		if (crda) {
			crda->crd_skip = 0;
			crda->crd_len = cnop[req].len;
			crda->crd_inject = 0;	/* ??? */

			crda->crd_alg = cse->mac;
			crda->crd_key = cse->mackey;
			crda->crd_klen = cse->mackeylen * 8;
		}

		if (crde) {
			if (cnop[req].op == COP_ENCRYPT)
				crde->crd_flags |= CRD_F_ENCRYPT;
			else
				crde->crd_flags &= ~CRD_F_ENCRYPT;
			crde->crd_len = cnop[req].len;
			crde->crd_inject = 0;

			crde->crd_alg = cse->cipher;
#ifdef notyet		/* XXX must notify h/w driver new key, drain */
			if(cnop[req].key && cnop[req].keylen) {
				crde->crd_key = malloc(cnop[req].keylen,
						    M_XDATA, M_WAITOK);
				if((error = copyin(cnop[req].key, 
				    crde->crd_key, cnop[req].keylen))) {
					cnop[req].status = EINVAL;
					goto bail;
				}
				crde->crd_klen =  cnop[req].keylen * 8;
			} else { ... }
#endif
			crde->crd_key = cse->key;
			crde->crd_klen = cse->keylen * 8;
		}

		crp->crp_ilen = cnop[req].len;
		crp->crp_flags = CRYPTO_F_IOV | CRYPTO_F_CBIMM |
		    (cnop[req].flags & COP_F_BATCH) | flags;
		crp->crp_buf = (void *)&crp->uio;
		crp->crp_callback = (int (*) (struct cryptop *)) cryptodev_mcb;
		crp->crp_sid = cse->sid;
		crp->crp_opaque = (void *)cse;
		crp->fcrp = fcr;
		crp->dst = cnop[req].dst;
		crp->len = cnop[req].len; /* input len, iov may be larger */
		crp->mac = cnop[req].mac;
		DPRINTF(("cryptodev_mop: iov_base %p dst %p len %d mac %p\n",
			    crp->uio.uio_iov[0].iov_base, crp->dst, crp->len,
			    crp->mac));

		if (cnop[req].iv) {
			if (crde == NULL) {
				cnop[req].status = EINVAL;
				goto bail;
			}
			if (cse->cipher == CRYPTO_ARC4) { /* XXX use flag? */
				cnop[req].status = EINVAL;
				goto bail;
			}
			if ((error = copyin(cnop[req].iv, crp->tmp_iv,
			    cse->txform->ivsize))) {
				cnop[req].status = EINVAL;
				goto bail;
			}
			(void)memcpy(crde->crd_iv, crp->tmp_iv,
			    cse->txform->ivsize);
			crde->crd_flags |= CRD_F_IV_EXPLICIT | CRD_F_IV_PRESENT;
			crde->crd_skip = 0;
		} else if (crde) {
			if (cse->cipher == CRYPTO_ARC4) { /* XXX use flag? */
				crde->crd_skip = 0;
			} else {
				if (!(crde->crd_flags & CRD_F_ENCRYPT))
					crde->crd_flags |= CRD_F_IV_PRESENT;
				crde->crd_skip = cse->txform->ivsize;
				crde->crd_len -= cse->txform->ivsize;
			}
		}
	
		if (cnop[req].mac) {
			if (crda == NULL) {
				cnop[req].status = EINVAL;
				goto bail;
			}
			crp->crp_mac=cse->tmp_mac;
		}
		cnop[req].reqid = atomic_inc_32_nv(&(fcr->requestid));
		crp->crp_reqid = cnop[req].reqid;
		crp->crp_usropaque = cnop[req].opaque;
		cv_init(&crp->crp_cv, "crydev");
#ifdef notyet
eagain:
#endif
		cnop[req].status = crypto_dispatch(crp);
		mutex_enter(&crypto_mtx);	/* XXX why mutex? */

		switch (cnop[req].status) {
#ifdef notyet	/* don't loop forever -- but EAGAIN not possible here yet */
		case EAGAIN:
			mutex_exit(&crypto_mtx);
			goto eagain;
			break;
#endif
		case 0:
			break;
		default:
			DPRINTF(("cryptodev_op: not waiting, error.\n"));
			mutex_exit(&crypto_mtx);
			cv_destroy(&crp->crp_cv);
			goto bail;
		}

		mutex_exit(&crypto_mtx);
		cv_destroy(&crp->crp_cv);
bail:
		if (cnop[req].status) {
			if (crp) {
				if (crp->uio.uio_iov[0].iov_base) {
					kmem_free(crp->uio.uio_iov[0].iov_base,
					    crp->uio.uio_iov[0].iov_len);
				}
				crypto_freereq(crp);
			}
			error = 0;
		}
	}
	return error;
}

static int
cryptodev_mkey(struct fcrypt *fcr, struct crypt_n_kop *kop, int count)
{
	struct cryptkop *krp = NULL;
	int error = EINVAL;
	int in, out, size, i, req;

	for (req = 0; req < count; req++) {
		if (kop[req].crk_iparams + kop[req].crk_oparams > CRK_MAXPARAM)
			return EFBIG;

		in = kop[req].crk_iparams;
		out = kop[req].crk_oparams;
		switch (kop[req].crk_op) {
		case CRK_MOD_EXP:
			if (in == 3 && out == 1)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_MOD_EXP_CRT:
			if (in == 6 && out == 1)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_DSA_SIGN:
			if (in == 5 && out == 2)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_DSA_VERIFY:
			if (in == 7 && out == 0)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_DH_COMPUTE_KEY:
			if (in == 3 && out == 1)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_MOD_ADD:
			if (in == 3 && out == 1)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_MOD_ADDINV:
			if (in == 2 && out == 1)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_MOD_SUB:
			if (in == 3 && out == 1)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_MOD_MULT:
			if (in == 3 && out == 1)
				break;	
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_MOD_MULTINV:
			if (in == 2 && out == 1)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		case CRK_MOD:
			if (in == 2 && out == 1)
				break;
			kop[req].crk_status = EINVAL;
			continue;
		default:
			kop[req].crk_status = EINVAL;
			continue;
		}

		krp = pool_get(&cryptkop_pool, PR_WAITOK);
		(void)memset(krp, 0, sizeof *krp);
		cv_init(&krp->krp_cv, "crykdev");
		krp->krp_op = kop[req].crk_op;
		krp->krp_status = kop[req].crk_status;
		krp->krp_iparams = kop[req].crk_iparams;
		krp->krp_oparams = kop[req].crk_oparams;
		krp->krp_status = 0;
		krp->krp_callback =
		    (int (*) (struct cryptkop *)) cryptodevkey_mcb;
		(void)memcpy(krp->crk_param, kop[req].crk_param,
		    sizeof(kop[req].crk_param));

		krp->krp_flags = CRYPTO_F_CBIMM;

		for (i = 0; i < CRK_MAXPARAM; i++)
			krp->krp_param[i].crp_nbits =
			    kop[req].crk_param[i].crp_nbits;
		for (i = 0; i < krp->krp_iparams + krp->krp_oparams; i++) {
			size = (krp->krp_param[i].crp_nbits + 7) / 8;
			if (size == 0)
				continue;
			krp->krp_param[i].crp_p =
			    kmem_alloc(size, KM_SLEEP);
			if (i >= krp->krp_iparams)
				continue;
			kop[req].crk_status =
			    copyin(kop[req].crk_param[i].crp_p,
			    krp->krp_param[i].crp_p, size);
			if (kop[req].crk_status)
				goto fail;
		}
		krp->fcrp = fcr;

		kop[req].crk_reqid = atomic_inc_32_nv(&(fcr->requestid));
		krp->krp_reqid = kop[req].crk_reqid;
		krp->krp_usropaque = kop[req].crk_opaque;

		kop[req].crk_status = crypto_kdispatch(krp);
		if (kop[req].crk_status != 0) {
			goto fail;
		}

fail:
		if(kop[req].crk_status) {
			if (krp) {
				kop[req].crk_status = krp->krp_status;
				for (i = 0; i < CRK_MAXPARAM; i++) {
					struct crparam *kp =
						&(krp->krp_param[i]);
					if (kp->crp_p) {
						size = (kp->crp_nbits + 7) / 8;
						KASSERT(size > 0);
						memset(kp->crp_p, 0, size);
						kmem_free(kp->crp_p, size);
					}
				}
				cv_destroy(&krp->krp_cv);
				pool_put(&cryptkop_pool, krp);
			}
		}
		error = 0;
	}
	DPRINTF(("cryptodev_key: error=0x%08x\n", error));
	return error;
}

int
cryptodev_session(struct fcrypt *fcr, struct session_op *sop) 
{
	struct cryptoini cria, crie;
	struct cryptoini cric;		/* compressor */
	struct cryptoini *crihead = NULL;
	const struct enc_xform *txform = NULL;
	const struct auth_hash *thash = NULL;
	const struct comp_algo *tcomp = NULL;
	struct csession *cse;
	u_int64_t sid;
	int error = 0;

	DPRINTF(("cryptodev_session() cipher=%d, mac=%d\n", sop->cipher, sop->mac));

	/* XXX there must be a way to not embed the list of xforms here */
	switch (sop->cipher) {
	case 0:
		break;
	case CRYPTO_DES_CBC:
		txform = &enc_xform_des;
		break;
	case CRYPTO_3DES_CBC:
		txform = &enc_xform_3des;
		break;
	case CRYPTO_BLF_CBC:
		txform = &enc_xform_blf;
		break;
	case CRYPTO_CAST_CBC:
		txform = &enc_xform_cast5;
		break;
	case CRYPTO_SKIPJACK_CBC:
		txform = &enc_xform_skipjack;
		break;
	case CRYPTO_AES_CBC:
		txform = &enc_xform_rijndael128;
		break;
	case CRYPTO_CAMELLIA_CBC:
		txform = &enc_xform_camellia;
		break;
	case CRYPTO_AES_CTR:
		txform = &enc_xform_aes_ctr;
		break;
	case CRYPTO_AES_GCM_16:
		txform = &enc_xform_aes_gcm;
		break;
	case CRYPTO_AES_GMAC:
		txform = &enc_xform_aes_gmac;
		break;
	case CRYPTO_NULL_CBC:
		txform = &enc_xform_null;
		break;
	case CRYPTO_ARC4:
		txform = &enc_xform_arc4;
		break;
	default:
		DPRINTF(("Invalid cipher %d\n", sop->cipher));
		return EINVAL;
	}

	switch (sop->comp_alg) {
	case 0:
		break;
	case CRYPTO_DEFLATE_COMP:
		tcomp = &comp_algo_deflate;
		break;
	case CRYPTO_GZIP_COMP:
		tcomp = &comp_algo_gzip;
		DPRINTF(("cryptodev_session() tcomp for GZIP\n"));
		break;
	default:
		DPRINTF(("Invalid compression alg %d\n", sop->comp_alg));
		return EINVAL;
	}

	switch (sop->mac) {
	case 0:
		break;
	case CRYPTO_MD5_HMAC:
		thash = &auth_hash_hmac_md5;
		break;
	case CRYPTO_SHA1_HMAC:
		thash = &auth_hash_hmac_sha1;
		break;
	case CRYPTO_MD5_HMAC_96:
		thash = &auth_hash_hmac_md5_96;
		break;
	case CRYPTO_SHA1_HMAC_96:
		thash = &auth_hash_hmac_sha1_96;
		break;
	case CRYPTO_SHA2_HMAC:
		/* XXX switching on key length seems questionable */
		if (sop->mackeylen == auth_hash_hmac_sha2_256.keysize) {
			thash = &auth_hash_hmac_sha2_256;
		} else if (sop->mackeylen == auth_hash_hmac_sha2_384.keysize) {
			thash = &auth_hash_hmac_sha2_384;
		} else if (sop->mackeylen == auth_hash_hmac_sha2_512.keysize) {
			thash = &auth_hash_hmac_sha2_512;
		} else {
			DPRINTF(("Invalid mackeylen %d\n", sop->mackeylen));
			return EINVAL;
		}
		break;
	case CRYPTO_RIPEMD160_HMAC:
		thash = &auth_hash_hmac_ripemd_160;
		break;
	case CRYPTO_RIPEMD160_HMAC_96:
		thash = &auth_hash_hmac_ripemd_160_96;
		break;
	case CRYPTO_MD5:
		thash = &auth_hash_md5;
		break;
	case CRYPTO_SHA1:
		thash = &auth_hash_sha1;
		break;
	case CRYPTO_AES_XCBC_MAC_96:
		thash = &auth_hash_aes_xcbc_mac_96;
		break;
	case CRYPTO_AES_128_GMAC:
		thash = &auth_hash_gmac_aes_128;
		break;
	case CRYPTO_AES_192_GMAC:
		thash = &auth_hash_gmac_aes_192;
		break;
	case CRYPTO_AES_256_GMAC:
		thash = &auth_hash_gmac_aes_256;
		break;
	case CRYPTO_NULL_HMAC:
		thash = &auth_hash_null;
		break;
	default:
		DPRINTF(("Invalid mac %d\n", sop->mac));
		return EINVAL;
	}

	memset(&crie, 0, sizeof(crie));
	memset(&cria, 0, sizeof(cria));
	memset(&cric, 0, sizeof(cric));

	if (tcomp) {
		cric.cri_alg = tcomp->type;
		cric.cri_klen = 0;
		DPRINTF(("tcomp->type = %d\n", tcomp->type));

		crihead = &cric;
		if (txform) {
			cric.cri_next = &crie;
		} else if (thash) {
			cric.cri_next = &cria;
		}
	}

	if (txform) {
		crie.cri_alg = txform->type;
		crie.cri_klen = sop->keylen * 8;
		if (sop->keylen > txform->maxkey ||
		    sop->keylen < txform->minkey) {
			DPRINTF(("keylen %d not in [%d,%d]\n",
			    sop->keylen, txform->minkey, txform->maxkey));
			error = EINVAL;
			goto bail;
		}

		crie.cri_key = malloc(crie.cri_klen / 8, M_XDATA, M_WAITOK);
		if ((error = copyin(sop->key, crie.cri_key, crie.cri_klen / 8)))
			goto bail;
		if (!crihead) {
			crihead = &crie;
		}
		if (thash)
			crie.cri_next = &cria;
	} 

	if (thash) {
		cria.cri_alg = thash->type;
		cria.cri_klen = sop->mackeylen * 8;
		if (sop->mackeylen != thash->keysize) {
			DPRINTF(("mackeylen %d != keysize %d\n",
			    sop->mackeylen, thash->keysize));
			error = EINVAL;
			goto bail;
		}
		if (cria.cri_klen) {
			cria.cri_key = malloc(cria.cri_klen / 8, M_XDATA,
			    M_WAITOK);
			if ((error = copyin(sop->mackey, cria.cri_key,
			    cria.cri_klen / 8))) {
				goto bail;
			}
		}
		if (!crihead) {
			crihead = &cria;
		}
	}

	error = crypto_newsession(&sid, crihead, crypto_devallowsoft);
	if (!error) {
		DPRINTF(("cryptodev_session: got session %d\n", (uint32_t)sid));
		cse = csecreate(fcr, sid, crie.cri_key, crie.cri_klen,
		    cria.cri_key, cria.cri_klen, (txform ? sop->cipher : 0), sop->mac,
		    (tcomp ? sop->comp_alg : 0), txform, thash, tcomp);
		if (cse != NULL) {
			sop->ses = cse->ses;
		} else {
			DPRINTF(("csecreate failed\n"));
			crypto_freesession(sid);
			error = EINVAL;
		}
	} else {
		DPRINTF(("SIOCSESSION violates kernel parameters %d\n",
		    error));
	}
bail:
	if (error) {
		if (crie.cri_key) {
			memset(crie.cri_key, 0, crie.cri_klen / 8);
			free(crie.cri_key, M_XDATA);
		}
		if (cria.cri_key) {
			memset(cria.cri_key, 0, cria.cri_klen / 8);
			free(cria.cri_key, M_XDATA);
		}
	}
	return error;
}

int
cryptodev_msession(struct fcrypt *fcr, struct session_n_op *sn_ops,
		   int count)
{
	int i;

	for (i = 0; i < count; i++, sn_ops++) {
		struct session_op s_op;
		s_op.cipher =		sn_ops->cipher;
		s_op.mac =		sn_ops->mac;
		s_op.keylen =		sn_ops->keylen;
		s_op.key =		sn_ops->key;
		s_op.mackeylen =	sn_ops->mackeylen;
		s_op.mackey =		sn_ops->mackey;

		sn_ops->status = cryptodev_session(fcr, &s_op);
		sn_ops->ses =		s_op.ses;
	}

	return 0;
}

static int
cryptodev_msessionfin(struct fcrypt *fcr, int count, u_int32_t *sesid)
{
	struct csession *cse;
	int req, error = 0;

	mutex_enter(&crypto_mtx);
	for(req = 0; req < count; req++) {
		cse = csefind(fcr, sesid[req]);
		if (cse == NULL)
			continue;
		csedelete(fcr, cse);
		mutex_exit(&crypto_mtx);
		error = csefree(cse);
		mutex_enter(&crypto_mtx);
	}
	mutex_exit(&crypto_mtx);
	return error;
}

/*
 * collect as many completed requests as are availble, or count completed
 * requests whichever is less.
 * return the number of requests.
 */
static int
cryptodev_getmstatus(struct fcrypt *fcr, struct crypt_result *crypt_res,
    int count)
{
	struct cryptop *crp = NULL;
	struct cryptkop *krp = NULL;
	struct csession *cse;
	int i, size, req = 0;
	int completed=0;

	/* On queue so nobody else can grab them
	 * and copyout can be delayed-- no locking */
	TAILQ_HEAD(, cryptop) crp_delfree_q = 
		TAILQ_HEAD_INITIALIZER(crp_delfree_q);
	TAILQ_HEAD(, cryptkop) krp_delfree_q = 
		TAILQ_HEAD_INITIALIZER(krp_delfree_q);

	/* at this point we do not know which response user is requesting for 
	 * (symmetric or asymmetric) so we copyout one from each i.e if the 
	 * count is 2 then 1 from symmetric and 1 from asymmetric queue and 
	 * if 3 then 2 symmetric and 1 asymmetric and so on */

	/* pull off a list of requests while protected from changes */
	mutex_enter(&crypto_mtx);
	while (req < count) {
		crp = TAILQ_FIRST(&fcr->crp_ret_mq);
		if (crp) {
			TAILQ_REMOVE(&fcr->crp_ret_mq, crp, crp_next);
			TAILQ_INSERT_TAIL(&crp_delfree_q, crp, crp_next);
			cse = (struct csession *)crp->crp_opaque;

			/* see if the session is still valid */
			cse = csefind(fcr, cse->ses);
			if (cse != NULL) {
				crypt_res[req].status = 0;
			} else {
				DPRINTF(("csefind failed\n"));
				crypt_res[req].status = EINVAL;
			}
			req++;
		}
		if(req < count) {
			crypt_res[req].status = 0;
			krp = TAILQ_FIRST(&fcr->crp_ret_mkq);
			if (krp) {
				TAILQ_REMOVE(&fcr->crp_ret_mkq, krp, krp_next);
				TAILQ_INSERT_TAIL(&krp_delfree_q, krp, krp_next);
			req++;
			}
		}
	}
	mutex_exit(&crypto_mtx);

	/* now do all the work outside the mutex */
	for(req=0; req < count ;) {
		crp = TAILQ_FIRST(&crp_delfree_q);
		if (crp) {
			if (crypt_res[req].status != 0) {
				/* csefind failed during collection */
				goto bail;
			}
			cse = (struct csession *)crp->crp_opaque;
			crypt_res[req].reqid = crp->crp_reqid;
			crypt_res[req].opaque = crp->crp_usropaque;
			completed++;
			
			if (crp->crp_etype != 0) {
				crypt_res[req].status = crp->crp_etype;
				goto bail;
			}

			if (cse->error) {
				crypt_res[req].status = cse->error;
				goto bail;
			}

			if (crp->dst && (crypt_res[req].status =
			    copyout(crp->uio.uio_iov[0].iov_base, crp->dst,
			    crp->len)))
				goto bail;

			if (crp->mac && (crypt_res[req].status =
			    copyout(crp->crp_mac, crp->mac,
			    cse->thash->authsize)))
				goto bail;

bail:
			TAILQ_REMOVE(&crp_delfree_q, crp, crp_next);
			kmem_free(crp->uio.uio_iov[0].iov_base,
			    crp->uio.uio_iov[0].iov_len);
			crypto_freereq(crp);
			req++;
		}

		if (req < count) {
			krp = TAILQ_FIRST(&krp_delfree_q);
			if (krp) {
				crypt_res[req].reqid = krp->krp_reqid;
				crypt_res[req].opaque = krp->krp_usropaque;
				completed++;
				if (krp->krp_status != 0) {
					DPRINTF(("cryptodev_key: "
					    "krp->krp_status 0x%08x\n",
					    krp->krp_status));
					crypt_res[req].status = krp->krp_status;
					goto fail;
				}

				for (i = krp->krp_iparams; i < krp->krp_iparams
				    + krp->krp_oparams; i++) {
					size = (krp->krp_param[i].crp_nbits
					    + 7) / 8;
					if (size == 0)
						continue;
					crypt_res[req].status = copyout
					    (krp->krp_param[i].crp_p,
					    krp->crk_param[i].crp_p, size);
					if (crypt_res[req].status) {
						DPRINTF(("cryptodev_key: "
						    "copyout oparam %d failed, "
						    "error=%d\n",
						    i - krp->krp_iparams, 
						    crypt_res[req].status));
						goto fail;
					}
				}
fail:
				TAILQ_REMOVE(&krp_delfree_q, krp, krp_next);
				/* not sure what to do for this */
				/* kop[req].crk_status = krp->krp_status; */ 
				for (i = 0; i < CRK_MAXPARAM; i++) {
					struct crparam *kp = &(krp->krp_param[i]);
					if (kp->crp_p) {
						size = (kp->crp_nbits + 7) / 8;
						KASSERT(size > 0);
						(void)memset(kp->crp_p, 0, size);
						kmem_free(kp->crp_p, size);
					}
				}
				cv_destroy(&krp->krp_cv);
				pool_put(&cryptkop_pool, krp);
				req++;
			}
		}
	}

	return completed;	
}

static int
cryptodev_getstatus (struct fcrypt *fcr, struct crypt_result *crypt_res)
{
        struct cryptop *crp = NULL, *cnext;
        struct cryptkop *krp = NULL, *knext;
        struct csession *cse;
        int i, size, req = 0;

	mutex_enter(&crypto_mtx);		
	/* Here we dont know for which request the user is requesting the 
	 * response so checking in both the queues */
	TAILQ_FOREACH_SAFE(crp, &fcr->crp_ret_mq, crp_next, cnext) {
		if(crp && (crp->crp_reqid == crypt_res->reqid)) {
			cse = (struct csession *)crp->crp_opaque;
		        crypt_res->opaque = crp->crp_usropaque;
			cse = csefind(fcr, cse->ses);
			if (cse == NULL) {
				DPRINTF(("csefind failed\n"));
				crypt_res->status = EINVAL;
				goto bail;
			}

			if (crp->crp_etype != 0) {
				crypt_res->status = crp->crp_etype;
				goto bail;
			}

			if (cse->error) {
				crypt_res->status = cse->error;
				goto bail;
			}

			if (crp->dst && (crypt_res->status =
			    copyout(crp->uio.uio_iov[0].iov_base, 
			    crp->dst, crp->len)))
				goto bail;
			
			if (crp->mac && (crypt_res->status =
			    copyout(crp->crp_mac, crp->mac,
			    cse->thash->authsize)))
				goto bail;
bail:
			TAILQ_REMOVE(&fcr->crp_ret_mq, crp, crp_next);

			mutex_exit(&crypto_mtx);
			crypto_freereq(crp);
			return 0;
		}
	}

	TAILQ_FOREACH_SAFE(krp, &fcr->crp_ret_mkq, krp_next, knext) {
		if(krp && (krp->krp_reqid == crypt_res->reqid)) {
			crypt_res[req].opaque = krp->krp_usropaque;
			if (krp->krp_status != 0) {
				DPRINTF(("cryptodev_key: "
				    "krp->krp_status 0x%08x\n", 
				    krp->krp_status));
				crypt_res[req].status = krp->krp_status;
				goto fail;
			}

			for (i = krp->krp_iparams; i < krp->krp_iparams +
			    krp->krp_oparams; i++) {
				size = (krp->krp_param[i].crp_nbits + 7) / 8;
				if (size == 0)
					continue;
				crypt_res[req].status = copyout(
				    krp->krp_param[i].crp_p, 
				    krp->crk_param[i].crp_p, size);
				if (crypt_res[req].status) {
					DPRINTF(("cryptodev_key: copyout oparam"
					    "%d failed, error=%d\n", 
					    i - krp->krp_iparams, 
					    crypt_res[req].status));
					goto fail;
				}
			}
fail:
			TAILQ_REMOVE(&fcr->crp_ret_mkq, krp, krp_next);
			mutex_exit(&crypto_mtx);
			/* not sure what to do for this */
			/* kop[req].crk_status = krp->krp_status; */ 
			for (i = 0; i < CRK_MAXPARAM; i++) {
				struct crparam *kp = &(krp->krp_param[i]);
				if (kp->crp_p) {
					size = (kp->crp_nbits + 7) / 8;
					KASSERT(size > 0);
					memset(kp->crp_p, 0, size);
					kmem_free(kp->crp_p, size);
				}
			}
			cv_destroy(&krp->krp_cv);
			pool_put(&cryptkop_pool, krp);
			return 0;
		}
	}
	mutex_exit(&crypto_mtx);
	return EINPROGRESS;			
}

static int      
cryptof_stat(struct file *fp, struct stat *st)
{
	struct fcrypt *fcr = fp->f_fcrypt;

	(void)memset(st, 0, sizeof(*st));

	mutex_enter(&crypto_mtx);
	st->st_dev = makedev(cdevsw_lookup_major(&crypto_cdevsw), fcr->sesn);
	st->st_atimespec = fcr->atime;
	st->st_mtimespec = fcr->mtime;
	st->st_ctimespec = st->st_birthtimespec = fcr->btime;
	st->st_uid = kauth_cred_geteuid(fp->f_cred);
	st->st_gid = kauth_cred_getegid(fp->f_cred);
	mutex_exit(&crypto_mtx);

	return 0;
}

static int      
cryptof_poll(struct file *fp, int events)
{
	struct fcrypt *fcr = fp->f_fcrypt;
	int revents = 0;

	if (!(events & (POLLIN | POLLRDNORM))) {
		/* only support read and POLLIN */
		return 0;
	}

	mutex_enter(&crypto_mtx);
	if (TAILQ_EMPTY(&fcr->crp_ret_mq) && TAILQ_EMPTY(&fcr->crp_ret_mkq)) {
		/* no completed requests pending, save the poll for later */
		selrecord(curlwp, &fcr->sinfo);
	} else {
		/* let the app(s) know that there are completed requests */
		revents = events & (POLLIN | POLLRDNORM);
	}
	mutex_exit(&crypto_mtx);

	return revents;
}

/*
 * Pseudo-device initialization routine for /dev/crypto
 */
void
cryptoattach(int num)
{
	crypto_init();

	pool_init(&fcrpl, sizeof(struct fcrypt), 0, 0, 0, "fcrpl",
	    NULL, IPL_NET);	/* XXX IPL_NET ("splcrypto") */
	pool_init(&csepl, sizeof(struct csession), 0, 0, 0, "csepl",
	    NULL, IPL_NET);	/* XXX IPL_NET ("splcrypto") */

	/*
	 * Preallocate space for 64 users, with 5 sessions each.
	 * (consider that a TLS protocol session requires at least
	 * 3DES, MD5, and SHA1 (both hashes are used in the PRF) for
	 * the negotiation, plus HMAC_SHA1 for the actual SSL records,
	 * consuming one session here for each algorithm.
	 */
	pool_prime(&fcrpl, 64);
	pool_prime(&csepl, 64 * 5);
}

void	crypto_attach(device_t, device_t, void *);

void
crypto_attach(device_t parent, device_t self, void * opaque)
{

	cryptoattach(0);
}

int	crypto_detach(device_t, int);

int
crypto_detach(device_t self, int num)
{

	pool_destroy(&fcrpl);
	pool_destroy(&csepl);

	return 0;
}

int crypto_match(device_t, cfdata_t, void *);
 
int
crypto_match(device_t parent, cfdata_t data, void *opaque) 
{   
 
	return 1;
}

MODULE(MODULE_CLASS_DRIVER, crypto, "opencrypto");

CFDRIVER_DECL(crypto, DV_DULL, NULL);

CFATTACH_DECL2_NEW(crypto, 0, crypto_match, crypto_attach, crypto_detach,
    NULL, NULL, NULL);

#ifdef _MODULE
static int cryptoloc[] = { -1, -1 };

static struct cfdata crypto_cfdata[] = {
	{
		.cf_name = "crypto",
		.cf_atname = "crypto",
		.cf_unit = 0,
		.cf_fstate = 0,
		.cf_loc = cryptoloc,
		.cf_flags = 0,
		.cf_pspec = NULL,
	},
	{ NULL, NULL, 0, 0, NULL, 0, NULL }
};
#endif

static int
crypto_modcmd(modcmd_t cmd, void *arg)
{
	int error = 0;
#ifdef _MODULE
	devmajor_t cmajor = NODEVMAJOR, bmajor = NODEVMAJOR;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE

		error = config_cfdriver_attach(&crypto_cd);
		if (error) {
			return error;
		}

		error = config_cfattach_attach(crypto_cd.cd_name, &crypto_ca);
		if (error) {
			config_cfdriver_detach(&crypto_cd);
			aprint_error("%s: unable to register cfattach\n",
				crypto_cd.cd_name);

			return error;
		}

		error = config_cfdata_attach(crypto_cfdata, 1);
		if (error) {
			config_cfattach_detach(crypto_cd.cd_name, &crypto_ca);
			config_cfdriver_detach(&crypto_cd);
			aprint_error("%s: unable to register cfdata\n",
				crypto_cd.cd_name);

			return error;
		}

		error = devsw_attach(crypto_cd.cd_name, NULL, &bmajor,
		    &crypto_cdevsw, &cmajor);
		if (error) {
			error = config_cfdata_detach(crypto_cfdata);
			if (error) {
				return error;
			}
			config_cfattach_detach(crypto_cd.cd_name, &crypto_ca);
			config_cfdriver_detach(&crypto_cd);
			aprint_error("%s: unable to register devsw\n",
				crypto_cd.cd_name);

			return error;
		}

		(void)config_attach_pseudo(crypto_cfdata);
#endif

		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_cfdata_detach(crypto_cfdata);
		if (error) {
			return error;
		}

		config_cfattach_detach(crypto_cd.cd_name, &crypto_ca);
		config_cfdriver_detach(&crypto_cd);
		devsw_detach(NULL, &crypto_cdevsw);
#endif

		return error;
#ifdef _MODULE
	case MODULE_CMD_AUTOUNLOAD:
#if 0	/*
	 * XXX Completely disable auto-unload for now, since there is still
	 * XXX a (small) window where in-module ref-counting doesn't help
	 */
		if (crypto_refcount != 0)
#endif
			return EBUSY;
	/* FALLTHROUGH */
#endif
	default:
		return ENOTTY;
	}
}
