/*	$NetBSD: smbfs_smb.c,v 1.47 2014/12/21 10:48:53 hannken Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
 * FreeBSD: src/sys/fs/smbfs/smbfs_smb.c,v 1.3 2001/12/10 08:09:46 obrien Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smbfs_smb.c,v 1.47 2014/12/21 10:48:53 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/mbuf.h>
#include <sys/mount.h>

#ifdef USE_MD5_HASH
#include <sys/md5.h>
#endif

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_conn.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

/*
 * Lack of inode numbers leads us to the problem of generating them.
 * Partially this problem can be solved by having a dir/file cache
 * with inode numbers generated from the incremented by one counter.
 * However this way will require too much kernel memory, gives all
 * sorts of locking and consistency problems, not to mentinon counter overflows.
 * So, I'm decided to use a hash function to generate pseudo random (and unique)
 * inode numbers.
 */
static long
smbfs_getino(struct smbnode *dnp, const char *name, int nmlen)
{
#ifdef USE_MD5_HASH
	MD5_CTX md5;
	u_int32_t state[4];
	long ino;
	int i;

	MD5Init(&md5);
	MD5Update(&md5, name, nmlen);
	MD5Final((u_char *)state, &md5);
	for (i = 0, ino = 0; i < 4; i++)
		ino += state[i];
	return dnp->n_ino + ino;
#endif
	u_int32_t ino;

	ino = dnp->n_ino + hash32_strn(name, nmlen, HASH32_STR_INIT);
	if (ino <= 2)
		ino += 3;
	return ino;
}

static int
smbfs_smb_lockandx(struct smbnode *np, int op, void *id, off_t start, off_t end,
	struct smb_cred *scred)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	u_char ltype = 0;
	int error;

	if (op == SMB_LOCK_SHARED)
		ltype |= SMB_LOCKING_ANDX_SHARED_LOCK;
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_LOCKING_ANDX, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);
	mb_put_mem(mbp, (void *)&np->n_fid, 2, MB_MSYSTEM);
	mb_put_uint8(mbp, ltype);	/* locktype */
	mb_put_uint8(mbp, 0);		/* oplocklevel - 0 seems is NO_OPLOCK */
	mb_put_uint32le(mbp, 0);	/* timeout - break immediately */
	mb_put_uint16le(mbp, op == SMB_LOCK_RELEASE ? 1 : 0);
	mb_put_uint16le(mbp, op == SMB_LOCK_RELEASE ? 0 : 1);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint16le(mbp, (((long) id) & 0xffff));	/* process ID */
	mb_put_uint32le(mbp, start);
	mb_put_uint32le(mbp, end - start);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	return error;
}

int
smbfs_smb_lock(struct smbnode *np, int op, void *id,
	off_t start, off_t end,	struct smb_cred *scred)
{
	struct smb_share *ssp = np->n_mount->sm_share;

	if (SMB_DIALECT(SSTOVC(ssp)) < SMB_DIALECT_LANMAN1_0)
		/*
		 * TODO: use LOCK_BYTE_RANGE here.
		 */
		return EINVAL;
	else
		return smbfs_smb_lockandx(np, op, id, start, end, scred);
}

int
smbfs_smb_statvfs(struct smb_share *ssp, struct statvfs *sbp,
	struct smb_cred *scred)
{
	unsigned long bsize;	/* Block (allocation unit) size */
	unsigned long bavail, bfree;

	/*
	 * The SMB request work with notion of sector size and
	 * allocation units. Allocation unit is what 'block'
	 * means in Unix context, sector size might be HW sector size.
	 */

	if (SMB_DIALECT(SSTOVC(ssp)) >= SMB_DIALECT_LANMAN2_0) {
		struct smb_t2rq *t2p;
		struct mbchain *mbp;
		struct mdchain *mdp;
		u_int16_t secsz;
		u_int32_t units, bpu, funits;
		int error;

		error = smb_t2_alloc(SSTOCP(ssp),
		    SMB_TRANS2_QUERY_FS_INFORMATION, scred, &t2p);
		if (error)
			return error;
		mbp = &t2p->t2_tparam;
		mb_init(mbp);
		mb_put_uint16le(mbp, SMB_INFO_ALLOCATION);
		t2p->t2_maxpcount = 4;
		t2p->t2_maxdcount = 4 * 4 + 2;
		error = smb_t2_request(t2p);
		if (error) {
			smb_t2_done(t2p);
			return error;
		}
		mdp = &t2p->t2_rdata;
		md_get_uint32(mdp, NULL);	/* fs id */
		md_get_uint32le(mdp, &bpu);	/* Number of sectors per unit */
		md_get_uint32le(mdp, &units);	/* Total number of units */
		md_get_uint32le(mdp, &funits);	/* Number of available units */
		md_get_uint16le(mdp, &secsz);	/* Number of bytes per sector */
		smb_t2_done(t2p);

		bsize = bpu * secsz;
		bavail = units;
		bfree = funits;
	} else {
		struct smb_rq *rqp;
		struct mdchain *mdp;
		u_int16_t units, bpu, secsz, funits;
		int error;

		error = smb_rq_alloc(SSTOCP(ssp),
		    SMB_COM_QUERY_INFORMATION_DISK, scred, &rqp);
		if (error)
			return error;
		smb_rq_wstart(rqp);
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (error) {
			smb_rq_done(rqp);
			return error;
		}
		smb_rq_getreply(rqp, &mdp);
		md_get_uint16le(mdp, &units);	/* Total units per server */
		md_get_uint16le(mdp, &bpu);	/* Blocks per allocation unit */
		md_get_uint16le(mdp, &secsz);	/* Block size (in bytes) */
		md_get_uint16le(mdp, &funits);	/* Number of free units */
		smb_rq_done(rqp);

		bsize = bpu * secsz;
		bavail = units;
		bfree = funits;
	}

	sbp->f_bsize = bsize;		/* fundamental file system block size */
	sbp->f_frsize = bsize;		/* fundamental file system frag size */
	sbp->f_iosize = bsize;		/* optimal I/O size */
	sbp->f_blocks = bavail;		/* total data blocks in file system */
	sbp->f_bfree = bfree;		/* free blocks in fs */
	sbp->f_bresvd = 0;		/* reserved blocks in fs */
	sbp->f_bavail= bfree;		/* free blocks avail to non-superuser */
	sbp->f_files = 0xffff;		/* total file nodes in file system */
	sbp->f_ffree = 0xffff;		/* free file nodes to non-superuser */
	sbp->f_favail = 0xffff;		/* free file nodes in fs */
	sbp->f_fresvd = 0;		/* reserved file nodes in fs */
	return 0;
}

static int
smbfs_smb_seteof(struct smbnode *np, int64_t newsize, struct smb_cred *scred)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_FILE_INFORMATION,
	    scred, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (void *)&np->n_fid, 2, MB_MSYSTEM);
	mb_put_uint16le(mbp, SMB_SET_FILE_END_OF_FILE_INFO);
	mb_put_uint32le(mbp, 0);
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_int64le(mbp, newsize);
	mb_put_uint32le(mbp, 0);			/* padding */
	mb_put_uint16le(mbp, 0);
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}

int
smbfs_smb_setfsize(struct smbnode *np, u_quad_t newsize,
		   struct smb_cred *scred)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	if (newsize >= (1LL << 32)) {
		if (!(SMB_CAPS(SSTOVC(ssp)) & SMB_CAP_LARGE_FILES))
			return EFBIG;
		return smbfs_smb_seteof(np, (int64_t)newsize, scred);
	}

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_WRITE, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (void *)&np->n_fid, 2, MB_MSYSTEM);
	mb_put_uint16le(mbp, 0);
	mb_put_uint32le(mbp, newsize);
	mb_put_uint16le(mbp, 0);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_DATA);
	mb_put_uint16le(mbp, 0);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	return error;
}


/*
 * Set DOS file attributes. mtime should be NULL for dialects above lm10
 */
int
smbfs_smb_setpattr(struct smbnode *np, u_int16_t attr, struct timespec *mtime,
	struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	u_long xtime;
	int error, svtz;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_SET_INFORMATION, scred, &rqp);
	if (error)
		return error;
	svtz = SSTOVC(ssp)->vc_sopt.sv_tz;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, attr);
	if (mtime) {
		smb_time_local2server(mtime, svtz, &xtime);
	} else
		xtime = 0;
	mb_put_uint32le(mbp, xtime);		/* mtime */
	mb_put_mem(mbp, NULL, 5 * 2, MB_MZERO);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	do {
		error = smbfs_fullpath(mbp, SSTOVC(ssp), np, NULL, 0);
		if (error)
			break;
		mb_put_uint8(mbp, SMB_DT_ASCII);
		mb_put_uint8(mbp, 0);
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (error)
			break;
	} while(0);
	smb_rq_done(rqp);
	return error;
}

/*
 * Note, win95 doesn't support this call.
 */
int
smbfs_smb_setptime2(struct smbnode *np, struct timespec *mtime,
	struct timespec *atime, int attr, struct smb_cred *scred)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	u_int16_t xdate, xtime;
	int error, tzoff;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_PATH_INFORMATION,
	    scred, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_INFO_STANDARD);
	mb_put_uint32le(mbp, 0);		/* MBZ */
	error = smbfs_fullpath(mbp, vcp, np, NULL, 0);
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	tzoff = vcp->vc_sopt.sv_tz;
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_uint32le(mbp, 0);		/* creation time */
	if (atime)
		smb_time_unix2dos(atime, tzoff, &xdate, &xtime, NULL);
	else
		xtime = xdate = 0;
	mb_put_uint16le(mbp, xdate);
	mb_put_uint16le(mbp, xtime);
	if (mtime)
		smb_time_unix2dos(mtime, tzoff, &xdate, &xtime, NULL);
	else
		xtime = xdate = 0;
	mb_put_uint16le(mbp, xdate);
	mb_put_uint16le(mbp, xtime);
	mb_put_uint32le(mbp, 0);		/* file size */
	mb_put_uint32le(mbp, 0);		/* allocation unit size */
	mb_put_uint16le(mbp, attr);	/* DOS attr */
	mb_put_uint32le(mbp, 0);		/* EA size */
	t2p->t2_maxpcount = 5 * 2;
	t2p->t2_maxdcount = vcp->vc_txmax;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}

/*
 * NT level. Specially for win9x
 */
int
smbfs_smb_setpattrNT(struct smbnode *np, u_short attr, struct timespec *mtime,
	struct timespec *atime, struct smb_cred *scred)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	int64_t tm;
	int error, tzoff;

	/*
	 * SMB_SET_FILE_BASIC_INFO isn't supported for
	 * SMB_TRANS2_SET_PATH_INFORMATION,
	 * so use SMB_SET_FILE_BASIC_INFORMATION instead,
	 * but it requires SMB_CAP_INFOLEVEL_PASSTHRU capability.
	 */
	if ((SMB_CAPS(vcp) & SMB_CAP_INFOLEVEL_PASSTHRU) == 0)
		return smbfs_smb_setptime2(np, mtime, atime, attr, scred);

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_PATH_INFORMATION,
	    scred, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_SET_FILE_BASIC_INFORMATION);
	mb_put_uint32le(mbp, 0);		/* MBZ */
	error = smbfs_fullpath(mbp, vcp, np, NULL, 0);
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	tzoff = vcp->vc_sopt.sv_tz;
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_int64le(mbp, 0);		/* creation time */
	if (atime) {
		smb_time_local2NT(atime, tzoff, &tm);
	} else
		tm = 0;
	mb_put_int64le(mbp, tm);
	if (mtime) {
		smb_time_local2NT(mtime, tzoff, &tm);
	} else
		tm = 0;
	mb_put_int64le(mbp, tm);
	mb_put_int64le(mbp, tm);		/* change time */
	mb_put_uint32le(mbp, attr);		/* attr */
	mb_put_uint32le(mbp, 0);		/* padding */
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}

/*
 * Set file atime and mtime. Doesn't supported by core dialect.
 */
int
smbfs_smb_setftime(struct smbnode *np, struct timespec *mtime,
	struct timespec *atime, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	u_int16_t xdate, xtime;
	int error, tzoff;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_SET_INFORMATION2, scred, &rqp);
	if (error)
		return error;
	tzoff = SSTOVC(ssp)->vc_sopt.sv_tz;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (void *)&np->n_fid, 2, MB_MSYSTEM);
	mb_put_uint32le(mbp, 0);		/* creation time */

	if (atime)
		smb_time_unix2dos(atime, tzoff, &xdate, &xtime, NULL);
	else
		xtime = xdate = 0;
	mb_put_uint16le(mbp, xdate);
	mb_put_uint16le(mbp, xtime);
	if (mtime)
		smb_time_unix2dos(mtime, tzoff, &xdate, &xtime, NULL);
	else
		xtime = xdate = 0;
	mb_put_uint16le(mbp, xdate);
	mb_put_uint16le(mbp, xtime);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG(("%d\n", error));
	smb_rq_done(rqp);
	return error;
}

/*
 * Set DOS file attributes.
 * Looks like this call can be used only if CAP_NT_SMBS bit is on.
 */
int
smbfs_smb_setfattrNT(struct smbnode *np, u_int16_t attr, struct timespec *mtime,
	struct timespec *atime, struct smb_cred *scred)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int64_t tm;
	int error, svtz;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_FILE_INFORMATION,
	    scred, &t2p);
	if (error)
		return error;
	svtz = SSTOVC(ssp)->vc_sopt.sv_tz;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (void *)&np->n_fid, 2, MB_MSYSTEM); 	/* FID */
	mb_put_uint16le(mbp, SMB_SET_FILE_BASIC_INFO);		/* info level */
	mb_put_uint32le(mbp, 0);				/* reserved */
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_int64le(mbp, 0);		/* creation time */
	if (atime) {
		smb_time_local2NT(atime, svtz, &tm);
	} else
		tm = 0;
	mb_put_int64le(mbp, tm);
	if (mtime) {
		smb_time_local2NT(mtime, svtz, &tm);
	} else
		tm = 0;
	mb_put_int64le(mbp, tm);
	mb_put_int64le(mbp, tm);		/* change time */
	mb_put_uint32le(mbp, attr);		/* attr */
	mb_put_uint32le(mbp, 0);		/* padding */
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}


int
smbfs_smb_open(struct smbnode *np, int accmode, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	u_int16_t fid, wattr, grantedmode;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_OPEN, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, accmode);
	mb_put_uint16le(mbp, SMB_FA_SYSTEM | SMB_FA_HIDDEN);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	do {
		error = smbfs_fullpath(mbp, SSTOVC(ssp), np, NULL, 0);
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		if (md_get_uint8(mdp, &wc) != 0 || wc != 7) {
			error = EBADRPC;
			break;
		}
		md_get_uint16(mdp, &fid);
		md_get_uint16le(mdp, &wattr);
		md_get_uint32(mdp, NULL);	/* mtime */
		md_get_uint32(mdp, NULL);	/* fsize */
		md_get_uint16le(mdp, &grantedmode);
		/*
		 * TODO: refresh attributes from this reply
		 */
	} while(0);
	smb_rq_done(rqp);
	if (error)
		return error;
	np->n_fid = fid;
	np->n_rwstate = grantedmode;
	return 0;
}


int
smbfs_smb_close(struct smb_share *ssp, u_int16_t fid, struct timespec *mtime,
	struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	u_long xtime;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_CLOSE, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (void *)&fid, sizeof(fid), MB_MSYSTEM);
	if (mtime) {
		smb_time_local2server(mtime, SSTOVC(ssp)->vc_sopt.sv_tz, &xtime);
	} else
		xtime = 0;
	mb_put_uint32le(mbp, xtime);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	return error;
}

int
smbfs_smb_create(struct smbnode *dnp, const char *name, int nmlen,
	struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = dnp->n_mount->sm_share;
	struct mbchain *mbp;
	struct mdchain *mdp;
	struct timespec ctime;
	u_int8_t wc;
	u_int16_t fid;
	u_long tm;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_CREATE_NEW, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);

	/* get current time */
	getnanotime(&ctime);
	smb_time_local2server(&ctime, SSTOVC(ssp)->vc_sopt.sv_tz, &tm);

	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, SMB_FA_ARCHIVE);	/* attributes  */
	mb_put_uint32le(mbp, tm);
	smb_rq_wend(rqp);

	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, SSTOVC(ssp), dnp, name, nmlen);
	if (!error) {
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (!error) {
			smb_rq_getreply(rqp, &mdp);
			md_get_uint8(mdp, &wc);
			if (wc == 1)
				md_get_uint16(mdp, &fid);
			else
				error = EBADRPC;
		}
	}

	smb_rq_done(rqp);
	if (!error)
		smbfs_smb_close(ssp, fid, &ctime, scred);

	return (error);
}

int
smbfs_smb_delete(struct smbnode *np, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_DELETE, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, SMB_FA_SYSTEM | SMB_FA_HIDDEN);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, SSTOVC(ssp), np, NULL, 0);
	if (!error) {
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	}
	smb_rq_done(rqp);
	return error;
}

int
smbfs_smb_rename(struct smbnode *src, struct smbnode *tdnp,
	const char *tname, int tnmlen, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = src->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_RENAME, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, SMB_FA_SYSTEM | SMB_FA_HIDDEN);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	do {
		error = smbfs_fullpath(mbp, SSTOVC(ssp), src, NULL, 0);
		if (error)
			break;
		mb_put_uint8(mbp, SMB_DT_ASCII);
		error = smbfs_fullpath(mbp, SSTOVC(ssp), tdnp, tname, tnmlen);
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	} while(0);
	smb_rq_done(rqp);
	return error;
}

#ifdef notnow
int
smbfs_smb_move(struct smbnode *src, struct smbnode *tdnp,
	const char *tname, int tnmlen, u_int16_t flags, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = src->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_MOVE, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, SMB_TID_UNKNOWN);
	mb_put_uint16le(mbp, 0x20);	/* delete target file */
	mb_put_uint16le(mbp, flags);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	do {
		error = smbfs_fullpath(mbp, SSTOVC(ssp), src, NULL, 0);
		if (error)
			break;
		mb_put_uint8(mbp, SMB_DT_ASCII);
		error = smbfs_fullpath(mbp, SSTOVC(ssp), tdnp, tname, tnmlen);
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	} while(0);
	smb_rq_done(rqp);
	return error;
}
#endif

int
smbfs_smb_mkdir(struct smbnode *dnp, const char *name, int len,
	struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = dnp->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_CREATE_DIRECTORY, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, SSTOVC(ssp), dnp, name, len);
	if (!error) {
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	}
	smb_rq_done(rqp);
	return error;
}

int
smbfs_smb_rmdir(struct smbnode *np, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_DELETE_DIRECTORY, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, SSTOVC(ssp), np, NULL, 0);
	if (!error) {
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	}
	smb_rq_done(rqp);
	return error;
}

static int
smbfs_smb_search(struct smbfs_fctx *ctx)
{
	struct smb_vc *vcp = SSTOVC(ctx->f_ssp);
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc, bt;
	u_int16_t ec, dlen, bc;
	int maxent, error;

	maxent = min(ctx->f_left, (vcp->vc_txmax - SMB_HDRLEN - 3) / SMB_DENTRYLEN);
	if (ctx->f_rq) {
		smb_rq_done(ctx->f_rq);
		ctx->f_rq = NULL;
	}
	error = smb_rq_alloc(SSTOCP(ctx->f_ssp), SMB_COM_SEARCH, ctx->f_scred, &rqp);
	if (error)
		return error;
	ctx->f_rq = rqp;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, maxent);	/* max entries to return */
	mb_put_uint16le(mbp, ctx->f_attrmask);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);	/* buffer format */
	if (ctx->f_flags & SMBFS_RDD_FINDFIRST) {
		error = smbfs_fullpath(mbp, vcp, ctx->f_dnp, ctx->f_wildcard, ctx->f_wclen);
		if (error)
			return error;
		mb_put_uint8(mbp, SMB_DT_VARIABLE);
		mb_put_uint16le(mbp, 0);	/* context length */
		ctx->f_flags &= ~SMBFS_RDD_FINDFIRST;
	} else {
		mb_put_uint8(mbp, 0);	/* file name length */
		mb_put_uint8(mbp, SMB_DT_VARIABLE);
		mb_put_uint16le(mbp, SMB_SKEYLEN);
		mb_put_mem(mbp, ctx->f_skey, SMB_SKEYLEN, MB_MSYSTEM);
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	if (error) {
		if (error == ENOENT)
			ctx->f_flags |= SMBFS_RDD_EOF;

		return error;
	}
	smb_rq_getreply(rqp, &mdp);
	md_get_uint8(mdp, &wc);
	if (wc != 1)
		return EBADRPC;
	md_get_uint16le(mdp, &ec);
	if (ec == 0)
		return ENOENT;
	ctx->f_ecnt = ec;
	md_get_uint16le(mdp, &bc);
	if (bc < 3)
		return EBADRPC;
	bc -= 3;
	md_get_uint8(mdp, &bt);
	if (bt != SMB_DT_VARIABLE)
		return EBADRPC;
	md_get_uint16le(mdp, &dlen);
	if (dlen != bc || dlen % SMB_DENTRYLEN != 0)
		return EBADRPC;
	return 0;
}

static int
smbfs_findopenLM1(struct smbfs_fctx *ctx, struct smbnode *dnp,
    const char *wildcard, int wclen, int attr, struct smb_cred *scred)
{
	ctx->f_attrmask = attr;
	if (wildcard) {
		if (wclen == 1 && wildcard[0] == '*') {
			ctx->f_wildcard = "*.*";
			ctx->f_wclen = 3;
		} else {
			ctx->f_wildcard = wildcard;
			ctx->f_wclen = wclen;
		}
	} else {
		ctx->f_wildcard = NULL;
		ctx->f_wclen = 0;
	}
	ctx->f_name = ctx->f_fname;
	return 0;
}

static int
smbfs_findnextLM1(struct smbfs_fctx *ctx, int limit)
{
	struct mdchain *mbp;
	struct smb_rq *rqp;
	char *cp;
	u_int8_t battr;
	u_int16_t xdate, xtime;
	u_int32_t size;
	int error;

	if (ctx->f_ecnt == 0) {
		if (ctx->f_flags & SMBFS_RDD_EOF)
			return ENOENT;
		ctx->f_left = ctx->f_limit = limit;
		error = smbfs_smb_search(ctx);
		if (error)
			return error;
	}
	rqp = ctx->f_rq;
	smb_rq_getreply(rqp, &mbp);
	md_get_mem(mbp, ctx->f_skey, SMB_SKEYLEN, MB_MSYSTEM);
	md_get_uint8(mbp, &battr);
	md_get_uint16le(mbp, &xtime);
	md_get_uint16le(mbp, &xdate);
	md_get_uint32le(mbp, &size);
	KASSERT(ctx->f_name == ctx->f_fname);
	cp = ctx->f_name;
	md_get_mem(mbp, cp, sizeof(ctx->f_fname), MB_MSYSTEM);
	cp[sizeof(ctx->f_fname) - 1] = '\0';
	cp += strlen(cp) - 1;
	while(*cp == ' ' && cp > ctx->f_name)
		*cp-- = '\0';
	ctx->f_attr.fa_attr = battr;
	smb_dos2unixtime(xdate, xtime, 0, rqp->sr_vc->vc_sopt.sv_tz,
	    &ctx->f_attr.fa_mtime);
	ctx->f_attr.fa_size = size;
	ctx->f_nmlen = strlen(ctx->f_name);
	ctx->f_ecnt--;
	ctx->f_left--;
	return 0;
}

static int
smbfs_findcloseLM1(struct smbfs_fctx *ctx)
{
	if (ctx->f_rq)
		smb_rq_done(ctx->f_rq);
	return 0;
}

/*
 * TRANS2_FIND_FIRST2/NEXT2, used for NT LM12 dialect
 */
static int
smbfs_smb_trans2find2(struct smbfs_fctx *ctx)
{
	struct smb_t2rq *t2p;
	struct smb_vc *vcp = SSTOVC(ctx->f_ssp);
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t tw, flags;
	int error;

	if (ctx->f_t2) {
		smb_t2_done(ctx->f_t2);
		ctx->f_t2 = NULL;
	}
	ctx->f_flags &= ~SMBFS_RDD_GOTRNAME;
	flags = 8 | 2;			/* <resume> | <close if EOS> */
	if (ctx->f_flags & SMBFS_RDD_FINDSINGLE) {
		flags |= 1;		/* close search after this request */
		ctx->f_flags |= SMBFS_RDD_NOCLOSE;
	}
	if (ctx->f_flags & SMBFS_RDD_FINDFIRST) {
		error = smb_t2_alloc(SSTOCP(ctx->f_ssp), SMB_TRANS2_FIND_FIRST2,
		    ctx->f_scred, &t2p);
		if (error)
			return error;
		ctx->f_t2 = t2p;
		mbp = &t2p->t2_tparam;
		mb_init(mbp);
		mb_put_uint16le(mbp, ctx->f_attrmask);
		mb_put_uint16le(mbp, ctx->f_limit);
		mb_put_uint16le(mbp, flags);
		mb_put_uint16le(mbp, ctx->f_infolevel);
		mb_put_uint32le(mbp, 0);
		error = smbfs_fullpath(mbp, vcp, ctx->f_dnp, ctx->f_wildcard, ctx->f_wclen);
		if (error)
			return error;
	} else	{
		error = smb_t2_alloc(SSTOCP(ctx->f_ssp), SMB_TRANS2_FIND_NEXT2,
		    ctx->f_scred, &t2p);
		if (error)
			return error;
		ctx->f_t2 = t2p;
		mbp = &t2p->t2_tparam;
		mb_init(mbp);
		mb_put_mem(mbp, (void *)&ctx->f_Sid, 2, MB_MSYSTEM);
		mb_put_uint16le(mbp, ctx->f_limit);
		mb_put_uint16le(mbp, ctx->f_infolevel);
		mb_put_uint32le(mbp, 0);		/* resume key */
		mb_put_uint16le(mbp, flags);
		if (ctx->f_rname)
			mb_put_mem(mbp, ctx->f_rname, strlen(ctx->f_rname) + 1, MB_MSYSTEM);
		else
			mb_put_uint8(mbp, 0);	/* resume file name */
#if 0
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 200 * 1000;	/* 200ms */
		if (vcp->vc_flags & SMBC_WIN95) {
			/*
			 * some implementations suggests to sleep here
			 * for 200ms, due to the bug in the Win95.
			 * I've didn't notice any problem, but put code
			 * for it.
			 */
			 tsleep(&flags, PVFS, "fix95", tvtohz(&tv));
		}
#endif
	}
	t2p->t2_maxpcount = 5 * 2;
	t2p->t2_maxdcount = vcp->vc_txmax;
	error = smb_t2_request(t2p);
	if (error)
		return error;
	mdp = &t2p->t2_rparam;
	if (ctx->f_flags & SMBFS_RDD_FINDFIRST) {
		if ((error = md_get_uint16(mdp, &ctx->f_Sid)) != 0)
			return error;
		ctx->f_flags &= ~SMBFS_RDD_FINDFIRST;
	}
	if ((error = md_get_uint16le(mdp, &tw)) != 0)
		return error;
	ctx->f_ecnt = tw;
	if ((error = md_get_uint16le(mdp, &tw)) != 0)
		return error;
	if (tw)
		ctx->f_flags |= SMBFS_RDD_EOF | SMBFS_RDD_NOCLOSE;
	if ((error = md_get_uint16le(mdp, &tw)) != 0)
		return error;
	if ((error = md_get_uint16le(mdp, &tw)) != 0)
		return error;
	if (ctx->f_ecnt == 0) {
		ctx->f_flags |= SMBFS_RDD_EOF | SMBFS_RDD_NOCLOSE;
		return ENOENT;
	}
	ctx->f_rnameofs = tw;
	mdp = &t2p->t2_rdata;

	KASSERT(mdp->md_top != NULL);
	KASSERT(mdp->md_top->m_len != 0);

	ctx->f_eofs = 0;
	return 0;
}

static int
smbfs_smb_findclose2(struct smbfs_fctx *ctx)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(ctx->f_ssp), SMB_COM_FIND_CLOSE2, ctx->f_scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (void *)&ctx->f_Sid, 2, MB_MSYSTEM);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	return error;
}

static int
smbfs_findopenLM2(struct smbfs_fctx *ctx, struct smbnode *dnp,
    const char *wildcard, int wclen, int attr, struct smb_cred *scred)
{
	ctx->f_name = malloc(SMB_MAXNAMLEN * 2, M_SMBFSDATA, M_WAITOK);
	if (ctx->f_name == NULL)
		return ENOMEM;
	ctx->f_infolevel = SMB_DIALECT(SSTOVC(ctx->f_ssp)) < SMB_DIALECT_NTLM0_12 ?
	    SMB_INFO_STANDARD : SMB_FIND_FILE_DIRECTORY_INFO;
	ctx->f_attrmask = attr;
	ctx->f_wildcard = wildcard;
	ctx->f_wclen = wclen;
	return 0;
}

static int
smbfs_findnextLM2(struct smbfs_fctx *ctx, int limit)
{
	struct mdchain *mbp;
	struct smb_t2rq *t2p;
	char *cp;
	u_int8_t tb;
	u_int16_t xdate, xtime, wattr;
	u_int32_t size, next, dattr;
	int64_t tmp;
	int error, svtz, cnt, fxsz, nmlen, recsz;

	if (ctx->f_ecnt == 0) {
		if (ctx->f_flags & SMBFS_RDD_EOF)
			return ENOENT;
		ctx->f_left = ctx->f_limit = limit;
		error = smbfs_smb_trans2find2(ctx);
		if (error)
			return error;
	}
	t2p = ctx->f_t2;
	mbp = &t2p->t2_rdata;
	svtz = SSTOVC(ctx->f_ssp)->vc_sopt.sv_tz;
	switch (ctx->f_infolevel) {
	case SMB_INFO_STANDARD:
		next = 0;
		fxsz = 0;
		md_get_uint16le(mbp, &xdate);
		md_get_uint16le(mbp, &xtime);	/* creation time */
		md_get_uint16le(mbp, &xdate);
		md_get_uint16le(mbp, &xtime);	/* access time */
		smb_dos2unixtime(xdate, xtime, 0, svtz, &ctx->f_attr.fa_atime);
		md_get_uint16le(mbp, &xdate);
		md_get_uint16le(mbp, &xtime);	/* access time */
		smb_dos2unixtime(xdate, xtime, 0, svtz, &ctx->f_attr.fa_mtime);
		md_get_uint32le(mbp, &size);
		ctx->f_attr.fa_size = size;
		md_get_uint32(mbp, NULL);	/* allocation size */
		md_get_uint16le(mbp, &wattr);
		ctx->f_attr.fa_attr = wattr;
		md_get_uint8(mbp, &tb);
		size = nmlen = tb;
		fxsz = 23;
		recsz = next = 24 + nmlen;	/* docs misses zero byte at end */
		break;
	case SMB_FIND_FILE_DIRECTORY_INFO:
		md_get_uint32le(mbp, &next);
		md_get_uint32(mbp, NULL);	/* file index */
		md_get_int64(mbp, NULL);	/* creation time */
		md_get_int64le(mbp, &tmp);
		smb_time_NT2local(tmp, svtz, &ctx->f_attr.fa_atime);
		md_get_int64le(mbp, &tmp);
		smb_time_NT2local(tmp, svtz, &ctx->f_attr.fa_mtime);
		md_get_int64le(mbp, &tmp);
		smb_time_NT2local(tmp, svtz, &ctx->f_attr.fa_ctime);
		md_get_int64le(mbp, &tmp);	/* file size */
		ctx->f_attr.fa_size = tmp;
		md_get_int64(mbp, NULL);	/* real size (should use) */
		md_get_uint32le(mbp, &dattr);	/* EA */
		ctx->f_attr.fa_attr = dattr;
		md_get_uint32le(mbp, &size);	/* name len */
		fxsz = 64;
		recsz = next ? next : fxsz + size;
		break;
	default:
#ifdef DIAGNOSTIC
		panic("smbfs_findnextLM2: unexpected info level %d\n",
		    ctx->f_infolevel);
#else
		return EINVAL;
#endif
	}
	nmlen = min(size, SMB_MAXNAMLEN * 2);
	cp = ctx->f_name;
	error = md_get_mem(mbp, cp, nmlen, MB_MSYSTEM);
	if (error)
		return error;
	if (next) {
		cnt = next - nmlen - fxsz;
		if (cnt > 0)
			md_get_mem(mbp, NULL, cnt, MB_MSYSTEM);
#ifdef DIAGNOSTIC
		else if (cnt < 0)
			panic("smbfs_findnextLM2: out of sync");
#endif
	}
	if (nmlen && cp[nmlen - 1] == 0)
		nmlen--;
	if (nmlen == 0)
		return EBADRPC;

	next = ctx->f_eofs + recsz;
	if (ctx->f_rnameofs && (ctx->f_flags & SMBFS_RDD_GOTRNAME) == 0 &&
	    (ctx->f_rnameofs >= ctx->f_eofs && ctx->f_rnameofs < next)) {
		/*
		 * Server needs a resume filename.
		 */
		if (ctx->f_rnamelen <= nmlen) {
			if (ctx->f_rname)
				free(ctx->f_rname, M_SMBFSDATA);
			ctx->f_rname = malloc(nmlen + 1, M_SMBFSDATA, M_WAITOK);
			ctx->f_rnamelen = nmlen;
		}
		memcpy(ctx->f_rname, ctx->f_name, nmlen);
		ctx->f_rname[nmlen] = 0;
		ctx->f_flags |= SMBFS_RDD_GOTRNAME;
	}
	ctx->f_nmlen = nmlen;
	ctx->f_eofs = next;
	ctx->f_ecnt--;
	ctx->f_left--;
	return 0;
}

static int
smbfs_findcloseLM2(struct smbfs_fctx *ctx)
{
	if (ctx->f_name)
		free(ctx->f_name, M_SMBFSDATA);
	if (ctx->f_t2)
		smb_t2_done(ctx->f_t2);
	if ((ctx->f_flags & SMBFS_RDD_NOCLOSE) == 0)
		smbfs_smb_findclose2(ctx);
	return 0;
}

int
smbfs_findopen(struct smbnode *dnp, const char *wildcard, int wclen, int attr,
	struct smb_cred *scred, struct smbfs_fctx **ctxpp)
{
	struct smbfs_fctx *ctx;
	int error;

	ctx = malloc(sizeof(*ctx), M_SMBFSDATA, M_WAITOK|M_ZERO);
	ctx->f_ssp = dnp->n_mount->sm_share;
	ctx->f_dnp = dnp;
	ctx->f_flags = SMBFS_RDD_FINDFIRST;
	ctx->f_scred = scred;
	if (SMB_DIALECT(SSTOVC(ctx->f_ssp)) < SMB_DIALECT_LANMAN2_0 ||
	    (dnp->n_mount->sm_args.flags & SMBFS_MOUNT_NO_LONG)) {
		ctx->f_flags |= SMBFS_RDD_USESEARCH;
		error = smbfs_findopenLM1(ctx, dnp, wildcard, wclen, attr, scred);
	} else
		error = smbfs_findopenLM2(ctx, dnp, wildcard, wclen, attr, scred);
	if (error)
		smbfs_findclose(ctx, scred);
	else
		*ctxpp = ctx;
	return error;
}

int
smbfs_findnext(struct smbfs_fctx *ctx, int limit, struct smb_cred *scred)
{
	int error;

	if (limit == 0)
		limit = 1000000;
	else if (limit > 1)
		limit *= 4;	/* empirical */
	ctx->f_scred = scred;
	for (;;) {
		if (ctx->f_flags & SMBFS_RDD_USESEARCH) {
			error = smbfs_findnextLM1(ctx, limit);
		} else
			error = smbfs_findnextLM2(ctx, limit);
		if (error)
			return error;

		/* Skip '.' and '..' */
		if ((ctx->f_nmlen == 1 && ctx->f_name[0] == '.') ||
		    (ctx->f_nmlen == 2 && ctx->f_name[0] == '.' &&
		     ctx->f_name[1] == '.'))
			continue;
		break;
	}
	smbfs_fname_tolocal(SSTOVC(ctx->f_ssp), ctx->f_name, &ctx->f_nmlen,
	    ctx->f_dnp->n_mount->sm_caseopt);
	ctx->f_attr.fa_ino = smbfs_getino(ctx->f_dnp, ctx->f_name, ctx->f_nmlen);
	return 0;
}

int
smbfs_findclose(struct smbfs_fctx *ctx, struct smb_cred *scred)
{
	ctx->f_scred = scred;
	if (ctx->f_flags & SMBFS_RDD_USESEARCH) {
		smbfs_findcloseLM1(ctx);
	} else
		smbfs_findcloseLM2(ctx);
	if (ctx->f_rname)
		free(ctx->f_rname, M_SMBFSDATA);
	free(ctx, M_SMBFSDATA);
	return 0;
}

int
smbfs_smb_lookup(struct smbnode *dnp, const char *name, int nmlen,
	struct smbfattr *fap, struct smb_cred *scred)
{
	struct smbfs_fctx *ctx;
	int error;

	if (dnp == NULL || (dnp->n_ino == 2 && name == NULL)) {
		memset(fap, 0, sizeof(*fap));
		fap->fa_attr = SMB_FA_DIR;
		fap->fa_ino = 2;
		return 0;
	}
	if (nmlen == 1 && name[0] == '.') {
		error = smbfs_smb_lookup(dnp, NULL, 0, fap, scred);
		return error;
	} else if (nmlen == 2 && name[0] == '.' && name[1] == '.') {
		error = smbfs_smb_lookup(VTOSMB(dnp->n_parent), NULL, 0,
		    fap, scred);
		printf("%s: knows NOTHING about '..'\n", __func__);
		return error;
	}
	error = smbfs_findopen(dnp, name, nmlen,
	    SMB_FA_SYSTEM | SMB_FA_HIDDEN | SMB_FA_DIR, scred, &ctx);
	if (error)
		return error;
	ctx->f_flags |= SMBFS_RDD_FINDSINGLE;
	error = smbfs_findnext(ctx, 1, scred);
	if (error == 0) {
		*fap = ctx->f_attr;
		if (name == NULL)
			fap->fa_ino = dnp->n_ino;

		/*
		 * Check the returned file name case exactly
		 * matches requested file name. ctx->f_nmlen is
		 * guaranteed to always match nmlen.
		 */
		if (nmlen > 0 && strncmp(name, ctx->f_name, nmlen) != 0)
			error = ENOENT;
	}
	smbfs_findclose(ctx, scred);
	return error;
}

/*
 * This call is used to fetch FID for directories. For normal files,
 * SMB_COM_OPEN is used.
 */
int
smbfs_smb_ntcreatex(struct smbnode *np, int accmode,
    struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	u_int8_t wc;
	u_int8_t *nmlen;
	u_int16_t flen;

	KASSERT(SMBTOV(np)->v_type == VDIR);

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_NT_CREATE_ANDX, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* Secondary command; 0xFF = None */
	mb_put_uint8(mbp, 0);		/* Reserved (must be 0) */
	mb_put_uint16le(mbp, 0);	/* Off to next cmd WordCount */
	mb_put_uint8(mbp, 0);		/* Reserved (must be 0) */
	nmlen = mb_reserve(mbp, sizeof(u_int16_t));
					/* Length of Name[] in bytes */
	mb_put_uint32le(mbp, SMB_FL_CANONICAL_PATHNAMES);
					/* Flags - Create bit set */
	mb_put_uint32le(mbp, 0);	/* If nonzero, open relative to this */
	mb_put_uint32le(mbp, NT_FILE_LIST_DIRECTORY);	/* Access mask */
	mb_put_uint32le(mbp, 0);	/* Low 32bit */
	mb_put_uint32le(mbp, 0);	/* Hi 32bit */
					/* Initial allocation size */
	mb_put_uint32le(mbp, 0);	/* File attributes */
	mb_put_uint32le(mbp, NT_FILE_SHARE_READ|NT_FILE_SHARE_WRITE);
					/* Type of share access */
	mb_put_uint32le(mbp, NT_OPEN_EXISTING);
					/* Create disposition - just open */
	mb_put_uint32le(mbp, NT_FILE_DIRECTORY_FILE);
					/* Options to use if creating a file */
	mb_put_uint32le(mbp, 0);	/* Security QOS information */
	mb_put_uint8(mbp, 0);		/* Security tracking mode flags */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);

	error = smbfs_fullpath(mbp, SSTOVC(ssp), np, NULL, 0);
	if (error)
		return error;

	/* Windows XP seems to include the final zero. Better do that too. */
	mb_put_uint8(mbp, 0);

	flen = mbp->mb_count;
	SMBRQ_PUTLE16(nmlen, flen);

	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	if (error)
		goto bad;

	smb_rq_getreply(rqp, &mdp);
	md_get_uint8(mdp, &wc);		/* WordCount - check? */
	md_get_uint8(mdp, NULL);	/* AndXCommand */
	md_get_uint8(mdp, NULL);	/* Reserved - must be zero */
	md_get_uint16(mdp, NULL);	/* Offset to next cmd WordCount */
	md_get_uint8(mdp, NULL);	/* Oplock level granted */
	md_get_uint16(mdp, &np->n_fid);	/* FID */
	/* ignore rest */

bad:
	smb_rq_done(rqp);
	return (error);
}

/*
 * Setup a request for NT DIRECTORY CHANGE NOTIFY.
 */
int
smbfs_smb_nt_dirnotify_setup(struct smbnode *dnp, struct smb_rq **rqpp, struct smb_cred *scred, void (*notifyhook)(void *), void *notifyarg)
{
	struct smb_rq *rqp;
	struct smb_share *ssp = dnp->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_NT_TRANSACT, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* Max setup words to return */
	mb_put_uint16le(mbp, 0);	/* Flags (according to Samba) */
	mb_put_uint32le(mbp, 0);	/* Total parameter bytes being sent*/
	mb_put_uint32le(mbp, 0);	/* Total data bytes being sent */
	mb_put_uint32le(mbp, 10*1024); /* Max parameter bytes to return */
	mb_put_uint32le(mbp, 0);	/* Max data bytes to return */
	mb_put_uint32le(mbp, 0);	/* Parameter bytes sent this buffer */
	mb_put_uint32le(mbp, 0);	/* Offset (from h. start) to Param */
	mb_put_uint32le(mbp, 0);	/* Data bytes sent this buffer */
	mb_put_uint32le(mbp, 0);	/* Offset (from h. start) to Data */
	mb_put_uint8(mbp, 4);		/* Count of setup words */
	mb_put_uint16le(mbp, SMB_NTTRANS_NOTIFY_CHANGE); /* Trans func code */

	/* NT TRANSACT NOTIFY CHANGE: Request Change Notification */
	mb_put_uint32le(mbp,
		FILE_NOTIFY_CHANGE_NAME|FILE_NOTIFY_CHANGE_ATTRIBUTES|
		FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE|
		FILE_NOTIFY_CHANGE_CREATION);	/* CompletionFilter */
	mb_put_mem(mbp, (void *)&dnp->n_fid, 2, MB_MSYSTEM);	/* FID */
	mb_put_uint8(mbp, 0);		/* WatchTree - Watch all subdirs too */
	mb_put_uint8(mbp, 0);		/* Reserved - must be zero */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);

	/* No timeout */
	rqp->sr_timo = -1;
	smb_rq_setcallback(rqp, notifyhook, notifyarg);

	error = smb_rq_enqueue(rqp);
	if (!error)
		*rqpp = rqp;
	else
		smb_rq_done(rqp);

	return (error);
}

int
smbfs_smb_nt_dirnotify_fetch(struct smb_rq *rqp, int *hint)
{
	int error;
	struct mdchain *mdp;
	u_int8_t sc;
	u_int32_t nextentry;

	error = smb_rq_reply(rqp);
	if (error) {
		/*
		 * If we get EMSGSIZE, there is already too many notifications
		 * available for the directory, and the internal buffer
		 * overflew. Just flag any possible relevant change.
		 */
		if (error == EMSGSIZE) {
			*hint = NOTE_ATTRIB | NOTE_WRITE;
			error = 0;
		}

		goto bad;
	}

	smb_rq_getreply(rqp, &mdp);

	/* Parse reply */
	error = md_get_mem(mdp, NULL, 4 + (8*4), MB_MZERO);	/* skip */
	if (error)
		goto bad;

	md_get_uint8(mdp, &sc);			/* SetupCount */
	if (sc > 0)
		md_get_mem(mdp, NULL, sc * sizeof(u_int16_t), MB_MZERO);
	md_get_uint16(mdp, NULL);		/* ByteCount */
	md_get_mem(mdp, NULL, 1 + (sc % 2) * 2, MB_MZERO);	/* Pad */

	/*
	 * The notify data are blocks of
	 *   ULONG nextentry - offset of next entry from start of this one
	 *   ULONG action - type of notification
	 *   ULONG filenamelen - length of filename in bytes
	 *   WCHAR filename[filenamelen/2] - Unicode filename
	 * nexentry == 0 means last notification, filename is in 16bit LE
	 * unicode
	 */
	*hint = 0;
	do {
		u_int32_t action;
#if 0
		u_int32_t fnlen;
		u_int16_t fnc;
#endif

		md_get_uint32le(mdp, &nextentry);
		md_get_uint32le(mdp, &action);
		if (nextentry)
			md_get_mem(mdp, NULL, nextentry - 2 * 4, MB_MZERO);
#if 0
		md_get_uint32le(mdp, &fnlen);

		printf("notify: next %u act %u fnlen %u fname '",
			nextentry, action, fnlen);
		for(; fnlen > 0; fnlen -= 2) {
			md_get_uint16le(mdp, &fnc);
			printf("%c", fnc&0xff);
		}
		printf("'\n");
#endif

		switch(action) {
		case FILE_ACTION_ADDED:
		case FILE_ACTION_REMOVED:
		case FILE_ACTION_RENAMED_OLD_NAME:
		case FILE_ACTION_RENAMED_NEW_NAME:
			*hint |= NOTE_ATTRIB | NOTE_WRITE;
			break;

		case FILE_ACTION_MODIFIED:
			*hint |= NOTE_ATTRIB;
			break;
		}
	} while(nextentry > 0);

bad:
	smb_rq_done(rqp);
	return error;
}

/*
 * Cancel previous SMB, with message ID mid. No reply is generated
 * to this one (only the previous message returns with error).
 */
int
smbfs_smb_ntcancel(struct smb_connobj *layer, u_int16_t mid, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mbuf *m;
	u_int8_t *mp;
	int error;

	error = smb_rq_alloc(layer, SMB_COM_NT_CANCEL, scred, &rqp);
	if (error)
		return (error);
	rqp->sr_flags |= SMBR_NOWAIT;	/* do not wait for reply */
	smb_rq_getrequest(rqp, &mbp);

	/*
	 * This is nonstandard. We need to rewrite the just written
	 * mid to different one. Access underlying mbuf directly.
	 * We assume mid is the last thing written smb_rq_alloc()
	 * to request buffer.
	 */
	m = mbp->mb_cur;
	mp = mtod(m, u_int8_t *) + m->m_len - 2;
	SMBRQ_PUTLE16(mp, mid);
	rqp->sr_mid = mid;

	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);

	error = (smb_rq_simple(rqp));

	/* Discard, there is no real reply */
	smb_rq_done(rqp);

	return (error);
}
