/*	$NetBSD: node.c,v 1.64 2012/01/20 22:07:58 jakllsch Exp $	*/

/*
 * Copyright (c) 2006-2009  Antti Kantee.  All Rights Reserved.
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
#ifndef lint
__RCSID("$NetBSD: node.c,v 1.64 2012/01/20 22:07:58 jakllsch Exp $");
#endif /* !lint */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "psshfs.h"
#include "sftp_proto.h"

int
psshfs_node_lookup(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn)
{
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	struct puffs_node *pn_dir = opc;
	struct psshfs_node *psn, *psn_dir = pn_dir->pn_data;
	struct puffs_node *pn;
	struct psshfs_dir *pd;
	struct vattr va;
	int rv;

	if (PCNISDOTDOT(pcn)) {
		psn = psn_dir->parent->pn_data;
		psn->stat &= ~PSN_RECLAIMED;

		puffs_newinfo_setcookie(pni, psn_dir->parent);
		puffs_newinfo_setvtype(pni, VDIR);
		return 0;
	}

	rv = sftp_readdir(pu, pctx, pn_dir);
	if (rv) {
		if (rv != EPERM)
			return rv;

		/*
		 * Can't read the directory.  We still might be
		 * able to find the node with getattr in -r+x dirs
		 */
		rv = getpathattr(pu, PCNPATH(pcn), &va);
		if (rv)
			return rv;

		/* guess */
		if (va.va_type == VDIR)
			va.va_nlink = 2;
		else
			va.va_nlink = 1;

		pn = allocnode(pu, pn_dir, pcn->pcn_name, &va);
		psn = pn->pn_data;
		psn->attrread = time(NULL);
	} else {
		pd = lookup(psn_dir->dir, psn_dir->dentnext, pcn->pcn_name);
		if (!pd) {
			return ENOENT;
		}

		if (pd->entry)
			pn = pd->entry;
		else
			pd->entry = pn = makenode(pu, pn_dir, pd, &pd->va);

		/*
		 * sure sure we have fresh attributes.  most likely we will
		 * have them cached.  we might not if we go through:
		 * create - reclaim - lookup (this).
		 */
		rv = getnodeattr(pu, pn, PCNPATH(pcn));
		if (rv)
			return rv;

		psn = pn->pn_data;
	}

	psn->stat &= ~PSN_RECLAIMED;

	puffs_newinfo_setcookie(pni, pn);
	puffs_newinfo_setvtype(pni, pn->pn_va.va_type);
	puffs_newinfo_setsize(pni, pn->pn_va.va_size);

	return 0;
}

int
psshfs_node_getattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct vattr *vap, const struct puffs_cred *pcr)
{
	struct puffs_node *pn = opc;
	int rv;

	rv = getnodeattr(pu, pn, NULL);
	if (rv)
		return rv;

	memcpy(vap, &pn->pn_va, sizeof(struct vattr));

	return 0;
}

int
psshfs_node_setattr(struct puffs_usermount *pu, puffs_cookie_t opc,
	const struct vattr *va, const struct puffs_cred *pcr)
{
	struct puffs_cc *pcc = puffs_cc_getcc(pu);
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	uint32_t reqid;
	struct puffs_framebuf *pb;
	struct vattr kludgeva;
	struct puffs_node *pn = opc;
	struct psshfs_node *psn = pn->pn_data;
	int rv;

	/*
	 * If we cached the remote attributes recently enough, and this
	 * setattr operation would change nothing that sftp actually
	 * records, then we can skip the sftp request.  So first check
	 * whether we have the attributes cached, and then compare
	 * every field that we might send to the sftp server.
	 */

	if (!psn->attrread || REFRESHTIMEOUT(pctx, time(NULL)-psn->attrread))
		goto setattr;

#define CHECK(FIELD, TYPE) do {						\
	if ((va->FIELD != (TYPE)PUFFS_VNOVAL) &&			\
	    (va->FIELD != pn->pn_va.FIELD))				\
		goto setattr;						\
} while (0)

#define CHECKID(FIELD, TYPE, DOMANGLE, MINE, MANGLED) do {		\
	if ((va->FIELD != (TYPE)PUFFS_VNOVAL) &&			\
	    (pn->pn_va.FIELD !=						\
		((pctx->DOMANGLE && (va->FIELD == pctx->MINE))		\
		    ? pctx->MANGLED					\
		    : va->FIELD)))					\
		goto setattr;						\
} while (0)

	CHECK(va_size, uint64_t);
	CHECKID(va_uid, uid_t, domangleuid, myuid, mangleuid);
	CHECKID(va_gid, gid_t, domanglegid, mygid, manglegid);
	CHECK(va_mode, mode_t);
	CHECK(va_atime.tv_sec, time_t);
	CHECK(va_mtime.tv_sec, time_t);

	/* Nothing to change.  */
	return 0;

#undef CHECK
#undef CHECKID

 setattr:
	reqid = NEXTREQ(pctx);
	pb = psbuf_makeout();

	psbuf_req_str(pb, SSH_FXP_SETSTAT, reqid, PNPATH(pn));

	memcpy(&kludgeva, va, sizeof(struct vattr));

	/* XXX: kludge due to openssh server implementation */
	if (va->va_atime.tv_sec != PUFFS_VNOVAL
	    && va->va_mtime.tv_sec == PUFFS_VNOVAL) {
		if (pn->pn_va.va_mtime.tv_sec != PUFFS_VNOVAL)
			kludgeva.va_mtime.tv_sec = pn->pn_va.va_mtime.tv_sec;
		else
			kludgeva.va_mtime.tv_sec = va->va_atime.tv_sec;
	}
	if (va->va_mtime.tv_sec != PUFFS_VNOVAL
	    && va->va_atime.tv_sec == PUFFS_VNOVAL) {
		if (pn->pn_va.va_atime.tv_sec != PUFFS_VNOVAL)
			kludgeva.va_atime.tv_sec = pn->pn_va.va_atime.tv_sec;
		else
			kludgeva.va_atime.tv_sec = va->va_mtime.tv_sec;
	}

	psbuf_put_vattr(pb, &kludgeva, pctx);
	GETRESPONSE(pb, pctx->sshfd);

	rv = psbuf_expect_status(pb);
	if (rv == 0) {
		puffs_setvattr(&pn->pn_va, &kludgeva);
		psn->attrread = time(NULL);
	}

 out:
	puffs_framebuf_destroy(pb);
	return rv;
}

int
psshfs_node_create(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *va)
{
	PSSHFSAUTOVAR(pu);
	struct puffs_node *pn = opc;
	struct puffs_node *pn_new;
	char *fhand = NULL;
	uint32_t fhandlen;

	/* Create node on server first */
	psbuf_req_str(pb, SSH_FXP_OPEN, reqid, PCNPATH(pcn));
	psbuf_put_4(pb, SSH_FXF_WRITE | SSH_FXF_CREAT | SSH_FXF_TRUNC);
	psbuf_put_vattr(pb, va, pctx);
	GETRESPONSE(pb, pctx->sshfd);
	rv = psbuf_expect_handle(pb, &fhand, &fhandlen);
	if (rv)
		goto out;

	/*
	 * Do *not* create the local node before getting a response
	 * from the server.  Otherwise we might screw up consistency,
	 * namely that the node can be looked up before create has
	 * returned (mind you, the kernel will unlock the directory
	 * before the create call from userspace returns).
	 */
	pn_new = allocnode(pu, pn, pcn->pcn_name, va);
	if (!pn_new) {
		struct puffs_framebuf *pb2 = psbuf_makeout();
		reqid = NEXTREQ(pctx);
		psbuf_req_str(pb2, SSH_FXP_REMOVE, reqid, PCNPATH(pcn));
		JUSTSEND(pb2, pctx->sshfd);
		rv = ENOMEM;
	}

	if (pn_new)
		puffs_newinfo_setcookie(pni, pn_new);

	reqid = NEXTREQ(pctx);
	psbuf_recycleout(pb);
	psbuf_req_data(pb, SSH_FXP_CLOSE, reqid, fhand, fhandlen);
	JUSTSEND(pb, pctx->sshfd);
	free(fhand);
	return rv;

 out:
	free(fhand);
	PSSHFSRETURN(rv);
}

/*
 * Open a file handle.  This is used for read and write.  We do not
 * wait here for the success or failure of this operation.  This is
 * because otherwise opening and closing file handles would block
 * reading potentially cached information.  Rather, we defer the wait
 * to read/write and therefore allow cached access without a wait.
 *
 * If we have not yet succesfully opened a type of handle, we do wait
 * here.  Also, if a lazy open fails, we revert back to the same
 * state of waiting.
 */
int
psshfs_node_open(struct puffs_usermount *pu, puffs_cookie_t opc, int mode,
	const struct puffs_cred *pcr)
{
	struct puffs_cc *pcc = puffs_cc_getcc(pu);
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	struct puffs_framebuf *pb, *pb2;
	struct vattr va;
	struct puffs_node *pn = opc;
	struct psshfs_node *psn = pn->pn_data;
	uint32_t reqid;
	int didread, didwrite;
	int rv = 0;

	if (pn->pn_va.va_type == VDIR)
		return 0;

	puffs_setback(pcc, PUFFS_SETBACK_INACT_N1);
	puffs_vattr_null(&va);
	didread = didwrite = 0;
	if (mode & FREAD && psn->fhand_r == NULL && psn->lazyopen_r == NULL) {
		pb = psbuf_makeout();

		reqid = NEXTREQ(pctx);
		psbuf_req_str(pb, SSH_FXP_OPEN, reqid, PNPATH(pn));
		psbuf_put_4(pb, SSH_FXF_READ);
		psbuf_put_vattr(pb, &va, pctx);

		if (puffs_framev_enqueue_cb(pu, pctx->sshfd_data, pb,
		    lazyopen_rresp, psn, 0) == -1) {
			rv = errno;
			puffs_framebuf_destroy(pb);
			goto out;
		}

		psn->lazyopen_r = pb;
		didread = 1;
	}
	if (mode & FWRITE && psn->fhand_w == NULL && psn->lazyopen_w == NULL) {
		pb2 = psbuf_makeout();

		reqid = NEXTREQ(pctx);
		psbuf_req_str(pb2, SSH_FXP_OPEN, reqid, PNPATH(pn));
		psbuf_put_4(pb2, SSH_FXF_WRITE);
		psbuf_put_vattr(pb2, &va, pctx);

		if (puffs_framev_enqueue_cb(pu, pctx->sshfd_data, pb2,
		    lazyopen_wresp, psn, 0) == -1) {
			rv = errno;
			puffs_framebuf_destroy(pb2);
			goto out;
		}

		psn->lazyopen_w = pb2;
		didwrite = 1;
	}
	psn->stat &= ~PSN_HANDLECLOSE;

 out:
	/* wait? */
	if (didread && (psn->stat & PSN_DOLAZY_R) == 0) {
		assert(psn->lazyopen_r);

		rv = puffs_framev_framebuf_ccpromote(psn->lazyopen_r, pcc);
		lazyopen_rresp(pu, psn->lazyopen_r, psn, rv);
		if (psn->fhand_r) {
			psn->stat |= PSN_DOLAZY_R;
		} else {
			if (psn->lazyopen_err_r)
				return psn->lazyopen_err_r;
			return EINVAL;
		}
	}

	/* wait? */
	if (didwrite && (psn->stat & PSN_DOLAZY_W) == 0) {
		assert(psn->lazyopen_w);

		rv = puffs_framev_framebuf_ccpromote(psn->lazyopen_w, pcc);
		lazyopen_wresp(pu, psn->lazyopen_w, psn, rv);
		if (psn->fhand_w) {
			psn->stat |= PSN_DOLAZY_W;
		} else {
			if (psn->lazyopen_err_w)
				return psn->lazyopen_err_w;
			return EINVAL;
		}
	}

	return rv;
}

int
psshfs_node_inactive(struct puffs_usermount *pu, puffs_cookie_t opc)
{
	struct puffs_node *pn = opc;

	closehandles(pu, pn->pn_data, HANDLE_READ | HANDLE_WRITE);
	return 0;
}

int
psshfs_node_readdir(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct dirent *dent, off_t *readoff, size_t *reslen,
	const struct puffs_cred *pcr, int *eofflag,
	off_t *cookies, size_t *ncookies)
{
	struct puffs_cc *pcc = puffs_cc_getcc(pu);
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	struct puffs_node *pn = opc;
	struct psshfs_node *psn = pn->pn_data;
	struct psshfs_dir *pd;
	size_t i;
	int rv, set_readdir;

 restart:
	if (psn->stat & PSN_READDIR) {
		struct psshfs_wait pw;

		set_readdir = 0;
		pw.pw_cc = pcc;
		pw.pw_type = PWTYPE_READDIR;
		TAILQ_INSERT_TAIL(&psn->pw, &pw, pw_entries);
		puffs_cc_yield(pcc);
		goto restart;
	} else {
		psn->stat |= PSN_READDIR;
		set_readdir = 1;
	}

	*ncookies = 0;
	rv = sftp_readdir(pu, pctx, pn);
	if (rv) {
		goto out;
	}

	/* find next dirent */
	for (i = *readoff;;i++) {
		if (i >= psn->dentnext)
			goto out;
		pd = &psn->dir[i];
		if (pd->valid)
			break;
	}

	for (;;) {
		*readoff = i;
		if (!puffs_nextdent(&dent, pd->entryname,
		    pd->va.va_fileid, puffs_vtype2dt(pd->va.va_type), reslen)) {
			rv = 0;
			goto out;
		}

		/* find next entry, store possible nfs key */
		do {
			if (++i >= psn->dentnext)
				goto out;
			pd = &psn->dir[i];
		} while (pd->valid == 0);
		PUFFS_STORE_DCOOKIE(cookies, ncookies, (off_t)i);
	}

 out:
	if (rv == 0) {
		if (i >= psn->dentnext)
			*eofflag = 1;

		*readoff = i;
	}

	if (set_readdir) {
		struct psshfs_wait *pw;

		/* all will likely run to completion because of cache */
		TAILQ_FOREACH(pw, &psn->pw, pw_entries) {
			assert(pw->pw_type == PWTYPE_READDIR);
			puffs_cc_schedule(pw->pw_cc);
			TAILQ_REMOVE(&psn->pw, pw, pw_entries);
		}

		psn->stat &= ~PSN_READDIR;
	}

	return rv;
}

int
psshfs_node_read(struct puffs_usermount *pu, puffs_cookie_t opc, uint8_t *buf,
	off_t offset, size_t *resid, const struct puffs_cred *pcr,
	int ioflag)
{
	PSSHFSAUTOVAR(pu);
	struct puffs_node *pn = opc;
	struct psshfs_node *psn = pn->pn_data;
	struct psshfs_wait *pwp;
	uint32_t readlen;

	if (pn->pn_va.va_type == VDIR) {
		rv = EISDIR;
		goto farout;
	}

	/* check that a lazyopen didn't fail */
	if (!psn->fhand_r && !psn->lazyopen_r) {
		rv = psn->lazyopen_err_r;
		goto farout;
	}

	/* if someone is already waiting for the lazyopen, "just" wait */
	if (psn->stat & PSN_LAZYWAIT_R) {
		struct psshfs_wait pw;

		assert(psn->lazyopen_r);

		pw.pw_cc = pcc;
		pw.pw_type = PWTYPE_READ1;
		TAILQ_INSERT_TAIL(&psn->pw, &pw, pw_entries);
		puffs_cc_yield(pcc);
	}

	/* if lazyopening, wait for the result */
	if (psn->lazyopen_r) {
		psn->stat |= PSN_LAZYWAIT_R;
		rv = puffs_framev_framebuf_ccpromote(psn->lazyopen_r, pcc);
		lazyopen_rresp(pu, psn->lazyopen_r, psn, rv);

		/* schedule extra waiters */
		TAILQ_FOREACH(pwp, &psn->pw, pw_entries)
			if (pwp->pw_type == PWTYPE_READ1) {
				puffs_cc_schedule(pwp->pw_cc);
				TAILQ_REMOVE(&psn->pw, pwp, pw_entries);
			}
		psn->stat &= ~PSN_LAZYWAIT_R;

		if ((rv = psn->lazyopen_err_r) != 0)
			goto farout;
	}

	/* if there is still no handle, just refuse to live with this */
	if (!psn->fhand_r) {
		rv = EINVAL;
		goto farout;
	}

again:
	readlen = *resid;
	psbuf_req_data(pb, SSH_FXP_READ, reqid, psn->fhand_r, psn->fhand_r_len);
	psbuf_put_8(pb, offset);
	psbuf_put_4(pb, readlen);

	/*
	 * Do this *after* accessing the file, the handle might not
	 * exist after blocking.
	 */
	if (max_reads && ++psn->readcount > max_reads) {
		struct psshfs_wait pw;

		pw.pw_cc = pcc;
		pw.pw_type = PWTYPE_READ2;
		TAILQ_INSERT_TAIL(&psn->pw, &pw, pw_entries);
		puffs_cc_yield(pcc);
	}

	GETRESPONSE(pb, pctx->sshfd_data);

	rv = psbuf_do_data(pb, buf, &readlen);
	if (rv == 0) {
		*resid -= readlen;
		buf += readlen;
		offset += readlen;
	}

 out:
	if (max_reads && --psn->readcount >= max_reads) {
		TAILQ_FOREACH(pwp, &psn->pw, pw_entries)
			if (pwp->pw_type == PWTYPE_READ2)
				break;
		assert(pwp != NULL);
		puffs_cc_schedule(pwp->pw_cc);
		TAILQ_REMOVE(&psn->pw, pwp, pw_entries);
	}

	if (rv == 0 && *resid > 0) {
		reqid = NEXTREQ(pctx);
		psbuf_recycleout(pb);
		goto again;
	}

 farout:
	/* check if we need a lazyclose */
	if (psn->stat & PSN_HANDLECLOSE && psn->fhand_r) {
		TAILQ_FOREACH(pwp, &psn->pw, pw_entries)
			if (pwp->pw_type == PWTYPE_READ1)
				break;
		if (pwp == NULL)
			closehandles(pu, psn, HANDLE_READ);
	}
	PSSHFSRETURN(rv);
}

/* XXX: we should getattr for size */
int
psshfs_node_write(struct puffs_usermount *pu, puffs_cookie_t opc, uint8_t *buf,
	off_t offset, size_t *resid, const struct puffs_cred *cred,
	int ioflag)
{
	PSSHFSAUTOVAR(pu);
	struct psshfs_wait *pwp;
	struct puffs_node *pn = opc;
	struct psshfs_node *psn = pn->pn_data;
	uint32_t writelen;

	if (pn->pn_va.va_type == VDIR) {
		rv = EISDIR;
		goto out;
	}

	/* check that a lazyopen didn't fail */
	if (!psn->fhand_w && !psn->lazyopen_w) {
		rv = psn->lazyopen_err_w;
		goto out;
	}

	if (psn->stat & PSN_LAZYWAIT_W) {
		struct psshfs_wait pw;

		assert(psn->lazyopen_w);

		pw.pw_cc = pcc;
		pw.pw_type = PWTYPE_WRITE;
		TAILQ_INSERT_TAIL(&psn->pw, &pw, pw_entries);
		puffs_cc_yield(pcc);
	}

	/*
	 * If lazyopening, wait for the result.
	 * There can still be more than oen writer at a time in case
	 * the kernel issues write FAFs.
	 */
	if (psn->lazyopen_w) {
		psn->stat |= PSN_LAZYWAIT_W;
		rv = puffs_framev_framebuf_ccpromote(psn->lazyopen_w, pcc);
		lazyopen_wresp(pu, psn->lazyopen_w, psn, rv);

		/* schedule extra waiters */
		TAILQ_FOREACH(pwp, &psn->pw, pw_entries)
			if (pwp->pw_type == PWTYPE_WRITE) {
				puffs_cc_schedule(pwp->pw_cc);
				TAILQ_REMOVE(&psn->pw, pwp, pw_entries);
			}
		psn->stat &= ~PSN_LAZYWAIT_W;

		if ((rv = psn->lazyopen_err_w) != 0)
			goto out;
	}

	if (!psn->fhand_w) {
		abort();
		rv = EINVAL;
		goto out;
	}

	writelen = *resid;
	psbuf_req_data(pb, SSH_FXP_WRITE, reqid, psn->fhand_w,psn->fhand_w_len);
	psbuf_put_8(pb, offset);
	psbuf_put_data(pb, buf, writelen);
	GETRESPONSE(pb, pctx->sshfd_data);

	rv = psbuf_expect_status(pb);
	if (rv == 0)
		*resid = 0;

	if (pn->pn_va.va_size < (uint64_t)offset + writelen)
		pn->pn_va.va_size = offset + writelen;

 out:
	/* check if we need a lazyclose */
	if (psn->stat & PSN_HANDLECLOSE && psn->fhand_w) {
		TAILQ_FOREACH(pwp, &psn->pw, pw_entries)
			if (pwp->pw_type == PWTYPE_WRITE)
				break;
		if (pwp == NULL)
			closehandles(pu, psn, HANDLE_WRITE);
	}
	PSSHFSRETURN(rv);
}

int
psshfs_node_readlink(struct puffs_usermount *pu, puffs_cookie_t opc,
	const struct puffs_cred *cred, char *linkvalue, size_t *linklen)
{
	PSSHFSAUTOVAR(pu);
	struct puffs_node *pn = opc;
	struct psshfs_node *psn = pn->pn_data;
	uint32_t count;

	if (pctx->protover < 3) {
		rv = EOPNOTSUPP;
		goto out;
	}

	/*
	 * check if we can use a cached version
	 *
	 * XXX: we might end up reading the same link multiple times
	 * from the server if we get many requests at once, but that's
	 * quite harmless as this routine is reentrant.
	 */
	if (psn->symlink && !REFRESHTIMEOUT(pctx, time(NULL) - psn->slread))
		goto copy;

	if (psn->symlink) {
		free(psn->symlink);
		psn->symlink = NULL;
		psn->slread = 0;
	}

	psbuf_req_str(pb, SSH_FXP_READLINK, reqid, PNPATH(pn));
	GETRESPONSE(pb, pctx->sshfd);

	rv = psbuf_expect_name(pb, &count);
	if (rv)
		goto out;
	if (count != 1) {
		rv = EPROTO;
		goto out;
	}

	rv = psbuf_get_str(pb, &psn->symlink, NULL);
	if (rv)
		goto out;
	psn->slread = time(NULL);

 copy:
	*linklen = strlen(psn->symlink);
	(void) memcpy(linkvalue, psn->symlink, *linklen);

 out:
	PSSHFSRETURN(rv);
}

static int
doremove(struct puffs_usermount *pu, struct puffs_node *pn_dir,
	struct puffs_node *pn, const char *name)
{
	PSSHFSAUTOVAR(pu);
	int op;

	if (pn->pn_va.va_type == VDIR)
		op = SSH_FXP_RMDIR;
	else
		op = SSH_FXP_REMOVE;

	psbuf_req_str(pb, op, reqid, PNPATH(pn));
	GETRESPONSE(pb, pctx->sshfd);

	rv = psbuf_expect_status(pb);
	if (rv == 0)
		nukenode(pn, name, 0);

 out:
	PSSHFSRETURN(rv);
}

int
psshfs_node_remove(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t targ, const struct puffs_cn *pcn)
{
	struct puffs_node *pn_targ = targ;
	int rv;

	assert(pn_targ->pn_va.va_type != VDIR);

	rv = doremove(pu, opc, targ, pcn->pcn_name);
	if (rv == 0)
		puffs_setback(puffs_cc_getcc(pu), PUFFS_SETBACK_NOREF_N2);

	return rv;
}

int
psshfs_node_rmdir(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t targ, const struct puffs_cn *pcn)
{
	struct puffs_node *pn_targ = targ;
	int rv;

	assert(pn_targ->pn_va.va_type == VDIR);

	rv = doremove(pu, opc, targ, pcn->pcn_name);
	if (rv == 0)
		puffs_setback(puffs_cc_getcc(pu), PUFFS_SETBACK_NOREF_N2);

	return rv;
}

int
psshfs_node_mkdir(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *va)
{
	PSSHFSAUTOVAR(pu);
	struct puffs_node *pn = opc;
	struct puffs_node *pn_new;

	psbuf_req_str(pb, SSH_FXP_MKDIR, reqid, PCNPATH(pcn));
	psbuf_put_vattr(pb, va, pctx);
	GETRESPONSE(pb, pctx->sshfd);

	rv = psbuf_expect_status(pb);
	if (rv)
		goto out;

	pn_new = allocnode(pu, pn, pcn->pcn_name, va);
	if (pn_new) {
		puffs_newinfo_setcookie(pni, pn_new);
	} else {
		struct puffs_framebuf *pb2 = psbuf_makeout();
		reqid = NEXTREQ(pctx);
		psbuf_recycleout(pb2);
		psbuf_req_str(pb2, SSH_FXP_RMDIR, reqid, PCNPATH(pcn));
		JUSTSEND(pb2, pctx->sshfd);
		rv = ENOMEM;
	}

 out:
	PSSHFSRETURN(rv);
}

int
psshfs_node_symlink(struct puffs_usermount *pu, puffs_cookie_t opc,
	struct puffs_newinfo *pni, const struct puffs_cn *pcn,
	const struct vattr *va, const char *link_target)
{
	PSSHFSAUTOVAR(pu);
	struct puffs_node *pn = opc;
	struct puffs_node *pn_new;

	if (pctx->protover < 3) {
		rv = EOPNOTSUPP;
		goto out;
	}

	/*
	 * XXX: ietf says: source, target.  openssh says: ietf who?
	 * Let's go with openssh and build quirk tables later if we care
	 */
	psbuf_req_str(pb, SSH_FXP_SYMLINK, reqid, link_target);
	psbuf_put_str(pb, PCNPATH(pcn));
	GETRESPONSE(pb, pctx->sshfd);

	rv = psbuf_expect_status(pb);
	if (rv)
		goto out;

	pn_new = allocnode(pu, pn, pcn->pcn_name, va);
	if (pn_new) {
		puffs_newinfo_setcookie(pni, pn_new);
	} else {
		struct puffs_framebuf *pb2 = psbuf_makeout();
		reqid = NEXTREQ(pctx);
		psbuf_recycleout(pb2);
		psbuf_req_str(pb2, SSH_FXP_REMOVE, reqid, PCNPATH(pcn));
		JUSTSEND(pb2, pctx->sshfd);
		rv = ENOMEM;
	}

 out:
	PSSHFSRETURN(rv);
}

int
psshfs_node_rename(struct puffs_usermount *pu, puffs_cookie_t opc,
	puffs_cookie_t src, const struct puffs_cn *pcn_src,
	puffs_cookie_t targ_dir, puffs_cookie_t targ,
	const struct puffs_cn *pcn_targ)
{
	PSSHFSAUTOVAR(pu);
	struct puffs_node *pn_sf = src;
	struct puffs_node *pn_td = targ_dir, *pn_tf = targ;
	struct psshfs_node *psn_src = pn_sf->pn_data;
	struct psshfs_node *psn_targdir = pn_td->pn_data;

	if (pctx->protover < 2) {
		rv = EOPNOTSUPP;
		goto out;
	}

	if (pn_tf) {
		rv = doremove(pu, targ_dir, pn_tf, pcn_targ->pcn_name);
		if (rv)
			goto out;
	}

	psbuf_req_str(pb, SSH_FXP_RENAME, reqid, PCNPATH(pcn_src));
	psbuf_put_str(pb, PCNPATH(pcn_targ));
	GETRESPONSE(pb, pctx->sshfd);

	rv = psbuf_expect_status(pb);
	if (rv == 0) {
		struct psshfs_dir *pd;

		/*
		 * XXX: interfaces didn't quite work with rename..
		 * the song remains the same.  go figure .. ;)
		 */
		nukenode(pn_sf, pcn_src->pcn_name, 0);
		pd = direnter(pn_td, pcn_targ->pcn_name);
		pd->entry = pn_sf;
		puffs_setvattr(&pd->va, &pn_sf->pn_va);

		if (opc != targ_dir) {
			psn_targdir->childcount++;
			psn_src->parent = pn_td;
			if (pn_sf->pn_va.va_type == VDIR)
				pn_td->pn_va.va_nlink++;
		}
	}

 out:
	PSSHFSRETURN(rv);
}

/*
 * So this file system happened to be written in such a way that
 * lookup for ".." is hard if we lose the in-memory node.  We'd
 * need to recreate the entire directory structure from the root
 * node up to the ".." node we're looking up.
 *
 * And since our entire fs structure is purely fictional (i.e. it's
 * only in-memory, not fetchable from the server), the easiest way
 * to deal with it is to not allow nodes with children to be
 * reclaimed.
 *
 * If a node with children is being attempted to be reclaimed, we
 * just mark it "reclaimed" but leave it as is until all its children
 * have been reclaimed.  If a lookup for that node is done meanwhile,
 * it will be found by lookup() and we just remove the "reclaimed"
 * bit.
 */
int
psshfs_node_reclaim(struct puffs_usermount *pu, puffs_cookie_t opc)
{
	struct puffs_node *pn = opc, *pn_next, *pn_root;
	struct psshfs_node *psn = pn->pn_data;

	/*
	 * don't reclaim if we have file handle issued, otherwise
	 * we can't do fhtonode
	 */
	if (psn->stat & PSN_HASFH)
		return 0;

	psn->stat |= PSN_RECLAIMED;
	pn_root = puffs_getroot(pu);
	for (; pn != pn_root; pn = pn_next) {
		psn = pn->pn_data;
		if ((psn->stat & PSN_RECLAIMED) == 0 || psn->childcount != 0)
			break;

		pn_next = psn->parent;
		doreclaim(pn);
	}

	return 0;
}
