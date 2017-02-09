/*	$NetBSD: nfs_bio.c,v 1.191 2015/07/15 03:28:55 manu Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)nfs_bio.c	8.9 (Berkeley) 3/30/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nfs_bio.c,v 1.191 2015/07/15 03:28:55 manu Exp $");

#ifdef _KERNEL_OPT
#include "opt_nfs.h"
#include "opt_ddb.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/kauth.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs_var.h>

extern int nfs_numasync;
extern int nfs_commitsize;
extern struct nfsstats nfsstats;

static int nfs_doio_read(struct buf *, struct uio *);
static int nfs_doio_write(struct buf *, struct uio *);
static int nfs_doio_phys(struct buf *, struct uio *);

/*
 * Vnode op for read using bio
 * Any similarity to readip() is purely coincidental
 */
int
nfs_bioread(struct vnode *vp, struct uio *uio, int ioflag,
	    kauth_cred_t cred, int cflag)
{
	struct nfsnode *np = VTONFS(vp);
	struct buf *bp = NULL, *rabp;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct nfsdircache *ndp = NULL, *nndp = NULL;
	void *baddr;
	int got_buf = 0, error = 0, n = 0, on = 0, en, enn;
	int enough = 0;
	struct dirent *dp, *pdp, *edp, *ep;
	off_t curoff = 0;
	int advice;
	struct lwp *l = curlwp;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("nfs_read mode");
#endif
	if (uio->uio_resid == 0)
		return (0);
	if (vp->v_type != VDIR && uio->uio_offset < 0)
		return (EINVAL);
#ifndef NFS_V2_ONLY
	if ((nmp->nm_flag & NFSMNT_NFSV3) &&
	    !(nmp->nm_iflag & NFSMNT_GOTFSINFO))
		(void)nfs_fsinfo(nmp, vp, cred, l);
#endif
	if (vp->v_type != VDIR &&
	    (uio->uio_offset + uio->uio_resid) > nmp->nm_maxfilesize)
		return (EFBIG);

	/*
	 * For nfs, cache consistency can only be maintained approximately.
	 * Although RFC1094 does not specify the criteria, the following is
	 * believed to be compatible with the reference port.
	 *
	 * If the file's modify time on the server has changed since the
	 * last read rpc or you have written to the file,
	 * you may have lost data cache consistency with the
	 * server, so flush all of the file's data out of the cache.
	 * Then force a getattr rpc to ensure that you have up to date
	 * attributes.
	 * NB: This implies that cache data can be read when up to
	 * nfs_attrtimeo seconds out of date. If you find that you need current
	 * attributes this could be forced by setting n_attrstamp to 0 before
	 * the VOP_GETATTR() call.
	 */

	if (vp->v_type != VLNK) {
		error = nfs_flushstalebuf(vp, cred, l,
		    NFS_FLUSHSTALEBUF_MYWRITE);
		if (error)
			return error;
	}

	do {
	    /*
	     * Don't cache symlinks.
	     */
	    if ((vp->v_vflag & VV_ROOT) && vp->v_type == VLNK) {
		return (nfs_readlinkrpc(vp, uio, cred));
	    }
	    baddr = (void *)0;
	    switch (vp->v_type) {
	    case VREG:
		nfsstats.biocache_reads++;

		advice = IO_ADV_DECODE(ioflag);
		error = 0;
		while (uio->uio_resid > 0) {
			vsize_t bytelen;

			nfs_delayedtruncate(vp);
			if (np->n_size <= uio->uio_offset) {
				break;
			}
			bytelen =
			    MIN(np->n_size - uio->uio_offset, uio->uio_resid);
			error = ubc_uiomove(&vp->v_uobj, uio, bytelen, advice,
			    UBC_READ | UBC_PARTIALOK | UBC_UNMAP_FLAG(vp));
			if (error) {
				/*
				 * XXXkludge
				 * the file has been truncated on the server.
				 * there isn't much we can do.
				 */
				if (uio->uio_offset >= np->n_size) {
					/* end of file */
					error = 0;
				} else {
					break;
				}
			}
		}
		break;

	    case VLNK:
		nfsstats.biocache_readlinks++;
		bp = nfs_getcacheblk(vp, (daddr_t)0, MAXPATHLEN, l);
		if (!bp)
			return (EINTR);
		if ((bp->b_oflags & BO_DONE) == 0) {
			bp->b_flags |= B_READ;
			error = nfs_doio(bp);
			if (error) {
				brelse(bp, 0);
				return (error);
			}
		}
		n = MIN(uio->uio_resid, MAXPATHLEN - bp->b_resid);
		got_buf = 1;
		on = 0;
		break;
	    case VDIR:
diragain:
		nfsstats.biocache_readdirs++;
		ndp = nfs_searchdircache(vp, uio->uio_offset,
			(nmp->nm_flag & NFSMNT_XLATECOOKIE), 0);
		if (!ndp) {
			/*
			 * We've been handed a cookie that is not
			 * in the cache. If we're not translating
			 * 32 <-> 64, it may be a value that was
			 * flushed out of the cache because it grew
			 * too big. Let the server judge if it's
			 * valid or not. In the translation case,
			 * we have no way of validating this value,
			 * so punt.
			 */
			if (nmp->nm_flag & NFSMNT_XLATECOOKIE)
				return (EINVAL);
			ndp = nfs_enterdircache(vp, uio->uio_offset,
				uio->uio_offset, 0, 0);
		}

		if (NFS_EOFVALID(np) &&
		    ndp->dc_cookie == np->n_direofoffset) {
			nfs_putdircache(np, ndp);
			nfsstats.direofcache_hits++;
			return (0);
		}

		bp = nfs_getcacheblk(vp, NFSDC_BLKNO(ndp), NFS_DIRBLKSIZ, l);
		if (!bp)
		    return (EINTR);
		if ((bp->b_oflags & BO_DONE) == 0) {
		    bp->b_flags |= B_READ;
		    bp->b_dcookie = ndp->dc_blkcookie;
		    error = nfs_doio(bp);
		    if (error) {
			/*
			 * Yuck! The directory has been modified on the
			 * server. Punt and let the userland code
			 * deal with it.
			 */
			nfs_putdircache(np, ndp);
			brelse(bp, 0);
			/*
			 * nfs_request maps NFSERR_BAD_COOKIE to EINVAL.
			 */
			if (error == EINVAL) { /* NFSERR_BAD_COOKIE */
			    nfs_invaldircache(vp, 0);
			    nfs_vinvalbuf(vp, 0, cred, l, 1);
			}
			return (error);
		    }
		}

		/*
		 * Just return if we hit EOF right away with this
		 * block. Always check here, because direofoffset
		 * may have been set by an nfsiod since the last
		 * check.
		 *
		 * also, empty block implies EOF.
		 */

		if (bp->b_bcount == bp->b_resid ||
		    (NFS_EOFVALID(np) &&
		    ndp->dc_blkcookie == np->n_direofoffset)) {
			KASSERT(bp->b_bcount != bp->b_resid ||
			    ndp->dc_blkcookie == bp->b_dcookie);
			nfs_putdircache(np, ndp);
			brelse(bp, BC_NOCACHE);
			return 0;
		}

		/*
		 * Find the entry we were looking for in the block.
		 */

		en = ndp->dc_entry;

		pdp = dp = (struct dirent *)bp->b_data;
		edp = (struct dirent *)(void *)((char *)bp->b_data + bp->b_bcount -
		    bp->b_resid);
		enn = 0;
		while (enn < en && dp < edp) {
			pdp = dp;
			dp = _DIRENT_NEXT(dp);
			enn++;
		}

		/*
		 * If the entry number was bigger than the number of
		 * entries in the block, or the cookie of the previous
		 * entry doesn't match, the directory cache is
		 * stale. Flush it and try again (i.e. go to
		 * the server).
		 */
		if (dp >= edp || (struct dirent *)_DIRENT_NEXT(dp) > edp ||
		    (en > 0 && NFS_GETCOOKIE(pdp) != ndp->dc_cookie)) {
#ifdef DEBUG
		    	printf("invalid cache: %p %p %p off %jx %jx\n",
				pdp, dp, edp,
				(uintmax_t)uio->uio_offset,
				(uintmax_t)NFS_GETCOOKIE(pdp));
#endif
			nfs_putdircache(np, ndp);
			brelse(bp, 0);
			nfs_invaldircache(vp, 0);
			nfs_vinvalbuf(vp, 0, cred, l, 0);
			goto diragain;
		}

		on = (char *)dp - (char *)bp->b_data;

		/*
		 * Cache all entries that may be exported to the
		 * user, as they may be thrown back at us. The
		 * NFSBIO_CACHECOOKIES flag indicates that all
		 * entries are being 'exported', so cache them all.
		 */

		if (en == 0 && pdp == dp) {
			dp = _DIRENT_NEXT(dp);
			enn++;
		}

		if (uio->uio_resid < (bp->b_bcount - bp->b_resid - on)) {
			n = uio->uio_resid;
			enough = 1;
		} else
			n = bp->b_bcount - bp->b_resid - on;

		ep = (struct dirent *)(void *)((char *)bp->b_data + on + n);

		/*
		 * Find last complete entry to copy, caching entries
		 * (if requested) as we go.
		 */

		while (dp < ep && (struct dirent *)_DIRENT_NEXT(dp) <= ep) {
			if (cflag & NFSBIO_CACHECOOKIES) {
				nndp = nfs_enterdircache(vp, NFS_GETCOOKIE(pdp),
				    ndp->dc_blkcookie, enn, bp->b_lblkno);
				if (nmp->nm_flag & NFSMNT_XLATECOOKIE) {
					NFS_STASHCOOKIE32(pdp,
					    nndp->dc_cookie32);
				}
				nfs_putdircache(np, nndp);
			}
			pdp = dp;
			dp = _DIRENT_NEXT(dp);
			enn++;
		}
		nfs_putdircache(np, ndp);

		/*
		 * If the last requested entry was not the last in the
		 * buffer (happens if NFS_DIRFRAGSIZ < NFS_DIRBLKSIZ),
		 * cache the cookie of the last requested one, and
		 * set of the offset to it.
		 */

		if ((on + n) < bp->b_bcount - bp->b_resid) {
			curoff = NFS_GETCOOKIE(pdp);
			nndp = nfs_enterdircache(vp, curoff, ndp->dc_blkcookie,
			    enn, bp->b_lblkno);
			if (nmp->nm_flag & NFSMNT_XLATECOOKIE) {
				NFS_STASHCOOKIE32(pdp, nndp->dc_cookie32);
				curoff = nndp->dc_cookie32;
			}
			nfs_putdircache(np, nndp);
		} else
			curoff = bp->b_dcookie;

		/*
		 * Always cache the entry for the next block,
		 * so that readaheads can use it.
		 */
		nndp = nfs_enterdircache(vp, bp->b_dcookie, bp->b_dcookie, 0,0);
		if (nmp->nm_flag & NFSMNT_XLATECOOKIE) {
			if (curoff == bp->b_dcookie) {
				NFS_STASHCOOKIE32(pdp, nndp->dc_cookie32);
				curoff = nndp->dc_cookie32;
			}
		}

		n = (char *)_DIRENT_NEXT(pdp) - ((char *)bp->b_data + on);

		/*
		 * If not eof and read aheads are enabled, start one.
		 * (You need the current block first, so that you have the
		 *  directory offset cookie of the next block.)
		 */
		if (nfs_numasync > 0 && nmp->nm_readahead > 0 &&
		    !NFS_EOFVALID(np)) {
			rabp = nfs_getcacheblk(vp, NFSDC_BLKNO(nndp),
						NFS_DIRBLKSIZ, l);
			if (rabp) {
			    if ((rabp->b_oflags & (BO_DONE | BO_DELWRI)) == 0) {
				rabp->b_dcookie = nndp->dc_cookie;
				rabp->b_flags |= (B_READ | B_ASYNC);
				if (nfs_asyncio(rabp)) {
				    brelse(rabp, BC_INVAL);
				}
			    } else
				brelse(rabp, 0);
			}
		}
		nfs_putdircache(np, nndp);
		got_buf = 1;
		break;
	    default:
		printf(" nfsbioread: type %x unexpected\n",vp->v_type);
		break;
	    }

	    if (n > 0) {
		if (!baddr)
			baddr = bp->b_data;
		error = uiomove((char *)baddr + on, (int)n, uio);
	    }
	    switch (vp->v_type) {
	    case VREG:
		break;
	    case VLNK:
		n = 0;
		break;
	    case VDIR:
		uio->uio_offset = curoff;
		if (enough)
			n = 0;
		break;
	    default:
		printf(" nfsbioread: type %x unexpected\n",vp->v_type);
	    }
	    if (got_buf)
		brelse(bp, 0);
	} while (error == 0 && uio->uio_resid > 0 && n > 0);
	return (error);
}

/*
 * Vnode op for write using bio
 */
int
nfs_write(void *v)
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		kauth_cred_t a_cred;
	} */ *ap = v;
	struct uio *uio = ap->a_uio;
	struct lwp *l = curlwp;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	kauth_cred_t cred = ap->a_cred;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	voff_t oldoff, origoff;
	vsize_t bytelen;
	int error = 0;
	int ioflag = ap->a_ioflag;
	int extended = 0, wrotedata = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("nfs_write mode");
#endif
	if (vp->v_type != VREG)
		return (EIO);
	if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		return (np->n_error);
	}
#ifndef NFS_V2_ONLY
	if ((nmp->nm_flag & NFSMNT_NFSV3) &&
	    !(nmp->nm_iflag & NFSMNT_GOTFSINFO))
		(void)nfs_fsinfo(nmp, vp, cred, l);
#endif
	if (ioflag & IO_APPEND) {
		NFS_INVALIDATE_ATTRCACHE(np);
		error = nfs_flushstalebuf(vp, cred, l,
		    NFS_FLUSHSTALEBUF_MYWRITE);
		if (error)
			return (error);
		uio->uio_offset = np->n_size;

		/*
		 * This is already checked above VOP_WRITE, but recheck
		 * the append case here to make sure our idea of the
		 * file size is as fresh as possible.
		 */
		if (uio->uio_offset + uio->uio_resid >
		      l->l_proc->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
			mutex_enter(proc_lock);
			psignal(l->l_proc, SIGXFSZ);
			mutex_exit(proc_lock);
			return (EFBIG);
		}
	}
	if (uio->uio_offset < 0)
		return (EINVAL);
	if ((uio->uio_offset + uio->uio_resid) > nmp->nm_maxfilesize)
		return (EFBIG);
	if (uio->uio_resid == 0)
		return (0);

	origoff = uio->uio_offset;
	do {
		bool overwrite; /* if we are overwriting whole pages */
		u_quad_t oldsize;
		oldoff = uio->uio_offset;
		bytelen = uio->uio_resid;

		nfsstats.biocache_writes++;

		oldsize = np->n_size;
		np->n_flag |= NMODIFIED;
		if (np->n_size < uio->uio_offset + bytelen) {
			np->n_size = uio->uio_offset + bytelen;
		}
		overwrite = false;
		if ((uio->uio_offset & PAGE_MASK) == 0) {
			if ((vp->v_vflag & VV_MAPPED) == 0 &&
			    bytelen > PAGE_SIZE) {
				bytelen = trunc_page(bytelen);
				overwrite = true;
			} else if ((bytelen & PAGE_MASK) == 0 &&
			    uio->uio_offset >= vp->v_size) {
				overwrite = true;
			}
		}
		if (vp->v_size < uio->uio_offset + bytelen) {
			uvm_vnp_setwritesize(vp, uio->uio_offset + bytelen);
		}
		error = ubc_uiomove(&vp->v_uobj, uio, bytelen,
		    UVM_ADV_RANDOM, UBC_WRITE | UBC_PARTIALOK |
		    (overwrite ? UBC_FAULTBUSY : 0) |
		    UBC_UNMAP_FLAG(vp));
		if (error) {
			uvm_vnp_setwritesize(vp, vp->v_size);
			if (overwrite && np->n_size != oldsize) {
				/*
				 * backout size and free pages past eof.
				 */
				np->n_size = oldsize;
				mutex_enter(vp->v_interlock);
				(void)VOP_PUTPAGES(vp, round_page(vp->v_size),
				    0, PGO_SYNCIO | PGO_FREE);
			}
			break;
		}
		wrotedata = 1;

		/*
		 * update UVM's notion of the size now that we've
		 * copied the data into the vnode's pages.
		 */

		if (vp->v_size < uio->uio_offset) {
			uvm_vnp_setsize(vp, uio->uio_offset);
			extended = 1;
		}

		if ((oldoff & ~(nmp->nm_wsize - 1)) !=
		    (uio->uio_offset & ~(nmp->nm_wsize - 1))) {
			mutex_enter(vp->v_interlock);
			error = VOP_PUTPAGES(vp,
			    trunc_page(oldoff & ~(nmp->nm_wsize - 1)),
			    round_page((uio->uio_offset + nmp->nm_wsize - 1) &
				       ~(nmp->nm_wsize - 1)), PGO_CLEANIT);
		}
	} while (uio->uio_resid > 0);
	if (wrotedata)
		VN_KNOTE(vp, NOTE_WRITE | (extended ? NOTE_EXTEND : 0));
	if (error == 0 && (ioflag & IO_SYNC) != 0) {
		mutex_enter(vp->v_interlock);
		error = VOP_PUTPAGES(vp,
		    trunc_page(origoff & ~(nmp->nm_wsize - 1)),
		    round_page((uio->uio_offset + nmp->nm_wsize - 1) &
			       ~(nmp->nm_wsize - 1)),
		    PGO_CLEANIT | PGO_SYNCIO);
	}
	return error;
}

/*
 * Get an nfs cache block.
 * Allocate a new one if the block isn't currently in the cache
 * and return the block marked busy. If the calling process is
 * interrupted by a signal for an interruptible mount point, return
 * NULL.
 */
struct buf *
nfs_getcacheblk(struct vnode *vp, daddr_t bn, int size, struct lwp *l)
{
	struct buf *bp;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);

	if (nmp->nm_flag & NFSMNT_INT) {
		bp = getblk(vp, bn, size, PCATCH, 0);
		while (bp == NULL) {
			if (nfs_sigintr(nmp, NULL, l))
				return (NULL);
			bp = getblk(vp, bn, size, 0, 2 * hz);
		}
	} else
		bp = getblk(vp, bn, size, 0, 0);
	return (bp);
}

/*
 * Flush and invalidate all dirty buffers. If another process is already
 * doing the flush, just wait for completion.
 */
int
nfs_vinvalbuf(struct vnode *vp, int flags, kauth_cred_t cred,
		struct lwp *l, int intrflg)
{
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, allerror = 0, slptimeo;
	bool catch_p;

	if ((nmp->nm_flag & NFSMNT_INT) == 0)
		intrflg = 0;
	if (intrflg) {
		catch_p = true;
		slptimeo = 2 * hz;
	} else {
		catch_p = false;
		if (nmp->nm_flag & NFSMNT_SOFT)
			slptimeo = nmp->nm_retry * nmp->nm_timeo;
		else
			slptimeo = 0;
	}
	/*
	 * First wait for any other process doing a flush to complete.
	 */
	mutex_enter(vp->v_interlock);
	while (np->n_flag & NFLUSHINPROG) {
		np->n_flag |= NFLUSHWANT;
		error = mtsleep(&np->n_flag, PRIBIO + 2, "nfsvinval",
			slptimeo, vp->v_interlock);
		if (error && intrflg && nfs_sigintr(nmp, NULL, l)) {
			mutex_exit(vp->v_interlock);
			return EINTR;
		}
	}

	/*
	 * Now, flush as required.
	 */
	np->n_flag |= NFLUSHINPROG;
	mutex_exit(vp->v_interlock);
	error = vinvalbuf(vp, flags, cred, l, catch_p, 0);
	while (error) {
		if (allerror == 0)
			allerror = error;
		if (intrflg && nfs_sigintr(nmp, NULL, l)) {
			error = EINTR;
			break;
		}
		error = vinvalbuf(vp, flags, cred, l, 0, slptimeo);
	}
	mutex_enter(vp->v_interlock);
	if (allerror != 0) {
		/*
		 * Keep error from vinvalbuf so fsync/close will know.
		 */
		np->n_error = allerror;
		np->n_flag |= NWRITEERR;
	}
	if (error == 0)
		np->n_flag &= ~NMODIFIED;
	np->n_flag &= ~NFLUSHINPROG;
	if (np->n_flag & NFLUSHWANT) {
		np->n_flag &= ~NFLUSHWANT;
		wakeup(&np->n_flag);
	}
	mutex_exit(vp->v_interlock);
	return error;
}

/*
 * nfs_flushstalebuf: flush cache if it's stale.
 *
 * => caller shouldn't own any pages or buffers which belong to the vnode.
 */

int
nfs_flushstalebuf(struct vnode *vp, kauth_cred_t cred, struct lwp *l,
    int flags)
{
	struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;
	int error;

	if (np->n_flag & NMODIFIED) {
		if ((flags & NFS_FLUSHSTALEBUF_MYWRITE) == 0
		    || vp->v_type != VREG) {
			error = nfs_vinvalbuf(vp, V_SAVE, cred, l, 1);
			if (error)
				return error;
			if (vp->v_type == VDIR) {
				nfs_invaldircache(vp, 0);
			}
		} else {
			/*
			 * XXX assuming writes are ours.
			 */
		}
		NFS_INVALIDATE_ATTRCACHE(np);
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error)
			return error;
		np->n_mtime = vattr.va_mtime;
	} else {
		error = VOP_GETATTR(vp, &vattr, cred);
		if (error)
			return error;
		if (timespeccmp(&np->n_mtime, &vattr.va_mtime, !=)) {
			if (vp->v_type == VDIR) {
				nfs_invaldircache(vp, 0);
			}
			error = nfs_vinvalbuf(vp, V_SAVE, cred, l, 1);
			if (error)
				return error;
			np->n_mtime = vattr.va_mtime;
		}
	}

	return error;
}

/*
 * Initiate asynchronous I/O. Return an error if no nfsiods are available.
 * This is mainly to avoid queueing async I/O requests when the nfsiods
 * are all hung on a dead server.
 */

int
nfs_asyncio(struct buf *bp)
{
	struct nfs_iod *iod;
	struct nfsmount *nmp;
	int slptimeo = 0, error;
	bool catch_p = false;

	if (nfs_numasync == 0)
		return (EIO);

	nmp = VFSTONFS(bp->b_vp->v_mount);

	if (nmp->nm_flag & NFSMNT_SOFT)
		slptimeo = nmp->nm_retry * nmp->nm_timeo;

	if (nmp->nm_iflag & NFSMNT_DISMNTFORCE)
		slptimeo = hz;

again:
	if (nmp->nm_flag & NFSMNT_INT)
		catch_p = true;

	/*
	 * Find a free iod to process this request.
	 */

	mutex_enter(&nfs_iodlist_lock);
	iod = LIST_FIRST(&nfs_iodlist_idle);
	if (iod) {
		/*
		 * Found one, so wake it up and tell it which
		 * mount to process.
		 */
		LIST_REMOVE(iod, nid_idle);
		mutex_enter(&iod->nid_lock);
		mutex_exit(&nfs_iodlist_lock);
		KASSERT(iod->nid_mount == NULL);
		iod->nid_mount = nmp;
		cv_signal(&iod->nid_cv);
		mutex_enter(&nmp->nm_lock);
		mutex_exit(&iod->nid_lock);
		nmp->nm_bufqiods++;
		if (nmp->nm_bufqlen < 2 * nmp->nm_bufqiods) {
			cv_broadcast(&nmp->nm_aiocv);
		}
	} else {
		mutex_exit(&nfs_iodlist_lock);
		mutex_enter(&nmp->nm_lock);
	}

	KASSERT(mutex_owned(&nmp->nm_lock));

	/*
	 * If we have an iod which can process the request, then queue
	 * the buffer.  However, even if we have an iod, do not initiate
	 * queue cleaning if curproc is the pageout daemon. if the NFS mount
	 * is via local loopback, we may put curproc (pagedaemon) to sleep
	 * waiting for the writes to complete. But the server (ourself)
	 * may block the write, waiting for its (ie., our) pagedaemon
	 * to produce clean pages to handle the write: deadlock.
	 * XXX: start non-loopback mounts straight away?  If "lots free",
	 * let pagedaemon start loopback writes anyway?
	 */
	if (nmp->nm_bufqiods > 0) {

		/*
		 * Ensure that the queue never grows too large.
		 */
		if (curlwp == uvm.pagedaemon_lwp) {
	  		/* Enque for later, to avoid free-page deadlock */
		} else while (nmp->nm_bufqlen >= 2 * nmp->nm_bufqiods) {
			if (catch_p) {
				error = cv_timedwait_sig(&nmp->nm_aiocv,
				    &nmp->nm_lock, slptimeo);
			} else {
				error = cv_timedwait(&nmp->nm_aiocv,
				    &nmp->nm_lock, slptimeo);
			}
			if (error) {
				if (error == EWOULDBLOCK &&
				    nmp->nm_flag & NFSMNT_SOFT) {
					mutex_exit(&nmp->nm_lock);
					bp->b_error = EIO;
					return (EIO);
				}

				if (nfs_sigintr(nmp, NULL, curlwp)) {
					mutex_exit(&nmp->nm_lock);
					return (EINTR);
				}
				if (catch_p) {
					catch_p = false;
					slptimeo = 2 * hz;
				}
			}

			/*
			 * We might have lost our iod while sleeping,
			 * so check and loop if necessary.
			 */

			if (nmp->nm_bufqiods == 0) {
				mutex_exit(&nmp->nm_lock);
				goto again;
			}
		}
		TAILQ_INSERT_TAIL(&nmp->nm_bufq, bp, b_freelist);
		nmp->nm_bufqlen++;
		mutex_exit(&nmp->nm_lock);
		return (0);
	}
	mutex_exit(&nmp->nm_lock);

	/*
	 * All the iods are busy on other mounts, so return EIO to
	 * force the caller to process the i/o synchronously.
	 */

	return (EIO);
}

/*
 * nfs_doio for read.
 */
static int
nfs_doio_read(struct buf *bp, struct uio *uiop)
{
	struct vnode *vp = bp->b_vp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0;

	uiop->uio_rw = UIO_READ;
	switch (vp->v_type) {
	case VREG:
		nfsstats.read_bios++;
		error = nfs_readrpc(vp, uiop);
		if (!error && uiop->uio_resid) {
			int diff, len;

			/*
			 * If uio_resid > 0, there is a hole in the file and
			 * no writes after the hole have been pushed to
			 * the server yet or the file has been truncated
			 * on the server.
			 * Just zero fill the rest of the valid area.
			 */

			KASSERT(vp->v_size >=
			    uiop->uio_offset + uiop->uio_resid);
			diff = bp->b_bcount - uiop->uio_resid;
			len = uiop->uio_resid;
			memset((char *)bp->b_data + diff, 0, len);
			uiop->uio_resid = 0;
		}
#if 0
		if (uiop->uio_lwp && (vp->v_iflag & VI_TEXT) &&
		    timespeccmp(&np->n_mtime, &np->n_vattr->va_mtime, !=)) {
		    	mutex_enter(proc_lock);
			killproc(uiop->uio_lwp->l_proc, "process text file was modified");
		    	mutex_exit(proc_lock);
#if 0 /* XXX NJWLWP */
			uiop->uio_lwp->l_proc->p_holdcnt++;
#endif
		}
#endif
		break;
	case VLNK:
		KASSERT(uiop->uio_offset == (off_t)0);
		nfsstats.readlink_bios++;
		error = nfs_readlinkrpc(vp, uiop, np->n_rcred);
		break;
	case VDIR:
		nfsstats.readdir_bios++;
		uiop->uio_offset = bp->b_dcookie;
#ifndef NFS_V2_ONLY
		if (nmp->nm_flag & NFSMNT_RDIRPLUS) {
			error = nfs_readdirplusrpc(vp, uiop,
			    curlwp->l_cred);
			/*
			 * nfs_request maps NFSERR_NOTSUPP to ENOTSUP.
			 */
			if (error == ENOTSUP)
				nmp->nm_flag &= ~NFSMNT_RDIRPLUS;
		}
#else
		nmp->nm_flag &= ~NFSMNT_RDIRPLUS;
#endif
		if ((nmp->nm_flag & NFSMNT_RDIRPLUS) == 0)
			error = nfs_readdirrpc(vp, uiop,
			    curlwp->l_cred);
		if (!error) {
			bp->b_dcookie = uiop->uio_offset;
		}
		break;
	default:
		printf("nfs_doio:  type %x unexpected\n", vp->v_type);
		break;
	}
	bp->b_error = error;
	return error;
}

/*
 * nfs_doio for write.
 */
static int
nfs_doio_write(struct buf *bp, struct uio *uiop)
{
	struct vnode *vp = bp->b_vp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int iomode;
	bool stalewriteverf = false;
	int i, npages = (bp->b_bcount + PAGE_SIZE - 1) >> PAGE_SHIFT;
	struct vm_page **pgs, *spgs[UBC_MAX_PAGES];
#ifndef NFS_V2_ONLY
	bool needcommit = true; /* need only COMMIT RPC */
#else
	bool needcommit = false; /* need only COMMIT RPC */
#endif
	bool pageprotected;
	struct uvm_object *uobj = &vp->v_uobj;
	int error;
	off_t off, cnt;

	if (npages < __arraycount(spgs))
		pgs = spgs;
	else {
		if ((pgs = kmem_alloc(sizeof(*pgs) * npages, KM_NOSLEEP)) ==
		    NULL)
			return ENOMEM;
	}

	if ((bp->b_flags & B_ASYNC) != 0 && NFS_ISV3(vp)) {
		iomode = NFSV3WRITE_UNSTABLE;
	} else {
		iomode = NFSV3WRITE_FILESYNC;
	}

#ifndef NFS_V2_ONLY
again:
#endif
	rw_enter(&nmp->nm_writeverflock, RW_READER);

	for (i = 0; i < npages; i++) {
		pgs[i] = uvm_pageratop((vaddr_t)bp->b_data + (i << PAGE_SHIFT));
		if (pgs[i]->uobject == uobj &&
		    pgs[i]->offset == uiop->uio_offset + (i << PAGE_SHIFT)) {
			KASSERT(pgs[i]->flags & PG_BUSY);
			/*
			 * this page belongs to our object.
			 */
			mutex_enter(uobj->vmobjlock);
			/*
			 * write out the page stably if it's about to
			 * be released because we can't resend it
			 * on the server crash.
			 *
			 * XXX assuming PG_RELEASE|PG_PAGEOUT won't be
			 * changed until unbusy the page.
			 */
			if (pgs[i]->flags & (PG_RELEASED|PG_PAGEOUT))
				iomode = NFSV3WRITE_FILESYNC;
			/*
			 * if we met a page which hasn't been sent yet,
			 * we need do WRITE RPC.
			 */
			if ((pgs[i]->flags & PG_NEEDCOMMIT) == 0)
				needcommit = false;
			mutex_exit(uobj->vmobjlock);
		} else {
			iomode = NFSV3WRITE_FILESYNC;
			needcommit = false;
		}
	}
	if (!needcommit && iomode == NFSV3WRITE_UNSTABLE) {
		mutex_enter(uobj->vmobjlock);
		for (i = 0; i < npages; i++) {
			pgs[i]->flags |= PG_NEEDCOMMIT | PG_RDONLY;
			pmap_page_protect(pgs[i], VM_PROT_READ);
		}
		mutex_exit(uobj->vmobjlock);
		pageprotected = true; /* pages can't be modified during i/o. */
	} else
		pageprotected = false;

	/*
	 * Send the data to the server if necessary,
	 * otherwise just send a commit rpc.
	 */
#ifndef NFS_V2_ONLY
	if (needcommit) {

		/*
		 * If the buffer is in the range that we already committed,
		 * there's nothing to do.
		 *
		 * If it's in the range that we need to commit, push the
		 * whole range at once, otherwise only push the buffer.
		 * In both these cases, acquire the commit lock to avoid
		 * other processes modifying the range.
		 */

		off = uiop->uio_offset;
		cnt = bp->b_bcount;
		mutex_enter(&np->n_commitlock);
		if (!nfs_in_committed_range(vp, off, bp->b_bcount)) {
			bool pushedrange;
			if (nfs_in_tobecommitted_range(vp, off, bp->b_bcount)) {
				pushedrange = true;
				off = np->n_pushlo;
				cnt = np->n_pushhi - np->n_pushlo;
			} else {
				pushedrange = false;
			}
			error = nfs_commit(vp, off, cnt, curlwp);
			if (error == 0) {
				if (pushedrange) {
					nfs_merge_commit_ranges(vp);
				} else {
					nfs_add_committed_range(vp, off, cnt);
				}
			}
		} else {
			error = 0;
		}
		mutex_exit(&np->n_commitlock);
		rw_exit(&nmp->nm_writeverflock);
		if (!error) {
			/*
			 * pages are now on stable storage.
			 */
			uiop->uio_resid = 0;
			mutex_enter(uobj->vmobjlock);
			for (i = 0; i < npages; i++) {
				pgs[i]->flags &= ~(PG_NEEDCOMMIT | PG_RDONLY);
			}
			mutex_exit(uobj->vmobjlock);
			goto out;
		} else if (error == NFSERR_STALEWRITEVERF) {
			nfs_clearcommit(vp->v_mount);
			goto again;
		}
		if (error) {
			bp->b_error = np->n_error = error;
			np->n_flag |= NWRITEERR;
		}
		goto out;
	}
#endif
	off = uiop->uio_offset;
	cnt = bp->b_bcount;
	uiop->uio_rw = UIO_WRITE;
	nfsstats.write_bios++;
	error = nfs_writerpc(vp, uiop, &iomode, pageprotected, &stalewriteverf);
#ifndef NFS_V2_ONLY
	if (!error && iomode == NFSV3WRITE_UNSTABLE) {
		/*
		 * we need to commit pages later.
		 */
		mutex_enter(&np->n_commitlock);
		nfs_add_tobecommitted_range(vp, off, cnt);
		/*
		 * if there can be too many uncommitted pages, commit them now.
		 */
		if (np->n_pushhi - np->n_pushlo > nfs_commitsize) {
			off = np->n_pushlo;
			cnt = nfs_commitsize >> 1;
			error = nfs_commit(vp, off, cnt, curlwp);
			if (!error) {
				nfs_add_committed_range(vp, off, cnt);
				nfs_del_tobecommitted_range(vp, off, cnt);
			}
			if (error == NFSERR_STALEWRITEVERF) {
				stalewriteverf = true;
				error = 0; /* it isn't a real error */
			}
		} else {
			/*
			 * re-dirty pages so that they will be passed
			 * to us later again.
			 */
			mutex_enter(uobj->vmobjlock);
			for (i = 0; i < npages; i++) {
				pgs[i]->flags &= ~PG_CLEAN;
			}
			mutex_exit(uobj->vmobjlock);
		}
		mutex_exit(&np->n_commitlock);
	} else
#endif
	if (!error) {
		/*
		 * pages are now on stable storage.
		 */
		mutex_enter(&np->n_commitlock);
		nfs_del_committed_range(vp, off, cnt);
		mutex_exit(&np->n_commitlock);
		mutex_enter(uobj->vmobjlock);
		for (i = 0; i < npages; i++) {
			pgs[i]->flags &= ~(PG_NEEDCOMMIT | PG_RDONLY);
		}
		mutex_exit(uobj->vmobjlock);
	} else {
		/*
		 * we got an error.
		 */
		bp->b_error = np->n_error = error;
		np->n_flag |= NWRITEERR;
	}

	rw_exit(&nmp->nm_writeverflock);


	if (stalewriteverf) {
		nfs_clearcommit(vp->v_mount);
	}
#ifndef NFS_V2_ONLY
out:
#endif
	if (pgs != spgs)
		kmem_free(pgs, sizeof(*pgs) * npages);
	return error;
}

/*
 * nfs_doio for B_PHYS.
 */
static int
nfs_doio_phys(struct buf *bp, struct uio *uiop)
{
	struct vnode *vp = bp->b_vp;
	int error;

	uiop->uio_offset = ((off_t)bp->b_blkno) << DEV_BSHIFT;
	if (bp->b_flags & B_READ) {
		uiop->uio_rw = UIO_READ;
		nfsstats.read_physios++;
		error = nfs_readrpc(vp, uiop);
	} else {
		int iomode = NFSV3WRITE_DATASYNC;
		bool stalewriteverf;
		struct nfsmount *nmp = VFSTONFS(vp->v_mount);

		uiop->uio_rw = UIO_WRITE;
		nfsstats.write_physios++;
		rw_enter(&nmp->nm_writeverflock, RW_READER);
		error = nfs_writerpc(vp, uiop, &iomode, false, &stalewriteverf);
		rw_exit(&nmp->nm_writeverflock);
		if (stalewriteverf) {
			nfs_clearcommit(bp->b_vp->v_mount);
		}
	}
	bp->b_error = error;
	return error;
}

/*
 * Do an I/O operation to/from a cache block. This may be called
 * synchronously or from an nfsiod.
 */
int
nfs_doio(struct buf *bp)
{
	int error;
	struct uio uio;
	struct uio *uiop = &uio;
	struct iovec io;
	UVMHIST_FUNC("nfs_doio"); UVMHIST_CALLED(ubchist);

	uiop->uio_iov = &io;
	uiop->uio_iovcnt = 1;
	uiop->uio_offset = (((off_t)bp->b_blkno) << DEV_BSHIFT);
	UIO_SETUP_SYSSPACE(uiop);
	io.iov_base = bp->b_data;
	io.iov_len = uiop->uio_resid = bp->b_bcount;

	/*
	 * Historically, paging was done with physio, but no more...
	 */
	if (bp->b_flags & B_PHYS) {
		/*
		 * ...though reading /dev/drum still gets us here.
		 */
		error = nfs_doio_phys(bp, uiop);
	} else if (bp->b_flags & B_READ) {
		error = nfs_doio_read(bp, uiop);
	} else {
		error = nfs_doio_write(bp, uiop);
	}
	bp->b_resid = uiop->uio_resid;
	biodone(bp);
	return (error);
}

/*
 * Vnode op for VM getpages.
 */

int
nfs_getpages(void *v)
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;

	struct vnode *vp = ap->a_vp;
	struct uvm_object *uobj = &vp->v_uobj;
	struct nfsnode *np = VTONFS(vp);
	const int npages = *ap->a_count;
	struct vm_page *pg, **pgs, **opgs, *spgs[UBC_MAX_PAGES];
	off_t origoffset, len;
	int i, error;
	bool v3 = NFS_ISV3(vp);
	bool write = (ap->a_access_type & VM_PROT_WRITE) != 0;
	bool locked = (ap->a_flags & PGO_LOCKED) != 0;

	/*
	 * If we are not locked we are not really using opgs,
	 * so just initialize it
	 */
	if (!locked || npages < __arraycount(spgs))
		opgs = spgs;
	else {
		if ((opgs = kmem_alloc(npages * sizeof(*opgs), KM_NOSLEEP)) ==
		    NULL)
			return ENOMEM;
	}

	/*
	 * call the genfs code to get the pages.  `pgs' may be NULL
	 * when doing read-ahead.
	 */
	pgs = ap->a_m;
	if (write && locked && v3) {
		KASSERT(pgs != NULL);
#ifdef DEBUG

		/*
		 * If PGO_LOCKED is set, real pages shouldn't exists
		 * in the array.
		 */

		for (i = 0; i < npages; i++)
			KDASSERT(pgs[i] == NULL || pgs[i] == PGO_DONTCARE);
#endif
		memcpy(opgs, pgs, npages * sizeof(struct vm_pages *));
	}
	error = genfs_getpages(v);
	if (error)
		goto out;

	/*
	 * for read faults where the nfs node is not yet marked NMODIFIED,
	 * set PG_RDONLY on the pages so that we come back here if someone
	 * tries to modify later via the mapping that will be entered for
	 * this fault.
	 */

	if (!write && (np->n_flag & NMODIFIED) == 0 && pgs != NULL) {
		if (!locked) {
			mutex_enter(uobj->vmobjlock);
		}
		for (i = 0; i < npages; i++) {
			pg = pgs[i];
			if (pg == NULL || pg == PGO_DONTCARE) {
				continue;
			}
			pg->flags |= PG_RDONLY;
		}
		if (!locked) {
			mutex_exit(uobj->vmobjlock);
		}
	}
	if (!write)
		goto out;

	/*
	 * this is a write fault, update the commit info.
	 */

	origoffset = ap->a_offset;
	len = npages << PAGE_SHIFT;

	if (v3) {
		if (!locked) {
			mutex_enter(&np->n_commitlock);
		} else {
			if (!mutex_tryenter(&np->n_commitlock)) {

				/*
				 * Since PGO_LOCKED is set, we need to unbusy
				 * all pages fetched by genfs_getpages() above,
				 * tell the caller that there are no pages
				 * available and put back original pgs array.
				 */

				mutex_enter(&uvm_pageqlock);
				uvm_page_unbusy(pgs, npages);
				mutex_exit(&uvm_pageqlock);
				*ap->a_count = 0;
				memcpy(pgs, opgs,
				    npages * sizeof(struct vm_pages *));
				error = EBUSY;
				goto out;
			}
		}
		nfs_del_committed_range(vp, origoffset, len);
		nfs_del_tobecommitted_range(vp, origoffset, len);
	}
	np->n_flag |= NMODIFIED;
	if (!locked) {
		mutex_enter(uobj->vmobjlock);
	}
	for (i = 0; i < npages; i++) {
		pg = pgs[i];
		if (pg == NULL || pg == PGO_DONTCARE) {
			continue;
		}
		pg->flags &= ~(PG_NEEDCOMMIT | PG_RDONLY);
	}
	if (!locked) {
		mutex_exit(uobj->vmobjlock);
	}
	if (v3) {
		mutex_exit(&np->n_commitlock);
	}
out:
	if (opgs != spgs)
		kmem_free(opgs, sizeof(*opgs) * npages);
	return error;
}
