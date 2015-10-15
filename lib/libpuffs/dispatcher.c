/*	$NetBSD: dispatcher.c,v 1.48 2014/10/31 13:56:04 manu Exp $	*/

/*
 * Copyright (c) 2006, 2007, 2008 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Ulla Tuominen Foundation, the Finnish Cultural Foundation and
 * Research Foundation of Helsinki University of Technology.
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
#if !defined(lint)
__RCSID("$NetBSD: dispatcher.c,v 1.48 2014/10/31 13:56:04 manu Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/poll.h>

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <puffs.h>
#include <puffsdump.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "puffs_priv.h"

#define PUFFS_USE_FS_TTL(pu) (pu->pu_flags & PUFFS_KFLAG_CACHE_FS_TTL) 

static void dispatch(struct puffs_cc *);

/* for our eyes only */
void
puffs__ml_dispatch(struct puffs_usermount *pu, struct puffs_framebuf *pb)
{
	struct puffs_cc *pcc = puffs_cc_getcc(pu);
	struct puffs_req *preq;

	pcc->pcc_pb = pb;
	pcc->pcc_flags |= PCC_MLCONT;
	dispatch(pcc);

	/* Put result to kernel sendqueue if necessary */
	preq = puffs__framebuf_getdataptr(pcc->pcc_pb);
	if (PUFFSOP_WANTREPLY(preq->preq_opclass)) {
		if (pu->pu_flags & PUFFS_FLAG_OPDUMP)
			puffsdump_rv(preq);

		puffs_framev_enqueue_justsend(pu, pu->pu_fd,
		    pcc->pcc_pb, 0, 0);
	} else {
		puffs_framebuf_destroy(pcc->pcc_pb);
	}

	/* who needs information when you're living on borrowed time? */
	if (pcc->pcc_flags & PCC_BORROWED) {
		puffs_cc_yield(pcc); /* back to borrow source */
	}
	pcc->pcc_flags = 0;
}

/* public, but not really tested and only semi-supported */
int
puffs_dispatch_create(struct puffs_usermount *pu, struct puffs_framebuf *pb,
	struct puffs_cc **pccp)
{
	struct puffs_cc *pcc;

	if (puffs__cc_create(pu, dispatch, &pcc) == -1)
		return -1;

	pcc->pcc_pb = pb;
	*pccp = pcc;

	return 0;
}

int
puffs_dispatch_exec(struct puffs_cc *pcc, struct puffs_framebuf **pbp)
{
	int rv;

	puffs_cc_continue(pcc);

	if (pcc->pcc_flags & PCC_DONE) {
		rv = 1;
		*pbp = pcc->pcc_pb;
		pcc->pcc_flags = 0;
		puffs__cc_destroy(pcc, 0);
	} else {
		rv = 0;
	}

	return rv;
}

static void
dispatch(struct puffs_cc *pcc)
{
	struct puffs_usermount *pu = pcc->pcc_pu;
	struct puffs_ops *pops = &pu->pu_ops;
	struct puffs_req *preq = puffs__framebuf_getdataptr(pcc->pcc_pb);
	void *auxbuf; /* help with typecasting */
	puffs_cookie_t opcookie;
	int error = 0, buildpath, pncookie;

	/* XXX: smaller hammer, please */
	if ((PUFFSOP_OPCLASS(preq->preq_opclass == PUFFSOP_VFS &&
	    preq->preq_optype == PUFFS_VFS_VPTOFH)) ||
	    (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VN &&
	    (preq->preq_optype == PUFFS_VN_READDIR
	    || preq->preq_optype == PUFFS_VN_READ))) {
		if (puffs_framebuf_reserve_space(pcc->pcc_pb,
		    PUFFS_MSG_MAXSIZE) == -1)
			error = errno;
		preq = puffs__framebuf_getdataptr(pcc->pcc_pb);
	}

	auxbuf = preq;
	opcookie = preq->preq_cookie;

	assert((pcc->pcc_flags & PCC_DONE) == 0);

	buildpath = pu->pu_flags & PUFFS_FLAG_BUILDPATH;
	pncookie = pu->pu_flags & PUFFS_FLAG_PNCOOKIE;
	assert(!buildpath || pncookie);

	preq->preq_setbacks = 0;

	if (pu->pu_flags & PUFFS_FLAG_OPDUMP)
		puffsdump_req(preq);

	puffs__cc_setcaller(pcc, preq->preq_pid, preq->preq_lid);

	/* pre-operation */
	if (pu->pu_oppre)
		pu->pu_oppre(pu);

	if (error)
		goto out;

	/* Execute actual operation */
	if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VFS) {
		switch (preq->preq_optype) {
		case PUFFS_VFS_UNMOUNT:
		{
			struct puffs_vfsmsg_unmount *auxt = auxbuf;

			PU_SETSTATE(pu, PUFFS_STATE_UNMOUNTING);
			error = pops->puffs_fs_unmount(pu, auxt->pvfsr_flags);
			if (!error)
				PU_SETSTATE(pu, PUFFS_STATE_UNMOUNTED);
			else
				PU_SETSTATE(pu, PUFFS_STATE_RUNNING);
			break;
		}

		case PUFFS_VFS_STATVFS:
		{
			struct puffs_vfsmsg_statvfs *auxt = auxbuf;

			error = pops->puffs_fs_statvfs(pu, &auxt->pvfsr_sb);
			break;
		}

		case PUFFS_VFS_SYNC:
		{
			struct puffs_vfsmsg_sync *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvfsr_cred);

			error = pops->puffs_fs_sync(pu,
			    auxt->pvfsr_waitfor, pcr);
			break;
		}

		case PUFFS_VFS_FHTOVP:
		{
			struct puffs_vfsmsg_fhtonode *auxt = auxbuf;
			struct puffs_newinfo pni;

			pni.pni_cookie = &auxt->pvfsr_fhcookie;
			pni.pni_vtype = &auxt->pvfsr_vtype;
			pni.pni_size = &auxt->pvfsr_size;
			pni.pni_rdev = &auxt->pvfsr_rdev;
			pni.pni_va = NULL;
			pni.pni_va_ttl = NULL;
			pni.pni_cn_ttl = NULL;

			error = pops->puffs_fs_fhtonode(pu, auxt->pvfsr_data,
			    auxt->pvfsr_dsize, &pni);

			break;
		}

		case PUFFS_VFS_VPTOFH:
		{
			struct puffs_vfsmsg_nodetofh *auxt = auxbuf;

			error = pops->puffs_fs_nodetofh(pu,
			    auxt->pvfsr_fhcookie, auxt->pvfsr_data,
			    &auxt->pvfsr_dsize);

			break;
		}

		case PUFFS_VFS_EXTATTRCTL:
		{
			struct puffs_vfsmsg_extattrctl *auxt = auxbuf;
			const char *attrname;
			int flags;

			if (pops->puffs_fs_extattrctl == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			if (auxt->pvfsr_flags & PUFFS_EXTATTRCTL_HASATTRNAME)
				attrname = auxt->pvfsr_attrname;
			else
				attrname = NULL;

			flags = auxt->pvfsr_flags & PUFFS_EXTATTRCTL_HASNODE;
			error = pops->puffs_fs_extattrctl(pu, auxt->pvfsr_cmd,
			    opcookie, flags,
			    auxt->pvfsr_attrnamespace, attrname);
			break;
		}

		default:
			/*
			 * I guess the kernel sees this one coming
			 */
			error = EINVAL;
			break;
		}

	/* XXX: audit return values */
	/* XXX: sync with kernel */
	} else if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VN) {
		switch (preq->preq_optype) {
		case PUFFS_VN_LOOKUP:
		{
			struct puffs_vnmsg_lookup *auxt = auxbuf;
			struct puffs_newinfo pni;
			struct puffs_cn pcn;
			struct puffs_node *pn = NULL;

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			PUFFS_KCREDTOCRED(pcn.pcn_cred, &auxt->pvnr_cn_cred);
			pni.pni_cookie = &auxt->pvnr_newnode;
			pni.pni_vtype = &auxt->pvnr_vtype;
			pni.pni_size = &auxt->pvnr_size;
			pni.pni_rdev = &auxt->pvnr_rdev;
			pni.pni_va = &auxt->pvnr_va;
			pni.pni_va_ttl = &auxt->pvnr_va_ttl;
			pni.pni_cn_ttl = &auxt->pvnr_cn_ttl;

			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn, opcookie);
				if (error)
					break;
			}

			/* lookup *must* be present */
			error = pops->puffs_node_lookup(pu, opcookie,
			    &pni, &pcn);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					/*
					 * did we get a new node or a
					 * recycled node?
					 */
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					if (pn->pn_po.po_path == NULL)
						pn->pn_po = pcn.pcn_po_full;
					else
						pu->pu_pathfree(pu,
						    &pcn.pcn_po_full);
				}
			}

			if (pncookie && !error) {
				if (pn == NULL)
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
				pn->pn_nlookup++;
			}
			break;
		}

		case PUFFS_VN_CREATE:
		{
			struct puffs_vnmsg_create *auxt = auxbuf;
			struct puffs_newinfo pni;
			struct puffs_cn pcn;
			struct puffs_node *pn = NULL;

			if (pops->puffs_node_create == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			PUFFS_KCREDTOCRED(pcn.pcn_cred, &auxt->pvnr_cn_cred);

			memset(&pni, 0, sizeof(pni));
			pni.pni_cookie = &auxt->pvnr_newnode;
			pni.pni_va = &auxt->pvnr_va;
			pni.pni_va_ttl = &auxt->pvnr_va_ttl;
			pni.pni_cn_ttl = &auxt->pvnr_cn_ttl;

			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn, opcookie);
				if (error)
					break;
			}

			error = pops->puffs_node_create(pu,
			    opcookie, &pni, &pcn, &auxt->pvnr_va);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					pn->pn_po = pcn.pcn_po_full;
				}
			}

			if (pncookie && !error) {
				if (pn == NULL)
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
				pn->pn_nlookup++;
			}
			break;
		}

		case PUFFS_VN_MKNOD:
		{
			struct puffs_vnmsg_mknod *auxt = auxbuf;
			struct puffs_newinfo pni;
			struct puffs_cn pcn;
			struct puffs_node *pn = NULL;

			if (pops->puffs_node_mknod == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			PUFFS_KCREDTOCRED(pcn.pcn_cred, &auxt->pvnr_cn_cred);

			memset(&pni, 0, sizeof(pni));
			pni.pni_cookie = &auxt->pvnr_newnode;
			pni.pni_va = &auxt->pvnr_va;
			pni.pni_va_ttl = &auxt->pvnr_va_ttl;
			pni.pni_cn_ttl = &auxt->pvnr_cn_ttl;

			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn, opcookie);
				if (error)
					break;
			}

			error = pops->puffs_node_mknod(pu,
			    opcookie, &pni, &pcn, &auxt->pvnr_va);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					pn->pn_po = pcn.pcn_po_full;
				}
			}

			if (pncookie && !error) {
				if (pn == NULL)
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
				pn->pn_nlookup++;
			}
			break;
		}

		case PUFFS_VN_OPEN:
		{
			struct puffs_vnmsg_open *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (pops->puffs_node_open2 != NULL) {
				error = pops->puffs_node_open2(pu,
				    opcookie, auxt->pvnr_mode, pcr, 
				    &auxt->pvnr_oflags);

				break;
			}

			if (pops->puffs_node_open == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_open(pu,
			    opcookie, auxt->pvnr_mode, pcr);
			break;
		}

		case PUFFS_VN_CLOSE:
		{
			struct puffs_vnmsg_close *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (pops->puffs_node_close == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_close(pu,
			    opcookie, auxt->pvnr_fflag, pcr);
			break;
		}

		case PUFFS_VN_ACCESS:
		{
			struct puffs_vnmsg_access *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (pops->puffs_node_access == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_access(pu,
			    opcookie, auxt->pvnr_mode, pcr);
			break;
		}

		case PUFFS_VN_GETATTR:
		{
			struct puffs_vnmsg_getattr *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (PUFFS_USE_FS_TTL(pu)) {
				if (pops->puffs_node_getattr_ttl == NULL) {
					error = EOPNOTSUPP;
					break;
				}

				error = pops->puffs_node_getattr_ttl(pu,
				    opcookie, &auxt->pvnr_va, pcr,
				    &auxt->pvnr_va_ttl);
			} else {
				if (pops->puffs_node_getattr == NULL) {
					error = EOPNOTSUPP;
					break;
				}

				error = pops->puffs_node_getattr(pu,
				    opcookie, &auxt->pvnr_va, pcr);
			}
			break;
		}

		case PUFFS_VN_SETATTR:
		{
			struct puffs_vnmsg_setattr *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (PUFFS_USE_FS_TTL(pu)) {
				int xflag = 0;

				if (pops->puffs_node_setattr_ttl == NULL) {
					error = EOPNOTSUPP;
					break;
				}

				if (!PUFFSOP_WANTREPLY(preq->preq_opclass))
					xflag |= PUFFS_SETATTR_FAF;

				error = pops->puffs_node_setattr_ttl(pu,
				    opcookie, &auxt->pvnr_va, pcr,
				    &auxt->pvnr_va_ttl, xflag);
			} else {
				if (pops->puffs_node_setattr == NULL) {
					error = EOPNOTSUPP;
					break;
				}

				error = pops->puffs_node_setattr(pu,
				    opcookie, &auxt->pvnr_va, pcr);
			}
			break;
		}

		case PUFFS_VN_MMAP:
		{
			struct puffs_vnmsg_mmap *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (pops->puffs_node_mmap == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_mmap(pu,
			    opcookie, auxt->pvnr_prot, pcr);
			break;
		}

		case PUFFS_VN_FSYNC:
		{
			struct puffs_vnmsg_fsync *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (pops->puffs_node_fsync == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_fsync(pu, opcookie, pcr,
			    auxt->pvnr_flags, auxt->pvnr_offlo,
			    auxt->pvnr_offhi);
			break;
		}

		case PUFFS_VN_SEEK:
		{
			struct puffs_vnmsg_seek *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (pops->puffs_node_seek == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_seek(pu,
			    opcookie, auxt->pvnr_oldoff,
			    auxt->pvnr_newoff, pcr);
			break;
		}

		case PUFFS_VN_REMOVE:
		{
			struct puffs_vnmsg_remove *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_remove == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			PUFFS_KCREDTOCRED(pcn.pcn_cred, &auxt->pvnr_cn_cred);

			error = pops->puffs_node_remove(pu,
			    opcookie, auxt->pvnr_cookie_targ, &pcn);
			break;
		}

		case PUFFS_VN_LINK:
		{
			struct puffs_vnmsg_link *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_link == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			PUFFS_KCREDTOCRED(pcn.pcn_cred, &auxt->pvnr_cn_cred);

			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn, opcookie);
				if (error)
					break;
			}

			error = pops->puffs_node_link(pu,
			    opcookie, auxt->pvnr_cookie_targ, &pcn);
			if (buildpath)
				pu->pu_pathfree(pu, &pcn.pcn_po_full);

			break;
		}

		case PUFFS_VN_RENAME:
		{
			struct puffs_vnmsg_rename *auxt = auxbuf;
			struct puffs_cn pcn_src, pcn_targ;
			struct puffs_node *pn_src;

			if (pops->puffs_node_rename == NULL) {
				error = 0;
				break;
			}

			pcn_src.pcn_pkcnp = &auxt->pvnr_cn_src;
			PUFFS_KCREDTOCRED(pcn_src.pcn_cred,
			    &auxt->pvnr_cn_src_cred);

			pcn_targ.pcn_pkcnp = &auxt->pvnr_cn_targ;
			PUFFS_KCREDTOCRED(pcn_targ.pcn_cred,
			    &auxt->pvnr_cn_targ_cred);

			if (buildpath) {
				pn_src = auxt->pvnr_cookie_src;
				pcn_src.pcn_po_full = pn_src->pn_po;

				error = puffs_path_pcnbuild(pu, &pcn_targ,
				    auxt->pvnr_cookie_targdir);
				if (error)
					break;
			}

			error = pops->puffs_node_rename(pu,
			    opcookie, auxt->pvnr_cookie_src,
			    &pcn_src, auxt->pvnr_cookie_targdir,
			    auxt->pvnr_cookie_targ, &pcn_targ);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu,
					    &pcn_targ.pcn_po_full);
				} else {
					struct puffs_pathinfo pi;
					struct puffs_pathobj po_old;

					/* handle this node */
					po_old = pn_src->pn_po;
					pn_src->pn_po = pcn_targ.pcn_po_full;

					if (pn_src->pn_va.va_type != VDIR) {
						pu->pu_pathfree(pu, &po_old);
						break;
					}

					/* handle all child nodes for DIRs */
					pi.pi_old = &pcn_src.pcn_po_full;
					pi.pi_new = &pcn_targ.pcn_po_full;

					PU_LOCK();
					if (puffs_pn_nodewalk(pu,
					    puffs_path_prefixadj, &pi) != NULL)
						error = ENOMEM;
					PU_UNLOCK();
					pu->pu_pathfree(pu, &po_old);
				}
			}
			break;
		}

		case PUFFS_VN_MKDIR:
		{
			struct puffs_vnmsg_mkdir *auxt = auxbuf;
			struct puffs_newinfo pni;
			struct puffs_cn pcn;
			struct puffs_node *pn = NULL;

			if (pops->puffs_node_mkdir == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			PUFFS_KCREDTOCRED(pcn.pcn_cred, &auxt->pvnr_cn_cred);

			memset(&pni, 0, sizeof(pni));
			pni.pni_cookie = &auxt->pvnr_newnode;
			pni.pni_va = &auxt->pvnr_va;
			pni.pni_va_ttl = &auxt->pvnr_va_ttl;
			pni.pni_cn_ttl = &auxt->pvnr_cn_ttl;

			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn, opcookie);
				if (error)
					break;
			}

			error = pops->puffs_node_mkdir(pu,
			    opcookie, &pni, &pcn, &auxt->pvnr_va);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					pn->pn_po = pcn.pcn_po_full;
				}
			}

			if (pncookie && !error) {
				if (pn == NULL)
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
				pn->pn_nlookup++;
			}
			break;
		}

		case PUFFS_VN_RMDIR:
		{
			struct puffs_vnmsg_rmdir *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_rmdir == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			PUFFS_KCREDTOCRED(pcn.pcn_cred, &auxt->pvnr_cn_cred);

			error = pops->puffs_node_rmdir(pu,
			    opcookie, auxt->pvnr_cookie_targ, &pcn);
			break;
		}

		case PUFFS_VN_SYMLINK:
		{
			struct puffs_vnmsg_symlink *auxt = auxbuf;
			struct puffs_newinfo pni;
			struct puffs_cn pcn;
			struct puffs_node *pn = NULL;

			if (pops->puffs_node_symlink == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			PUFFS_KCREDTOCRED(pcn.pcn_cred, &auxt->pvnr_cn_cred);

			memset(&pni, 0, sizeof(pni));
			pni.pni_cookie = &auxt->pvnr_newnode;
			pni.pni_va = &auxt->pvnr_va;
			pni.pni_va_ttl = &auxt->pvnr_va_ttl;
			pni.pni_cn_ttl = &auxt->pvnr_cn_ttl;

			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn, opcookie);
				if (error)
					break;
			}

			error = pops->puffs_node_symlink(pu,
			    opcookie, &pni, &pcn,
			    &auxt->pvnr_va, auxt->pvnr_link);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					pn->pn_po = pcn.pcn_po_full;
				}
			}

			if (pncookie && !error) {
				if (pn == NULL)
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
				pn->pn_nlookup++;
			}
			break;
		}

		case PUFFS_VN_READDIR:
		{
			struct puffs_vnmsg_readdir *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);
			struct dirent *dent;
			off_t *cookies;
			size_t res, origcookies;

			if (pops->puffs_node_readdir == NULL) {
				error = 0;
				break;
			}

			if (auxt->pvnr_ncookies) {
				/* LINTED: pvnr_data is __aligned() */
				cookies = (off_t *)auxt->pvnr_data;
				origcookies = auxt->pvnr_ncookies;
			} else {
				cookies = NULL;
				origcookies = 0;
			}
			/* LINTED: dentoff is aligned in the kernel */
			dent = (struct dirent *)
			    (auxt->pvnr_data + auxt->pvnr_dentoff);

			res = auxt->pvnr_resid;
			error = pops->puffs_node_readdir(pu,
			    opcookie, dent, &auxt->pvnr_offset,
			    &auxt->pvnr_resid, pcr, &auxt->pvnr_eofflag,
			    cookies, &auxt->pvnr_ncookies);

			/* much easier to track non-working NFS */
			assert(auxt->pvnr_ncookies <= origcookies);

			/* need to move a bit more */
			preq->preq_buflen = sizeof(struct puffs_vnmsg_readdir) 
			    + auxt->pvnr_dentoff + (res - auxt->pvnr_resid);
			break;
		}

		case PUFFS_VN_READLINK:
		{
			struct puffs_vnmsg_readlink *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (pops->puffs_node_readlink == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			/*LINTED*/
			error = pops->puffs_node_readlink(pu, opcookie, pcr,
			    auxt->pvnr_link, &auxt->pvnr_linklen);
			break;
		}

		case PUFFS_VN_RECLAIM:
		{
			struct puffs_vnmsg_reclaim *auxt = auxbuf;
			struct puffs_node *pn;
		
			if (pops->puffs_node_reclaim2 != NULL) {
				error = pops->puffs_node_reclaim2(pu, opcookie,
					     auxt->pvnr_nlookup);
				break;
			}

			if (pops->puffs_node_reclaim == NULL) {
				error = 0;
				break;
			}

			/*
			 * This fixes a race condition, 
			 * where a node in reclaimed by kernel 
			 * after a lookup request is sent, 
			 * but before the reply, leaving the kernel
			 * with a invalid vnode/cookie reference.
			 */
			if (pncookie) {
				pn = PU_CMAP(pu, opcookie);
				pn->pn_nlookup -= auxt->pvnr_nlookup;
				if (pn->pn_nlookup >= 1) {
					error = 0;
					break;
				}
			}

			error = pops->puffs_node_reclaim(pu, opcookie);
			break;
		}

		case PUFFS_VN_INACTIVE:
		{

			if (pops->puffs_node_inactive == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			error = pops->puffs_node_inactive(pu, opcookie);
			break;
		}

		case PUFFS_VN_PATHCONF:
		{
			struct puffs_vnmsg_pathconf *auxt = auxbuf;
			if (pops->puffs_node_pathconf == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_pathconf(pu,
			    opcookie, auxt->pvnr_name,
			    &auxt->pvnr_retval);
			break;
		}

		case PUFFS_VN_ADVLOCK:
		{
			struct puffs_vnmsg_advlock *auxt = auxbuf;
			if (pops->puffs_node_advlock == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_advlock(pu,
			    opcookie, auxt->pvnr_id, auxt->pvnr_op,
			    &auxt->pvnr_fl, auxt->pvnr_flags);
			break;
		}

		case PUFFS_VN_PRINT:
		{
			if (pops->puffs_node_print == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_print(pu,
			    opcookie);
			break;
		}

		case PUFFS_VN_ABORTOP:
		{
			struct puffs_vnmsg_abortop *auxt = auxbuf;
			struct puffs_cn pcn;

			if (pops->puffs_node_abortop == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			PUFFS_KCREDTOCRED(pcn.pcn_cred, &auxt->pvnr_cn_cred);

			error = pops->puffs_node_abortop(pu, opcookie, &pcn);
				
			break;
		}

		case PUFFS_VN_READ:
		{
			struct puffs_vnmsg_read *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);
			size_t res;

			if (pops->puffs_node_read == NULL) {
				error = EIO;
				break;
			}

			res = auxt->pvnr_resid;
			error = pops->puffs_node_read(pu,
			    opcookie, auxt->pvnr_data,
			    auxt->pvnr_offset, &auxt->pvnr_resid,
			    pcr, auxt->pvnr_ioflag);

			/* need to move a bit more */
			preq->preq_buflen = sizeof(struct puffs_vnmsg_read)
			    + (res - auxt->pvnr_resid);
			break;
		}

		case PUFFS_VN_WRITE:
		{
			struct puffs_vnmsg_write *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (pops->puffs_node_write2 != NULL) {
				int xflag = 0;

				if (!PUFFSOP_WANTREPLY(preq->preq_opclass))
					xflag |= PUFFS_SETATTR_FAF;

				error = pops->puffs_node_write2(pu,
				    opcookie, auxt->pvnr_data,
				    auxt->pvnr_offset, &auxt->pvnr_resid,
				    pcr, auxt->pvnr_ioflag, xflag);

			} else if (pops->puffs_node_write != NULL) {
				error = pops->puffs_node_write(pu,
				    opcookie, auxt->pvnr_data,
				    auxt->pvnr_offset, &auxt->pvnr_resid,
				    pcr, auxt->pvnr_ioflag);
			} else {
				error = EIO;
				break;
			}


			/* don't need to move data back to the kernel */
			preq->preq_buflen = sizeof(struct puffs_vnmsg_write);
			break;
		}

		case PUFFS_VN_POLL:
		{
			struct puffs_vnmsg_poll *auxt = auxbuf;

			if (pops->puffs_node_poll == NULL) {
				error = 0;

				/* emulate genfs_poll() */
				auxt->pvnr_events &= (POLLIN | POLLOUT
						    | POLLRDNORM | POLLWRNORM);

				break;
			}

			error = pops->puffs_node_poll(pu,
			    opcookie, &auxt->pvnr_events);
			break;
		}

		case PUFFS_VN_GETEXTATTR:
		{
			struct puffs_vnmsg_getextattr *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);
			size_t res, *resp, *sizep;
			uint8_t *data;

			if (pops->puffs_node_getextattr == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			if (auxt->pvnr_datasize)
				sizep = &auxt->pvnr_datasize;
			else
				sizep = NULL;

			res = auxt->pvnr_resid;
			if (res > 0) {
				data = auxt->pvnr_data;
				resp = &auxt->pvnr_resid;
			} else {
				data = NULL;
				resp = NULL;
			}

			error = pops->puffs_node_getextattr(pu,
			    opcookie, auxt->pvnr_attrnamespace,
			    auxt->pvnr_attrname, sizep, data, resp, pcr);

			/* need to move a bit more? */
			preq->preq_buflen =
			    sizeof(struct puffs_vnmsg_getextattr)
			    + (res - auxt->pvnr_resid);
			break;
		}

		case PUFFS_VN_SETEXTATTR:
		{
			struct puffs_vnmsg_setextattr *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);
			size_t *resp;
			uint8_t *data;

			if (pops->puffs_node_setextattr == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			if (auxt->pvnr_resid > 0) {
				data = auxt->pvnr_data;
				resp = &auxt->pvnr_resid;
			} else {
				data = NULL;
				resp = NULL;
			}

			error = pops->puffs_node_setextattr(pu,
			    opcookie, auxt->pvnr_attrnamespace,
			    auxt->pvnr_attrname, data, resp, pcr);
			break;
		}

		case PUFFS_VN_LISTEXTATTR:
		{
			struct puffs_vnmsg_listextattr *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);
			size_t res, *resp, *sizep;
			int flag;
			uint8_t *data;

			if (pops->puffs_node_listextattr == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			if (auxt->pvnr_datasize)
				sizep = &auxt->pvnr_datasize;
			else
				sizep = NULL;

			res = auxt->pvnr_resid;
			if (res > 0) {
				data = auxt->pvnr_data;
				resp = &auxt->pvnr_resid;
			} else {
				data = NULL;
				resp = NULL;
			}

			res = auxt->pvnr_resid;
			flag = auxt->pvnr_flag;
			error = pops->puffs_node_listextattr(pu,
			    opcookie, auxt->pvnr_attrnamespace,
			    sizep, data, resp, flag, pcr);

			/* need to move a bit more? */
			preq->preq_buflen =
			    sizeof(struct puffs_vnmsg_listextattr)
			    + (res - auxt->pvnr_resid);
			break;
		}

		case PUFFS_VN_DELETEEXTATTR:
		{
			struct puffs_vnmsg_deleteextattr *auxt = auxbuf;
			PUFFS_MAKECRED(pcr, &auxt->pvnr_cred);

			if (pops->puffs_node_deleteextattr == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			error = pops->puffs_node_deleteextattr(pu,
			    opcookie, auxt->pvnr_attrnamespace,
			    auxt->pvnr_attrname, pcr);
			break;
		}

		case PUFFS_VN_FALLOCATE:
		{
			struct puffs_vnmsg_fallocate *auxt = auxbuf;

			if (pops->puffs_node_fallocate == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			error = pops->puffs_node_fallocate(pu,
			    opcookie, auxt->pvnr_off, auxt->pvnr_len);
			break;
		}

		case PUFFS_VN_FDISCARD:
		{
			struct puffs_vnmsg_fdiscard *auxt = auxbuf;

			if (pops->puffs_node_fdiscard == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			error = pops->puffs_node_fdiscard(pu,
			    opcookie, auxt->pvnr_off, auxt->pvnr_len);
			break;
		}

		default:
			printf("inval op %d\n", preq->preq_optype);
			error = EINVAL;
			break;
		}

#if 0
	/* not issued by kernel currently */
	} else if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_CACHE) {
		struct puffs_cacheinfo *pci = (void *)preq;

		if (pu->pu_ops.puffs_cache_write) {
			pu->pu_ops.puffs_cache_write(pu, preq->preq_cookie,
			    pci->pcache_nruns, pci->pcache_runs);
		}
		error = 0;
#endif

	} else if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_ERROR) {
		struct puffs_error *perr = (void *)preq;

		pu->pu_errnotify(pu, preq->preq_optype,
		    perr->perr_error, perr->perr_str, preq->preq_cookie);
		error = 0;
	} else {
		/*
		 * I guess the kernel sees this one coming also
		 */
		error = EINVAL;
	}

 out:
	preq->preq_rv = error;

	if (pu->pu_oppost)
		pu->pu_oppost(pu);

	pcc->pcc_flags |= PCC_DONE;
}
