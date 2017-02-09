/*	$NetBSD: ocryptodev.c,v 1.6 2014/09/05 09:23:40 matt Exp $ */
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

/*
 * Implement backward compatibility IOCTLs in this module.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ocryptodev.c,v 1.6 2014/09/05 09:23:40 matt Exp $");

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

#ifdef _KERNEL_OPT
#include "opt_ocf.h"
#endif

#include <opencrypto/cryptodev.h>
#include <opencrypto/cryptodev_internal.h>
#include <opencrypto/ocryptodev.h>
#include <opencrypto/xform.h>

static int	ocryptodev_op(struct csession *, struct ocrypt_op *,
		    struct lwp *);
static int	ocryptodev_mop(struct fcrypt *, struct ocrypt_n_op *, int,
		    struct lwp *);
static int	ocryptodev_session(struct fcrypt *, struct osession_op *);
static int	ocryptodev_msession(struct fcrypt *, struct osession_n_op *, int);

int
ocryptof_ioctl(struct file *fp, u_long cmd, void *data)
{
	struct fcrypt *fcr = fp->f_fcrypt;
	struct csession *cse;
	struct osession_op *osop;
	struct osession_n_op *osnop;
	struct ocrypt_op *ocop;
	struct ocrypt_mop *omop;
	struct ocrypt_n_op *ocnop;
	struct ocrypt_sgop *osgop;

	int error = 0;

	switch (cmd) {
	case OCIOCGSESSION:
		osop = (struct osession_op *)data;
		error = ocryptodev_session(fcr, osop);
		break;
	case CIOCNGSESSION:
		osgop = (struct ocrypt_sgop *)data;
		osnop = kmem_alloc((osgop->count *
				  sizeof(struct osession_n_op)), KM_SLEEP);
		error = copyin(osgop->sessions, osnop, osgop->count *
			       sizeof(struct osession_n_op));
		if (error) {
			goto mbail;
		}

		error = ocryptodev_msession(fcr, osnop, osgop->count);
		if (error) {
			goto mbail;
		}

		error = copyout(osnop, osgop->sessions, osgop->count *
		    sizeof(struct osession_n_op));
mbail:
		kmem_free(osnop, osgop->count * sizeof(struct osession_n_op));
		break;
	case OCIOCCRYPT:
		mutex_enter(&crypto_mtx);
		ocop = (struct ocrypt_op *)data;
		cse = cryptodev_csefind(fcr, ocop->ses);
		mutex_exit(&crypto_mtx);
		if (cse == NULL) {
			DPRINTF(("csefind failed\n"));
			return EINVAL;
		}
		error = ocryptodev_op(cse, ocop, curlwp);
		DPRINTF(("ocryptodev_op error = %d\n", error));
		break;
	case OCIOCNCRYPTM:
		omop = (struct ocrypt_mop *)data;
		ocnop = kmem_alloc((omop->count * sizeof(struct ocrypt_n_op)),
		    KM_SLEEP);
		error = copyin(omop->reqs, ocnop,
		    (omop->count * sizeof(struct ocrypt_n_op)));
		if(!error) {
			error = ocryptodev_mop(fcr, ocnop, omop->count, curlwp);
			if (!error) {
				error = copyout(ocnop, omop->reqs, 
				    (omop->count * sizeof(struct ocrypt_n_op)));
			}
		}
		kmem_free(ocnop, (omop->count * sizeof(struct ocrypt_n_op)));
		break;	
	default:
		DPRINTF(("invalid ioctl cmd 0x%lx\n", cmd));
		return EINVAL;
	}
	return error;
}


static int
ocryptodev_op(struct csession *cse, struct ocrypt_op *ocop, struct lwp *l)
{
	struct crypt_op cop;

	cop.ses = ocop->ses;
	cop.op = ocop->op;
	cop.flags = ocop->flags;
	cop.len = ocop->len;
	cop.src = ocop->src;
	cop.dst = ocop->dst;
	cop.mac = ocop->mac;
	cop.iv = ocop->iv;
	cop.dst_len = 0;

	return cryptodev_op(cse, &cop, l);
};

static int 
ocryptodev_mop(struct fcrypt *fcr, 
              struct ocrypt_n_op *ocnop,
              int count, struct lwp *l)
{
	int res;

	struct crypt_n_op cnop;

	cnop.ses = ocnop->ses;
	cnop.op = ocnop->op;
	cnop.flags = ocnop->flags;
	cnop.len = ocnop->len;
	cnop.reqid = ocnop->reqid;
	cnop.status = ocnop->status;
	cnop.opaque = ocnop->opaque;
	cnop.keylen = ocnop->keylen;
	cnop.key = ocnop->key;
	cnop.mackeylen = ocnop->mackeylen;
	cnop.mackey = ocnop->mackey;
	cnop.src = ocnop->src;
	cnop.dst = ocnop->dst;
	cnop.mac = ocnop->mac;
	cnop.iv = ocnop->iv;
	cnop.dst_len = 0;
	res = cryptodev_mop(fcr, &cnop, count, l);
	ocnop->reqid = cnop.reqid;
	ocnop->status = cnop.status;

	return res;
};


static int
ocryptodev_session(struct fcrypt *fcr, struct osession_op *osop) 
{
	struct session_op sop;
	int res;

	sop.cipher = osop->cipher;
	sop.mac = osop->mac;
	sop.comp_alg = 0;
	sop.keylen = osop->keylen;
	sop.key = osop->key;
	sop.mackeylen = osop->mackeylen;
	sop.mackey = osop->mackey;
	res = cryptodev_session(fcr, &sop);
	osop->ses = sop.ses;
	return res;

}

static int
ocryptodev_msession(struct fcrypt *fcr, struct osession_n_op *osn_ops,
		   int count)
{
	int i;

	for (i = 0; i < count; i++, osn_ops++) {
		struct osession_op os_op;
		os_op.cipher =		osn_ops->cipher;
		os_op.mac =		osn_ops->mac;
		os_op.keylen =		osn_ops->keylen;
		os_op.key =		osn_ops->key;
		os_op.mackeylen =	osn_ops->mackeylen;
		os_op.mackey =		osn_ops->mackey;

		osn_ops->status = ocryptodev_session(fcr, &os_op);
		osn_ops->ses =		os_op.ses;
	}

	return 0;
}
