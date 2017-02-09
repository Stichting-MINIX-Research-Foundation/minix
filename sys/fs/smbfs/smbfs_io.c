/*	$NetBSD: smbfs_io.c,v 1.34 2010/04/23 15:38:47 pooka Exp $	*/

/*
 * Copyright (c) 2000-2001, Boris Popov
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
 * FreeBSD: src/sys/fs/smbfs/smbfs_io.c,v 1.7 2001/12/02 08:56:58 bp Exp
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smbfs_io.c,v 1.34 2010/04/23 15:38:47 pooka Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>	/* defines plimit structure in proc struct */
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/kauth.h>

#ifndef __NetBSD__
#include <vm/vm.h>
#if __FreeBSD_version < 400000
#include <vm/vm_prot.h>
#endif
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#else /* __NetBSD__ */
#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>
#endif /* __NetBSD__ */

/*
#include <sys/ioccom.h>
*/
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

/*#define SMBFS_RWGENERIC*/

#define DE_SIZE	(sizeof(struct dirent))

static int
smbfs_readvdir(struct vnode *vp, struct uio *uio, kauth_cred_t cred)
{
	struct dirent *de;
	struct smb_cred scred;
	struct smbfs_fctx *ctx;
	struct smbnode *np = VTOSMB(vp);
	int error = 0/*, *eofflag = ap->a_eofflag*/;
	long offset, limit;

	KASSERT(vp->v_type == VDIR);

	if (uio->uio_resid < DE_SIZE || uio->uio_offset < 0)
		return EINVAL;

	de = malloc(DE_SIZE, M_SMBFSDATA, M_WAITOK | M_ZERO);

	SMBVDEBUG("dirname='%.*s'\n", (int) np->n_nmlen, np->n_name);
	smb_makescred(&scred, curlwp, cred);
	offset = uio->uio_offset / DE_SIZE; 	/* offset in the directory */
	limit = uio->uio_resid / DE_SIZE;

	/* Simulate . */
	if (offset < 1) {
		memset(de, 0, DE_SIZE);
		de->d_fileno = np->n_ino;
		de->d_reclen = DE_SIZE;
		de->d_type = DT_DIR;
		de->d_namlen = 1;
		strncpy(de->d_name, ".", 2);
		error = uiomove(de, DE_SIZE, uio);
		if (error)
			goto out;
		limit--;
		offset++;
	}
	/* Simulate .. */
	if (limit > 0 && offset < 2) {
		memset(de, 0, DE_SIZE);
		de->d_fileno = (np->n_parent ? VTOSMB(np->n_parent)->n_ino : 2);
		de->d_reclen = DE_SIZE;
		de->d_type = DT_DIR;
		de->d_namlen = 2;
		strncpy(de->d_name, "..", 3);
		error = uiomove(de, DE_SIZE, uio);
		if (error)
			goto out;
		limit--;
		offset++;
	}

	if (limit == 0)
		goto out;

	if (offset != np->n_dirofs || np->n_dirseq == NULL) {
		SMBVDEBUG("Reopening search %ld:%ld\n", offset, np->n_dirofs);
		if (np->n_dirseq) {
			smbfs_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
		}
		np->n_dirofs = 2;
		error = smbfs_findopen(np, "*", 1,
		    SMB_FA_SYSTEM | SMB_FA_HIDDEN | SMB_FA_DIR,
		    &scred, &ctx);
		if (error) {
			SMBVDEBUG("can not open search, error = %d", error);
			goto out;
		}
		np->n_dirseq = ctx;
	} else
		ctx = np->n_dirseq;

	/* skip entries before offset */
	while (np->n_dirofs < offset) {
		error = smbfs_findnext(ctx, offset - np->n_dirofs++, &scred);
		if (error) {
			smbfs_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
			error = (error == ENOENT) ? 0 : error;
			goto out;
		}
	}

	for (; limit; limit--, offset++) {
		error = smbfs_findnext(ctx, limit, &scred);
		if (error) {
			if (error == ENOENT)
				error = 0;
			break;
		}
		np->n_dirofs++;
		memset(de, 0, DE_SIZE);
		de->d_reclen = DE_SIZE;
		de->d_fileno = ctx->f_attr.fa_ino;
		de->d_type = (ctx->f_attr.fa_attr & SMB_FA_DIR) ?
		    DT_DIR : DT_REG;
		de->d_namlen = ctx->f_nmlen;
		memcpy(de->d_name, ctx->f_name, de->d_namlen);
		de->d_name[de->d_namlen] = '\0';
		error = uiomove(de, DE_SIZE, uio);
		if (error)
			break;
	}

out:
	free(de, M_SMBFSDATA);
	return (error);
}

int
smbfs_readvnode(struct vnode *vp, struct uio *uiop, kauth_cred_t cred)
{
	struct smbmount *smp = VFSTOSMBFS(vp->v_mount);
	struct smbnode *np = VTOSMB(vp);
	struct lwp *l = curlwp;
	struct vattr vattr;
	struct smb_cred scred;
	int error;

	KASSERT(vp->v_type == VREG || vp->v_type == VDIR);

	if (uiop->uio_resid == 0)
		return 0;
	if (uiop->uio_offset < 0)
		return EINVAL;
/*	if (uiop->uio_offset + uiop->uio_resid > smp->nm_maxfilesize)
		return EFBIG;*/
	if (vp->v_type == VDIR) {
		error = smbfs_readvdir(vp, uiop, cred);
		return error;
	}

	if (np->n_flag & NMODIFIED) {
		smbfs_attr_cacheremove(vp);
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error)
			return error;
		np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error)
			return error;
		if (np->n_mtime.tv_sec != vattr.va_mtime.tv_sec) {
			error = smbfs_vinvalbuf(vp, V_SAVE, cred, l, 1);
			if (error)
				return error;
			np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
		}
	}
	smb_makescred(&scred, l, cred);
	return smb_read(smp->sm_share, np->n_fid, uiop, &scred);
}

int
smbfs_writevnode(struct vnode *vp, struct uio *uiop,
	kauth_cred_t cred, int ioflag)
{
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *np = VTOSMB(vp);
	struct lwp *l = curlwp;
	struct smb_cred scred;
	int error = 0;
	int extended = 0;
	size_t resid = uiop->uio_resid;

	/* vn types other than VREG unsupported */
	KASSERT(vp->v_type == VREG);

	SMBVDEBUG("ofs=%lld,resid=%zu\n",
		(long long int) uiop->uio_offset,
		uiop->uio_resid);
	if (uiop->uio_offset < 0)
		return EINVAL;
/*	if (uiop->uio_offset + uiop->uio_resid > smp->nm_maxfilesize)
		return (EFBIG);*/
	if (ioflag & (IO_APPEND | IO_SYNC)) {
		if (np->n_flag & NMODIFIED) {
			smbfs_attr_cacheremove(vp);
			error = smbfs_vinvalbuf(vp, V_SAVE, cred, l, 1);
			if (error)
				return error;
		}
		if (ioflag & IO_APPEND) {
#if notyet
			struct proc *p = curproc;

			/*
			 * File size can be changed by another client
			 */
			smbfs_attr_cacheremove(vp);
			error = VOP_GETATTR(vp, &vattr, cred, td);
			if (error)
				return (error);
			if (np->n_size + uiop->uio_resid >
			    p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
				mutex_enter(proc_lock);
				psignal(p, SIGXFSZ);
				mutex_exit(proc_lock);
				return EFBIG;
			}
#endif
			uiop->uio_offset = np->n_size;
		}
	}
	if (uiop->uio_resid == 0)
		return 0;
	smb_makescred(&scred, l, cred);
	error = smb_write(smp->sm_share, np->n_fid, uiop, &scred);
	SMBVDEBUG("after: ofs=%lld,resid=%zu,err=%d\n",
	    (long long int)uiop->uio_offset, uiop->uio_resid, error);
	if (!error) {
		if (uiop->uio_offset > np->n_size) {
			np->n_size = uiop->uio_offset;
			uvm_vnp_setsize(vp, np->n_size);
			extended = 1;
		}

	}
	if (resid > uiop->uio_resid)
		VN_KNOTE(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));
	return error;
}

/*
 * Do an I/O operation to/from a cache block.
 */
int
smbfs_doio(struct buf *bp, kauth_cred_t cr, struct lwp *l)
{
	struct vnode *vp = bp->b_vp;
	struct smbmount *smp = VFSTOSMBFS(vp->v_mount);
	struct smbnode *np = VTOSMB(vp);
	struct uio uio, *uiop = &uio;
	struct iovec io;
	struct smb_cred scred;
	int error = 0;

	uiop->uio_iov = &io;
	uiop->uio_iovcnt = 1;
	UIO_SETUP_SYSSPACE(uiop);

	smb_makescred(&scred, l, cr);

	if (bp->b_flags == B_READ) {
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    io.iov_base = bp->b_data;
	    uiop->uio_rw = UIO_READ;
	    switch (vp->v_type) {
	      case VREG:
		uiop->uio_offset = ((off_t)bp->b_blkno) * DEV_BSIZE;
		error = smb_read(smp->sm_share, np->n_fid, uiop, &scred);
		if (error)
			break;
		if (uiop->uio_resid) {
			int left = uiop->uio_resid;
			int nread = bp->b_bcount - left;
			if (left > 0)
			    memset((char *)bp->b_data + nread, 0, left);
		}
		break;
	    default:
		printf("smbfs_doio:  type %x unexpected\n",vp->v_type);
		break;
	    };
	    if (error) {
		bp->b_error = error;
	    }
	} else { /* write */
		io.iov_len = uiop->uio_resid = bp->b_bcount;
		uiop->uio_offset = ((off_t)bp->b_blkno) << DEV_BSHIFT;
		io.iov_base = bp->b_data;
		uiop->uio_rw = UIO_WRITE;
		bp->b_cflags |= BC_BUSY;
		error = smb_write(smp->sm_share, np->n_fid, uiop, &scred);
		bp->b_cflags &= ~BC_BUSY;


#ifndef __NetBSD__ /* XXX */
		/*
		 * For an interrupted write, the buffer is still valid
		 * and the write hasn't been pushed to the server yet,
		 * so we can't set BIO_ERROR and report the interruption
		 * by setting B_EINTR. For the B_ASYNC case, B_EINTR
		 * is not relevant, so the rpc attempt is essentially
		 * a noop.  For the case of a V3 write rpc not being
		 * committed to stable storage, the block is still
		 * dirty and requires either a commit rpc or another
		 * write rpc with iomode == NFSV3WRITE_FILESYNC before
		 * the block is reused. This is indicated by setting
		 * the B_DELWRI and B_NEEDCOMMIT flags.
		 */
    		if (error == EINTR
		    || (!error && (bp->b_flags & B_NEEDCOMMIT))) {
			int s;

			s = splbio();
			bp->b_flags &= ~(B_INVAL|B_NOCACHE);
			if ((bp->b_flags & B_ASYNC) == 0)
			    bp->b_flags |= B_EINTR;
			if ((bp->b_flags & B_PAGING) == 0) {
			    bdirty(bp);
			    bp->b_flags &= ~B_DONE;
			}
			if ((bp->b_flags & B_ASYNC) == 0)
			    bp->b_flags |= B_EINTR;
			splx(s);
	    	} else {
			if (error) {
				bp->b_ioflags |= BIO_ERROR;
				bp->b_error = error;
			}
			bp->b_dirtyoff = bp->b_dirtyend = 0;
		}
#endif /* !__NetBSD__ */
	}
	bp->b_resid = uiop->uio_resid;
	biodone(bp);
	return error;
}

/*
 * Flush and invalidate all dirty buffers. If another process is already
 * doing the flush, just wait for completion.
 */
int
smbfs_vinvalbuf(struct vnode *vp, int flags, kauth_cred_t cred, struct lwp *l, int intrflg)
{
	struct smbnode *np = VTOSMB(vp);
	int error = 0, slpflag;

	if (intrflg)
		slpflag = PCATCH;
	else
		slpflag = 0;

	while (np->n_flag & NFLUSHINPROG) {
		np->n_flag |= NFLUSHWANT;
		error = tsleep(&np->n_flag,
			(PRIBIO + 2) | slpflag, "smfsvinv", 0);
		if (error)
			return (error);
	}
	np->n_flag |= NFLUSHINPROG;
	for(;;) {
		if ((error = vinvalbuf(vp, flags, cred, l, slpflag, 0)) == 0)
			break;

		if (intrflg && (error == ERESTART || error == EINTR)) {
			np->n_flag &= ~NFLUSHINPROG;
			if (np->n_flag & NFLUSHWANT) {
				np->n_flag &= ~NFLUSHWANT;
				wakeup(&np->n_flag);
			}
			return (error);
		}
	}
	np->n_flag &= ~(NMODIFIED | NFLUSHINPROG);
	if (np->n_flag & NFLUSHWANT) {
		np->n_flag &= ~NFLUSHWANT;
		wakeup(&np->n_flag);
	}
	return (error);
}
