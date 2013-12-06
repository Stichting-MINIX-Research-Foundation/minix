/*	$NetBSD: opdump.c,v 1.36 2012/03/15 02:02:21 joerg Exp $	*/

/*
 * Copyright (c) 2005, 2006  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
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

/* Pretty-printing helper routines for VFS/VOP request contents */

/* yes, this is pretty much a mess */

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: opdump.c,v 1.36 2012/03/15 02:02:21 joerg Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/time.h>

#include <puffs.h>
#include <puffsdump.h>
#include <stdarg.h>
#include <stdio.h>

#include "puffs_priv.h"

#define DINT "    "

const char *puffsdump_vfsop_revmap[] = {
	"PUFFS_VFS_MOUNT",
	"PUFFS_VFS_START",
	"PUFFS_VFS_UNMOUNT",
	"PUFFS_VFS_ROOT",
	"PUFFS_VFS_QUOTACTL",
	"PUFFS_VFS_STATVFS",
	"PUFFS_VFS_SYNC",
	"PUFFS_VFS_VGET",
	"PUFFS_VFS_FHTOVP",
	"PUFFS_VFS_VPTOFH",
	"PUFFS_VFS_INIT",
	"PUFFS_VFS_DONE",
	"PUFFS_VFS_SNAPSHOT",
	"PUFFS_VFS_EXTATTRCTL",
	"PUFFS_VFS_SUSPEND"
};
size_t puffsdump_vfsop_count = __arraycount(puffsdump_vfsop_revmap);

const char *puffsdump_vnop_revmap[] = {
	"PUFFS_VN_LOOKUP",
	"PUFFS_VN_CREATE",
	"PUFFS_VN_MKNOD",
	"PUFFS_VN_OPEN",
	"PUFFS_VN_CLOSE",
	"PUFFS_VN_ACCESS",
	"PUFFS_VN_GETATTR",
	"PUFFS_VN_SETATTR",
	"PUFFS_VN_READ",
	"PUFFS_VN_WRITE",
	"PUFFS_VN_IOCTL",
	"PUFFS_VN_FCNTL",
	"PUFFS_VN_POLL",
	"PUFFS_VN_KQFILTER",
	"PUFFS_VN_REVOKE",
	"PUFFS_VN_MMAP",
	"PUFFS_VN_FSYNC",
	"PUFFS_VN_SEEK",
	"PUFFS_VN_REMOVE",
	"PUFFS_VN_LINK",
	"PUFFS_VN_RENAME",
	"PUFFS_VN_MKDIR",
	"PUFFS_VN_RMDIR",
	"PUFFS_VN_SYMLINK",
	"PUFFS_VN_READDIR",
	"PUFFS_VN_READLINK",
	"PUFFS_VN_ABORTOP",
	"PUFFS_VN_INACTIVE",
	"PUFFS_VN_RECLAIM",
	"PUFFS_VN_LOCK",
	"PUFFS_VN_UNLOCK",
	"PUFFS_VN_BMAP",
	"PUFFS_VN_STRATEGY",
	"PUFFS_VN_PRINT",
	"PUFFS_VN_ISLOCKED",
	"PUFFS_VN_PATHCONF",
	"PUFFS_VN_ADVLOCK",
	"PUFFS_VN_LEASE",
	"PUFFS_VN_WHITEOUT",
	"PUFFS_VN_GETPAGES",
	"PUFFS_VN_PUTPAGES",
	"PUFFS_VN_GETEXTATTR",
	"PUFFS_VN_LISTEXTATTR",
	"PUFFS_VN_OPENEXTATTR",
	"PUFFS_VN_DELETEEXTATTR",
	"PUFFS_VN_SETEXTATTR",
	"PUFFS_VN_CLOSEEXTATTR",
};
size_t puffsdump_vnop_count = __arraycount(puffsdump_vnop_revmap);

/* XXX! */
const char *puffsdump_cacheop_revmap[] = {
	"PUFFS_CACHE_WRITE"
};

const char *puffsdump_errnot_revmap[] = {
	"PUFFS_ERR_ERROR",
	"PUFFS_ERR_MAKENODE",
	"PUFFS_ERR_LOOKUP",
	"PUFFS_ERR_READDIR",
	"PUFFS_ERR_READLINK",
	"PUFFS_ERR_READ",
	"PUFFS_ERR_WRITE",
	"PUFFS_ERR_VPTOFH",
	"PUFFS_ERR_GETEXTATTR",
	"PUFFS_ERR_LISTEXTATTR",
};
size_t puffsdump_errnot_count = __arraycount(puffsdump_errnot_revmap);

const char *puffsdump_flush_revmap[] = {
	"PUFFS_INVAL_NAMECACHE_NODE",
	"PUFFS_INVAL_NAMECACHE_DIR",
	"PUFFS_INVAL_NAMECACHE_ALL",
	"PUFFS_INVAL_PAGECACHE_NODE_RANGE",
	"PUFFS_FLUSH_PAGECACHE_NODE_RANGE",
};
size_t puffsdump_flush_count = __arraycount(puffsdump_flush_revmap);

static __printflike(1, 2) void
mydprintf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
puffsdump_req(struct puffs_req *preq)
{
	char buf[128];
	static struct timeval tv_prev;
	struct timeval tv_now, tv;
	const char **map;
	const char *optype;
	size_t maxhandle;
	int opclass, isvn = 0;

	mydprintf("reqid: %" PRIu64 ", ", preq->preq_id);
	opclass = PUFFSOP_OPCLASS(preq->preq_opclass);
	switch (opclass) {
	case PUFFSOP_VFS:
		map = puffsdump_vfsop_revmap;
		maxhandle = puffsdump_vfsop_count;
		break;
	case PUFFSOP_VN:
		map = puffsdump_vnop_revmap;
		maxhandle = puffsdump_vnop_count;
		isvn = 1;
		break;
	case PUFFSOP_CACHE:
		map = puffsdump_cacheop_revmap;
		maxhandle = __arraycount(puffsdump_cacheop_revmap);
		break;
	case PUFFSOP_ERROR:
		map = puffsdump_errnot_revmap;
		maxhandle = puffsdump_errnot_count;
		break;
	case PUFFSOP_FLUSH:
		map = puffsdump_flush_revmap;
		maxhandle = puffsdump_flush_count;
		break;
	default:
		mydprintf("unhandled opclass %d\n", opclass);
		return;
	}

	if (preq->preq_optype < maxhandle) {
		optype = map[preq->preq_optype];
	} else {
		snprintf(buf, sizeof(buf), "UNKNOWN (%d)", preq->preq_optype);
		optype = buf;
	}

	mydprintf("opclass %d%s, optype: %s, "
	    "cookie: %p,\n" DINT "aux: %p, auxlen: %zu, pid: %d, lwpid: %d\n",
	    opclass, PUFFSOP_WANTREPLY(preq->preq_opclass) ? "" : " (FAF)",
	    optype, preq->preq_cookie,
	    preq->preq_buf, preq->preq_buflen,
	    preq->preq_pid, preq->preq_lid);

	if (isvn) {
		switch (preq->preq_optype) {
		case PUFFS_VN_LOOKUP:
			puffsdump_lookup(preq);
			break;
		case PUFFS_VN_READ:
		case PUFFS_VN_WRITE:
			puffsdump_readwrite(preq);
			break;
		case PUFFS_VN_OPEN:
			puffsdump_open(preq);
			break;
		case PUFFS_VN_REMOVE:
		case PUFFS_VN_RMDIR:
		case PUFFS_VN_LINK:
			puffsdump_targ(preq);
			break;
		case PUFFS_VN_READDIR:
			puffsdump_readdir(preq);
			break;
		case PUFFS_VN_CREATE:
		case PUFFS_VN_MKDIR:
		case PUFFS_VN_MKNOD:
		case PUFFS_VN_SYMLINK:
			puffsdump_create(preq);
			break;
		case PUFFS_VN_SETATTR:
			puffsdump_attr(preq);
			break;
		default:
			break;
		}
	}

	PU_LOCK();
	gettimeofday(&tv_now, NULL);
	timersub(&tv_now, &tv_prev, &tv);
	mydprintf(DINT "since previous call: %lld.%06ld\n",
	    (long long)tv.tv_sec, (long)tv.tv_usec);
	gettimeofday(&tv_prev, NULL);
	PU_UNLOCK();
}

void
puffsdump_rv(struct puffs_req *preq)
{

	if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VN) {
		switch (preq->preq_optype) {
		case PUFFS_VN_LOOKUP:
			puffsdump_lookup_rv(preq);
			break;
		case PUFFS_VN_CREATE:
		case PUFFS_VN_MKDIR:
		case PUFFS_VN_MKNOD:
		case PUFFS_VN_SYMLINK:
			puffsdump_create_rv(preq);
			break;
		case PUFFS_VN_READ:
		case PUFFS_VN_WRITE:
			puffsdump_readwrite_rv(preq);
			break;
		case PUFFS_VN_READDIR:
			puffsdump_readdir_rv(preq);
			break;
		case PUFFS_VN_GETATTR:
			puffsdump_attr(preq);
			break;
		default:
			break;
		}
	}

	mydprintf("RV reqid: %" PRIu64 ", result: %d %s\n",
	    preq->preq_id, preq->preq_rv,
	    preq->preq_rv ? strerror(preq->preq_rv) : "");
}

/*
 * Slightly tedious print-routine so that we get a nice NOVAL instead
 * of some tedious output representations for -1, especially (uint64_t)-1
 *
 * We use typecasting to make this work beyond time_t/dev_t size changes.
 */
static void
dumpattr(struct vattr *vap)
{
	const char * const vtypes[] = { VNODE_TYPES };
	char buf[128];

/* XXX: better readability.  and this is debug, so no cycle-sweat */
#define DEFAULTBUF() snprintf(buf, sizeof(buf), "NOVAL")

	mydprintf(DINT "vattr:\n");
	mydprintf(DINT DINT "type: %s, ", vtypes[vap->va_type]);

	DEFAULTBUF();
	if (vap->va_mode != (mode_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "0%o", vap->va_mode);
	mydprintf("mode: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_nlink != (nlink_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%d", vap->va_nlink);
	mydprintf("nlink: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_uid != (uid_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%d", vap->va_uid);
	mydprintf("uid: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_gid != (gid_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%d", vap->va_gid);
	mydprintf("gid: %s\n", buf);

	DEFAULTBUF();
	if ((unsigned long long)vap->va_fsid!=(unsigned long long)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "0x%llx",
		    (unsigned long long)vap->va_fsid);
	mydprintf(DINT DINT "fsid: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_fileid != (ino_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%" PRIu64, vap->va_fileid);
	mydprintf("ino: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_size != (u_quad_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%" PRIu64, vap->va_size);
	mydprintf("size: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_blocksize != (long)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%ld", vap->va_blocksize);
	mydprintf("bsize: %s\n", buf);

	DEFAULTBUF();
	if (vap->va_atime.tv_sec != (time_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%lld",
		    (long long)vap->va_atime.tv_sec);
	mydprintf(DINT DINT "a.s: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_atime.tv_nsec != (long)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%ld", vap->va_atime.tv_nsec);
	mydprintf("a.ns: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_mtime.tv_sec != (time_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%lld",
		    (long long)vap->va_mtime.tv_sec);
	mydprintf("m.s: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_mtime.tv_nsec != (long)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%ld", vap->va_mtime.tv_nsec);
	mydprintf("m.ns: %s\n", buf);

	DEFAULTBUF();
	if (vap->va_ctime.tv_sec != (time_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%lld",
		    (long long)vap->va_ctime.tv_sec);
	mydprintf(DINT DINT "c.s: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_ctime.tv_nsec != (long)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%ld", vap->va_ctime.tv_nsec);
	mydprintf("c.ns: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_birthtime.tv_sec != (time_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%lld",
		    (long long)vap->va_birthtime.tv_sec);
	mydprintf("b.s: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_birthtime.tv_nsec != (long)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%ld", vap->va_birthtime.tv_nsec);
	mydprintf("b.ns: %s\n", buf);

	DEFAULTBUF();
	if (vap->va_gen != (u_long)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%lu", vap->va_gen);
	mydprintf(DINT DINT "gen: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_flags != (u_long)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "0x%lx", vap->va_flags);
	mydprintf("flags: %s, ", buf);

	DEFAULTBUF();
	if (vap->va_rdev != (dev_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "0x%llx",
		    (unsigned long long)vap->va_rdev);
	mydprintf("rdev: %s\n", buf);

	DEFAULTBUF();
	if (vap->va_bytes != (u_quad_t)PUFFS_VNOVAL)
		snprintf(buf, sizeof(buf), "%" PRIu64, vap->va_bytes);
	mydprintf(DINT DINT "bytes: %s, ", buf);

	snprintf(buf, sizeof(buf), "%" PRIu64, vap->va_filerev);
	mydprintf("filerev: %s, ", buf);

	snprintf(buf, sizeof(buf), "0x%x", vap->va_vaflags);
	mydprintf("vaflags: %s\n", buf);
}

void
puffsdump_cookie(puffs_cookie_t c, const char *cookiename)
{
	
	mydprintf("%scookie: at %p\n", cookiename, c);
}

static const char *cn_opnames[] = {
	"LOOKUP",
	"CREATE",
	"DELETE",
	"RENAME"
};

void
puffsdump_cn(struct puffs_kcn *pkcn)
{

	mydprintf(DINT "puffs_cn: \"%s\", len %zu op %s (flags 0x%x)\n",
	    pkcn->pkcn_name, pkcn->pkcn_namelen,
	    cn_opnames[pkcn->pkcn_nameiop & NAMEI_OPMASK],
	    pkcn->pkcn_flags);
}

void
puffsdump_lookup(struct puffs_req *preq)
{
	struct puffs_vnmsg_lookup *lookup_msg = (void *)preq;

	puffsdump_cn(&lookup_msg->pvnr_cn);
}

void
puffsdump_lookup_rv(struct puffs_req *preq)
{
	struct puffs_vnmsg_lookup *lookup_msg = (void *)preq;

	mydprintf(DINT "new %p, type 0x%x, size 0x%"PRIu64", dev 0x%llx\n",
	    lookup_msg->pvnr_newnode, lookup_msg->pvnr_vtype,
	    lookup_msg->pvnr_size, (unsigned long long)lookup_msg->pvnr_rdev);
}

void
puffsdump_create(struct puffs_req *preq)
{
	/* XXX: wrong type, but we know it fits the slot */
	struct puffs_vnmsg_create *create_msg = (void *)preq;
	
	dumpattr(&create_msg->pvnr_va);
}

void
puffsdump_create_rv(struct puffs_req *preq)
{
	/* XXX: wrong type, but we know it fits the slot */
	struct puffs_vnmsg_create *create_msg = (void *)preq;

	mydprintf(DINT "new %p\n", create_msg->pvnr_newnode);
}

void
puffsdump_readwrite(struct puffs_req *preq)
{
	struct puffs_vnmsg_rw *rw_msg = (void *)preq;

	mydprintf(DINT "offset: %" PRId64 ", resid %zu, ioflag 0x%x\n",
	    rw_msg->pvnr_offset, rw_msg->pvnr_resid, rw_msg->pvnr_ioflag);
}

void
puffsdump_readwrite_rv(struct puffs_req *preq)
{
	struct puffs_vnmsg_rw *rw_msg = (void *)preq;

	mydprintf(DINT "resid after op: %zu\n", rw_msg->pvnr_resid);
}

void
puffsdump_readdir_rv(struct puffs_req *preq)
{
	struct puffs_vnmsg_readdir *readdir_msg = (void *)preq;

	mydprintf(DINT "resid after op: %zu, eofflag %d\n",
	    readdir_msg->pvnr_resid, readdir_msg->pvnr_eofflag);
}

void
puffsdump_open(struct puffs_req *preq)
{
	struct puffs_vnmsg_open *open_msg = (void *)preq;

	mydprintf(DINT "mode: 0x%x\n", open_msg->pvnr_mode);
}

void
puffsdump_targ(struct puffs_req *preq)
{
	struct puffs_vnmsg_remove *remove_msg = (void *)preq; /* XXX! */

	mydprintf(DINT "target cookie: %p\n", remove_msg->pvnr_cookie_targ);
}

void
puffsdump_readdir(struct puffs_req *preq)
{
	struct puffs_vnmsg_readdir *readdir_msg = (void *)preq;

	mydprintf(DINT "read offset: %" PRId64 "\n", readdir_msg->pvnr_offset);
}

void
puffsdump_attr(struct puffs_req *preq)
{
	struct puffs_vnmsg_setgetattr *attr_msg = (void *)preq;

	dumpattr(&attr_msg->pvnr_va);
}
