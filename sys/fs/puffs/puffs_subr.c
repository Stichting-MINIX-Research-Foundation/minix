/*	$NetBSD: puffs_subr.c,v 1.67 2014/11/10 18:46:33 maxv Exp $	*/

/*
 * Copyright (c) 2006, 2007  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Ulla Tuominen Foundation and the Finnish Cultural Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: puffs_subr.c,v 1.67 2014/11/10 18:46:33 maxv Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/poll.h>
#include <sys/proc.h>

#include <fs/puffs/puffs_msgif.h>
#include <fs/puffs/puffs_sys.h>

#ifdef PUFFSDEBUG
int puffsdebug;
#endif

void
puffs_makecn(struct puffs_kcn *pkcn, struct puffs_kcred *pkcr,
	const struct componentname *cn, int full)
{

	pkcn->pkcn_nameiop = cn->cn_nameiop;
	pkcn->pkcn_flags = cn->cn_flags;

	if (full) {
		(void)strcpy(pkcn->pkcn_name, cn->cn_nameptr);
	} else {
		(void)memcpy(pkcn->pkcn_name, cn->cn_nameptr, cn->cn_namelen);
		pkcn->pkcn_name[cn->cn_namelen] = '\0';
	}
	pkcn->pkcn_namelen = cn->cn_namelen;
	pkcn->pkcn_consume = 0;

	puffs_credcvt(pkcr, cn->cn_cred);
}

/*
 * Convert given credentials to struct puffs_kcred for userspace.
 */
void
puffs_credcvt(struct puffs_kcred *pkcr, const kauth_cred_t cred)
{

	memset(pkcr, 0, sizeof(struct puffs_kcred));

	if (cred == NOCRED || cred == FSCRED) {
		pkcr->pkcr_type = PUFFCRED_TYPE_INTERNAL;
		if (cred == NOCRED)
			pkcr->pkcr_internal = PUFFCRED_CRED_NOCRED;
		if (cred == FSCRED)
			pkcr->pkcr_internal = PUFFCRED_CRED_FSCRED;
 	} else {
		pkcr->pkcr_type = PUFFCRED_TYPE_UUC;
		kauth_cred_to_uucred(&pkcr->pkcr_uuc, cred);
	}
}

void
puffs_parkdone_asyncbioread(struct puffs_mount *pmp,
	struct puffs_req *preq, void *arg)
{
	struct puffs_vnmsg_read *read_msg = (void *)preq;
	struct buf *bp = arg;
	size_t moved;

	DPRINTF(("%s\n", __func__));

	bp->b_error = checkerr(pmp, preq->preq_rv, __func__);
	if (bp->b_error == 0) {
		if (read_msg->pvnr_resid > bp->b_bcount) {
			puffs_senderr(pmp, PUFFS_ERR_READ, E2BIG,
			    "resid grew", preq->preq_cookie);
			bp->b_error = E2BIG;
		} else {
			moved = bp->b_bcount - read_msg->pvnr_resid;
			bp->b_resid = read_msg->pvnr_resid;

			memcpy(bp->b_data, read_msg->pvnr_data, moved);
		}
	}

	biodone(bp);
}

void
puffs_parkdone_asyncbiowrite(struct puffs_mount *pmp,
	struct puffs_req *preq, void *arg)
{
	struct puffs_vnmsg_write *write_msg = (void *)preq;
	struct buf *bp = arg;

	DPRINTF(("%s\n", __func__));

	bp->b_error = checkerr(pmp, preq->preq_rv, __func__);
	if (bp->b_error == 0) {
		if (write_msg->pvnr_resid > bp->b_bcount) {
			puffs_senderr(pmp, PUFFS_ERR_WRITE, E2BIG,
			    "resid grew", preq->preq_cookie);
			bp->b_error = E2BIG;
		} else {
			bp->b_resid = write_msg->pvnr_resid;
		}
	}

	biodone(bp);
}

/* XXX: userspace can leak kernel resources */
void
puffs_parkdone_poll(struct puffs_mount *pmp, struct puffs_req *preq, void *arg)
{
	struct puffs_vnmsg_poll *poll_msg = (void *)preq;
	struct puffs_node *pn = arg;
	int revents, error;

	error = checkerr(pmp, preq->preq_rv, __func__);
	if (error)
		revents = poll_msg->pvnr_events;
	else
		revents = POLLERR;

	mutex_enter(&pn->pn_mtx);
	pn->pn_revents |= revents;
	mutex_exit(&pn->pn_mtx);

	selnotify(&pn->pn_sel, revents, 0);

	puffs_releasenode(pn);
}

void
puffs_mp_reference(struct puffs_mount *pmp)
{

	KASSERT(mutex_owned(&pmp->pmp_lock));
	pmp->pmp_refcount++;
}

void
puffs_mp_release(struct puffs_mount *pmp)
{

	KASSERT(mutex_owned(&pmp->pmp_lock));
	if (--pmp->pmp_refcount == 0)
		cv_broadcast(&pmp->pmp_refcount_cv);
}

void
puffs_gop_size(struct vnode *vp, off_t size, off_t *eobp,
	int flags)
{

	*eobp = size;
}

void
puffs_gop_markupdate(struct vnode *vp, int flags)
{
	int uflags = 0;

	if (flags & GOP_UPDATE_ACCESSED)
		uflags |= PUFFS_UPDATEATIME;
	if (flags & GOP_UPDATE_MODIFIED)
		uflags |= PUFFS_UPDATEMTIME;

	puffs_updatenode(VPTOPP(vp), uflags, 0);
}

void
puffs_senderr(struct puffs_mount *pmp, int type, int error,
	const char *str, puffs_cookie_t ck)
{
	struct puffs_msgpark *park;
	struct puffs_error *perr;

	puffs_msgmem_alloc(sizeof(struct puffs_error), &park, (void *)&perr, 1);
	puffs_msg_setfaf(park);
	puffs_msg_setinfo(park, PUFFSOP_ERROR, type, ck);

	perr->perr_error = error;
	strlcpy(perr->perr_str, str, sizeof(perr->perr_str));

	puffs_msg_enqueue(pmp, park);
	puffs_msgmem_release(park);
}
