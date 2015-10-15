/*	$NetBSD: puffs_compat.c,v 1.4 2015/04/22 17:07:24 pooka Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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

/*
 * This file handles puffs PDUs so that they are compatible between
 * 32bit<->64bit time_t/dev_t.  It enables running a -current kernel
 * against a 5.0 userland (assuming the protocol otherwise matches!).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: puffs_compat.c,v 1.4 2015/04/22 17:07:24 pooka Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/atomic.h>

#include <dev/putter/putter_sys.h>

#include <fs/puffs/puffs_msgif.h>
#include <fs/puffs/puffs_sys.h>

#include <compat/sys/time.h>

/*
 * compat types
 */
struct vattr50 {
	enum vtype		va_type;
	mode_t			va_mode;
	nlink_t			va_nlink;
	uid_t			va_uid;
	gid_t			va_gid;
	uint32_t		va_fsid;
	ino_t			va_fileid;
	u_quad_t		va_size;
	long			va_blocksize;
	struct timespec50	va_atime;
	struct timespec50	va_mtime;
	struct timespec50	va_ctime;
	struct timespec50	va_birthtime;
	u_long			va_gen;
	u_long			va_flags;
	uint32_t		va_rdev;
	u_quad_t		va_bytes;
	u_quad_t		va_filerev;
	u_int			va_vaflags;
	long			va_spare;
};

struct puffs50_vfsmsg_fhtonode {
	struct puffs_req	pvfsr_pr;

	void			*pvfsr_fhcookie;	/* IN   */
	enum vtype		pvfsr_vtype;		/* IN   */
	voff_t			pvfsr_size;		/* IN   */
	uint32_t		pvfsr_rdev;		/* IN   */

	size_t			pvfsr_dsize;		/* OUT */
	uint8_t			pvfsr_data[0]		/* OUT, XXX */
					__aligned(ALIGNBYTES+1);
};

struct puffs50_vnmsg_lookup {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/

	puffs_cookie_t		pvnr_newnode;		/* IN	*/
	enum vtype		pvnr_vtype;		/* IN	*/
	voff_t			pvnr_size;		/* IN	*/
	uint32_t		pvnr_rdev;		/* IN	*/
};

struct puffs50_vnmsg_create {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/

	struct vattr50		pvnr_va;		/* OUT	*/
	puffs_cookie_t		pvnr_newnode;		/* IN	*/
};

struct puffs50_vnmsg_mknod {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/

	struct vattr50		pvnr_va;		/* OUT	*/
	puffs_cookie_t		pvnr_newnode;		/* IN	*/
};

#define puffs50_vnmsg_setattr puffs50_vnmsg_setgetattr
#define puffs50_vnmsg_getattr puffs50_vnmsg_setgetattr
struct puffs50_vnmsg_setgetattr {
	struct puffs_req	pvn_pr;

	struct puffs_kcred	pvnr_cred;		/* OUT	*/
	struct vattr50		pvnr_va;		/* IN/OUT (op depend) */
};

struct puffs50_vnmsg_mkdir {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/

	struct vattr50		pvnr_va;		/* OUT	*/
	puffs_cookie_t		pvnr_newnode;		/* IN	*/
};

struct puffs50_vnmsg_symlink {
	struct puffs_req	pvn_pr;

	struct puffs_kcn	pvnr_cn;		/* OUT	*/
	struct puffs_kcred	pvnr_cn_cred;		/* OUT	*/

	struct vattr50		pvnr_va;		/* OUT	*/
	puffs_cookie_t		pvnr_newnode;		/* IN	*/
	char			pvnr_link[MAXPATHLEN];	/* OUT	*/
};

/*
 * vattr translation routines
 */

#ifdef COMPAT_50
static void
vattr_to_50(const struct vattr *va, struct vattr50 *va50)
{

	va50->va_type = va->va_type;
	va50->va_mode = va->va_mode;
	va50->va_nlink = va->va_nlink;
	va50->va_uid = va->va_uid;
	va50->va_gid = va->va_gid;
	va50->va_fsid = (uint64_t)va->va_fsid;
	va50->va_fileid = va->va_fileid;
	va50->va_size = va->va_size;
	va50->va_blocksize = va->va_blocksize;
	timespec_to_timespec50(&va->va_atime, &va50->va_atime);
	timespec_to_timespec50(&va->va_ctime, &va50->va_ctime);
	timespec_to_timespec50(&va->va_mtime, &va50->va_mtime);
	timespec_to_timespec50(&va->va_birthtime, &va50->va_birthtime);
	va50->va_gen = va->va_gen;
	va50->va_flags = va->va_flags;
	va50->va_rdev = (int32_t)va->va_rdev;
	va50->va_bytes = va->va_bytes;
	va50->va_filerev = va->va_filerev;
	va50->va_vaflags = va->va_flags;
}

static void
vattr_from_50(const struct vattr50 *va50, struct vattr *va)
{

	va->va_type = va50->va_type;
	va->va_mode = va50->va_mode;
	va->va_nlink = va50->va_nlink;
	va->va_uid = va50->va_uid;
	va->va_gid = va50->va_gid;
	va->va_fsid = (uint32_t)va50->va_fsid;
	va->va_fileid = va50->va_fileid;
	va->va_size = va50->va_size;
	va->va_blocksize = va50->va_blocksize;
	timespec50_to_timespec(&va50->va_atime, &va->va_atime);
	timespec50_to_timespec(&va50->va_ctime, &va->va_ctime);
	timespec50_to_timespec(&va50->va_mtime, &va->va_mtime);
	timespec50_to_timespec(&va50->va_birthtime, &va->va_birthtime);
	va->va_gen = va50->va_gen;
	va->va_flags = va50->va_flags;
	va->va_rdev = (uint32_t)va50->va_rdev;
	va->va_bytes = va50->va_bytes;
	va->va_filerev = va50->va_filerev;
	va->va_vaflags = va50->va_flags;
}
#endif /* COMPAT_50 */

/*
 * XXX: cannot assert that sleeping is possible
 * (this always a valid assumption for now)
 */
#define INIT(name, extra)						\
	struct puffs50_##name *cmsg;					\
	struct puffs_##name *omsg;					\
	creq =kmem_zalloc(sizeof(struct puffs50_##name)+extra,KM_SLEEP);\
	cmsg = (struct puffs50_##name *)creq;				\
	omsg = (struct puffs_##name *)oreq;				\
	delta = sizeof(struct puffs50_##name)-sizeof(struct puffs_##name);
#define ASSIGN(field)							\
	cmsg->field = omsg->field;

bool
puffs_compat_outgoing(struct puffs_req *oreq,
	struct puffs_req **creqp, ssize_t *deltap)
{
	bool rv = false;
#ifdef COMPAT_50
	struct puffs_req *creq = NULL;
	ssize_t delta = 0;

	if (PUFFSOP_OPCLASS(oreq->preq_opclass) == PUFFSOP_VFS
	    && oreq->preq_optype == PUFFS_VFS_FHTOVP) {
		INIT(vfsmsg_fhtonode,
		    ((struct puffs_vfsmsg_fhtonode *)oreq)->pvfsr_dsize);

		ASSIGN(pvfsr_pr);
		ASSIGN(pvfsr_dsize);
		memcpy(cmsg->pvfsr_data, omsg->pvfsr_data, cmsg->pvfsr_dsize);
	} else if (PUFFSOP_OPCLASS(oreq->preq_opclass) == PUFFSOP_VN) {
		switch (oreq->preq_optype) {
		case PUFFS_VN_LOOKUP:
		{
			INIT(vnmsg_lookup, 0);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_cn);
			ASSIGN(pvnr_cn_cred);

			break;
		}

		case PUFFS_VN_CREATE:
		{
			INIT(vnmsg_create, 0);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_cn);
			ASSIGN(pvnr_cn_cred);
			vattr_to_50(&omsg->pvnr_va, &cmsg->pvnr_va);

			break;
		}

		case PUFFS_VN_MKNOD:
		{
			INIT(vnmsg_mknod, 0);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_cn);
			ASSIGN(pvnr_cn_cred);
			vattr_to_50(&omsg->pvnr_va, &cmsg->pvnr_va);

			break;
		}

		case PUFFS_VN_MKDIR:
		{
			INIT(vnmsg_mkdir, 0);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_cn);
			ASSIGN(pvnr_cn_cred);
			vattr_to_50(&omsg->pvnr_va, &cmsg->pvnr_va);

			break;
		}

		case PUFFS_VN_SYMLINK:
		{
			INIT(vnmsg_symlink, 0);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_cn);
			ASSIGN(pvnr_cn_cred);
			vattr_to_50(&omsg->pvnr_va, &cmsg->pvnr_va);
			memcpy(cmsg->pvnr_link, omsg->pvnr_link,
			    sizeof(cmsg->pvnr_link));

			break;
		}

		case PUFFS_VN_SETATTR:
		{
			INIT(vnmsg_setattr, 0);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_cred);
			vattr_to_50(&omsg->pvnr_va, &cmsg->pvnr_va);

			break;
		}
		case PUFFS_VN_GETATTR:
		{
			INIT(vnmsg_getattr, 0);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_cred);

			break;
		}

		default:
			break;
		}
	}

	if (creq) {
		*creqp = creq;
		*deltap = delta;
		rv = true;
	}
#endif

	return rv;
}
#undef INIT
#undef ASSIGN

#define INIT(name)							\
	struct puffs50_##name *cmsg = (void *)preq;			\
	struct puffs_##name *omsg = (void *)creq;
#define ASSIGN(field)							\
	omsg->field = cmsg->field;

void
puffs_compat_incoming(struct puffs_req *preq, struct puffs_req *creq)
{

#ifdef COMPAT_50
	if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VFS
	    && preq->preq_optype == PUFFS_VFS_FHTOVP) {
		INIT(vfsmsg_fhtonode);

		ASSIGN(pvfsr_pr);

		ASSIGN(pvfsr_fhcookie);
		ASSIGN(pvfsr_vtype);
		ASSIGN(pvfsr_size);
		ASSIGN(pvfsr_rdev);
	} else if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VN) {
		switch (preq->preq_optype) {
		case PUFFS_VN_LOOKUP:
		{
			INIT(vnmsg_lookup);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_newnode);
			ASSIGN(pvnr_vtype);
			ASSIGN(pvnr_size);
			ASSIGN(pvnr_rdev);

			break;
		}

		case PUFFS_VN_CREATE:
		{
			INIT(vnmsg_create);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_newnode);

			break;
		}

		case PUFFS_VN_MKNOD:
		{
			INIT(vnmsg_mknod);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_newnode);

			break;
		}

		case PUFFS_VN_MKDIR:
		{
			INIT(vnmsg_mkdir);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_newnode);

			break;
		}

		case PUFFS_VN_SYMLINK:
		{
			INIT(vnmsg_symlink);

			ASSIGN(pvn_pr);
			ASSIGN(pvnr_newnode);

			break;
		}

		case PUFFS_VN_SETATTR:
		{
			INIT(vnmsg_setattr);

			ASSIGN(pvn_pr);

			break;
		}
		case PUFFS_VN_GETATTR:
		{
			INIT(vnmsg_getattr);

			ASSIGN(pvn_pr);
			vattr_from_50(&cmsg->pvnr_va, &omsg->pvnr_va);

			break;
		}

		default:
			panic("puffs compat ops come in pairs");
		}
	}
#endif /* COMPAT_50 */
}
