/*	$NetBSD: puffs_msgif.h,v 1.65.20.2 2009/12/14 19:36:57 sborrill Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007  Antti Kantee.  All Rights Reserved.
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

#ifndef _FS_PUFFS_PUFFS_MSGIF_H_
#define _FS_PUFFS_PUFFS_MSGIF_H_

#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/statvfs.h>
#include <sys/ucred.h>

#include <time.h>


#define PUFFSOP_VFS		0x01	/* kernel-> */
#define PUFFSOP_VN		0x02	/* kernel-> */
#define PUFFSOP_CACHE		0x03	/* only kernel-> */
#define PUFFSOP_ERROR		0x04	/* only kernel-> */
#define PUFFSOP_FLUSH		0x05	/* ->kernel */
#define PUFFSOP_SUSPEND		0x06	/* ->kernel */

#define PUFFSOPFLAG_FAF		0x10	/* fire-and-forget */
#define PUFFSOPFLAG_ISRESPONSE	0x20	/* req is actually a resp */

#define PUFFSOP_OPCMASK		0x07
#define PUFFSOP_OPCLASS(a)	((a) & PUFFSOP_OPCMASK)
#define PUFFSOP_WANTREPLY(a)	(((a) & PUFFSOPFLAG_FAF) == 0)

/* XXX: we don't need everything */
enum {
	PUFFS_VFS_MOUNT,	PUFFS_VFS_START,	PUFFS_VFS_UNMOUNT,
	PUFFS_VFS_ROOT,		PUFFS_VFS_STATVFS,	PUFFS_VFS_SYNC,
	PUFFS_VFS_VGET,		PUFFS_VFS_FHTOVP,	PUFFS_VFS_VPTOFH,
	PUFFS_VFS_INIT,		PUFFS_VFS_DONE,		PUFFS_VFS_SNAPSHOT,
	PUFFS_VFS_EXTATTCTL,	PUFFS_VFS_SUSPEND
};
#define PUFFS_VFS_MAX PUFFS_VFS_EXTATTCTL

/* moreXXX: we don't need everything here either */
enum {
	PUFFS_VN_LOOKUP,	PUFFS_VN_CREATE,	PUFFS_VN_MKNOD,
	PUFFS_VN_OPEN,		PUFFS_VN_CLOSE,		PUFFS_VN_ACCESS,
	PUFFS_VN_GETATTR,	PUFFS_VN_SETATTR,	PUFFS_VN_READ,
	PUFFS_VN_WRITE,		PUFFS_VN_IOCTL,		PUFFS_VN_FCNTL,
	PUFFS_VN_POLL,		PUFFS_VN_KQFILTER,	PUFFS_VN_REVOKE,
	PUFFS_VN_MMAP,		PUFFS_VN_FSYNC,		PUFFS_VN_SEEK,
	PUFFS_VN_REMOVE,	PUFFS_VN_LINK,		PUFFS_VN_RENAME,
	PUFFS_VN_MKDIR,		PUFFS_VN_RMDIR,		PUFFS_VN_SYMLINK,
	PUFFS_VN_READDIR,	PUFFS_VN_READLINK,	PUFFS_VN_ABORTOP,
	PUFFS_VN_INACTIVE,	PUFFS_VN_RECLAIM,	PUFFS_VN_LOCK,
	PUFFS_VN_UNLOCK,	PUFFS_VN_BMAP,		PUFFS_VN_STRATEGY,
	PUFFS_VN_PRINT,		PUFFS_VN_ISLOCKED,	PUFFS_VN_PATHCONF,
	PUFFS_VN_ADVLOCK,	PUFFS_VN_LEASE,		PUFFS_VN_WHITEOUT,
	PUFFS_VN_GETPAGES,	PUFFS_VN_PUTPAGES,	PUFFS_VN_GETEXTATTR,
	PUFFS_VN_LISTEXTATTR,	PUFFS_VN_OPENEXTATTR,	PUFFS_VN_DELETEEXTATTR,
	PUFFS_VN_SETEXTATTR
};
#define PUFFS_VN_MAX PUFFS_VN_SETEXTATTR

/*
 * These signal invalid parameters the file system returned.
 */
enum {
	PUFFS_ERR_MAKENODE,	PUFFS_ERR_LOOKUP,	PUFFS_ERR_READDIR,
	PUFFS_ERR_READLINK,	PUFFS_ERR_READ,		PUFFS_ERR_WRITE,
	PUFFS_ERR_VPTOFH,	PUFFS_ERR_ERROR
};
#define PUFFS_ERR_MAX PUFFS_ERR_VPTOFH

#define PUFFSDEVELVERS	0x80000000
#define PUFFSVERSION	26
#define PUFFSNAMESIZE	32

#define PUFFS_TYPEPREFIX "puffs|"

#if 0
#define PUFFS_TYPELEN (_VFS_NAMELEN - (sizeof(PUFFS_TYPEPREFIX)+1))
#define PUFFS_NAMELEN (_VFS_MNAMELEN-1)
#endif

/* 
 * Just a weak typedef for code clarity.  Additionally, we have a
 * more appropriate vanity type for puffs:
 * <uep> it should be croissant, not cookie.
 */
typedef void *puffs_cookie_t;
typedef puffs_cookie_t puffs_croissant_t;

/* FIXME: move? */
typedef off_t voff_t;

/* other netbsd stuff
 * FIXME: move to some other place?
 */
typedef int32_t lwpid_t;        /* LWP id */
typedef unsigned long vsize_t;
typedef int vm_prot_t;
typedef uint64_t u_quad_t;       /* quads */


/* FIXME: from sys/vnode.h, which is commented. */
/*
 * Vnode attributes.  A field value of VNOVAL represents a field whose value
 * is unavailable (getattr) or which is not to be changed (setattr).
 */
enum vtype      { VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO, VBAD };
struct vattr {
        enum vtype      va_type;        /* vnode type (for create) */
        mode_t          va_mode;        /* files access mode and type */
        nlink_t         va_nlink;       /* number of references to file */
        uid_t           va_uid;         /* owner user id */
        gid_t           va_gid;         /* owner group id */
        long            va_fsid;        /* file system id (dev for now) */
        ino_t           va_fileid;      /* file id */
        u_quad_t        va_size;        /* file size in bytes */
        long            va_blocksize;   /* blocksize preferred for i/o */
        struct timespec va_atime;       /* time of last access */
        struct timespec va_mtime;       /* time of last modification */
        struct timespec va_ctime;       /* time file changed */
        struct timespec va_birthtime;   /* time file created */
        u_long          va_gen;         /* generation number of file */
        u_long          va_flags;       /* flags defined for file */
        dev_t           va_rdev;        /* device the special file represents */
        u_quad_t        va_bytes;       /* bytes of disk space held by file */
        u_quad_t        va_filerev;     /* file modification number */
        u_int           va_vaflags;     /* operations flags, see below */
        long            va_spare;       /* remain quad aligned */
};

struct puffs_kargs {
        unsigned int    pa_vers;
        int             pa_fd;

        uint32_t        pa_flags;

        size_t          pa_maxmsglen;
        int             pa_nhashbuckets;

        size_t          pa_fhsize;
        int             pa_fhflags;

        puffs_cookie_t  pa_root_cookie;
        enum vtype      pa_root_vtype;
        voff_t          pa_root_vsize;
        dev_t           pa_root_rdev;

        struct statvfs  pa_svfsb;

        char            pa_typename[NAME_MAX + 1];
        char            pa_mntfromname[NAME_MAX + 1];

        uint8_t         pa_vnopmask[PUFFS_VN_MAX];
};



#define PUFFS_KFLAG_NOCACHE_NAME	0x01	/* don't use name cache     */
#define PUFFS_KFLAG_NOCACHE_PAGE	0x02	/* don't use page cache	    */
#define PUFFS_KFLAG_NOCACHE		0x03	/* no cache whatsoever      */
#define PUFFS_KFLAG_ALLOPS		0x04	/* ignore pa_vnopmask       */
#define PUFFS_KFLAG_WTCACHE		0x08	/* write-through page cache */
#define PUFFS_KFLAG_IAONDEMAND		0x10	/* inactive only on demand  */
#define PUFFS_KFLAG_LOOKUP_FULLPNBUF	0x20	/* full pnbuf in lookup     */
#define PUFFS_KFLAG_MASK		0x3f

#define PUFFS_FHFLAG_DYNAMIC		0x01
#define PUFFS_FHFLAG_NFSV2		0x02
#define PUFFS_FHFLAG_NFSV3		0x04
#define PUFFS_FHFLAG_PROTOMASK		0x06
#define PUFFS_FHFLAG_PASSTHROUGH	0x08
#define PUFFS_FHFLAG_MASK		0x0f

#define PUFFS_FHSIZE_MAX	1020	/* XXX: FHANDLE_SIZE_MAX - 4 */

#define PUFFS_SETBACK_INACT_N1	0x01	/* set VOP_INACTIVE for node 1 */
#define PUFFS_SETBACK_INACT_N2	0x02	/* set VOP_INACTIVE for node 2 */
#define PUFFS_SETBACK_NOREF_N1	0x04	/* set pn PN_NOREFS for node 1 */
#define PUFFS_SETBACK_NOREF_N2	0x08	/* set pn PN_NOREFS for node 2 */
#define PUFFS_SETBACK_MASK	0x0f

#define PUFFS_INVAL_NAMECACHE_NODE		0
#define PUFFS_INVAL_NAMECACHE_DIR		1
#define PUFFS_INVAL_NAMECACHE_ALL		2
#define PUFFS_INVAL_PAGECACHE_NODE_RANGE	3
#define PUFFS_FLUSH_PAGECACHE_NODE_RANGE	4

/* keep this for now */
#define PUFFSREQSIZEOP		_IOR ('p', 1, size_t)

#define PUFFCRED_TYPE_UUC	1
#define PUFFCRED_TYPE_INTERNAL	2
#define PUFFCRED_CRED_NOCRED	1
#define PUFFCRED_CRED_FSCRED	2

/*
 * 2*MAXPHYS is the max size the system will attempt to copy,
 * else treated as garbage
 */
#define PUFFS_MSG_MAXSIZE	2*MAXPHYS
#define PUFFS_MSGSTRUCT_MAX	4096 /* XXX: approxkludge */

#define PUFFS_TOMOVE(a,b) (MIN((a), b->pmp_msg_maxsize - PUFFS_MSGSTRUCT_MAX))

/* puffs struct componentname built by kernel */
struct puffs_kcn {
        /* args */
        uint32_t                pkcn_nameiop;   /* namei operation      */
        uint32_t                pkcn_flags;     /* flags                */

        char pkcn_name[MAXPATHLEN];     /* nulterminated path component */
        size_t pkcn_namelen;            /* current component length     */
        size_t pkcn_consume;            /* IN: extra chars server ate   */
};

/*
 * Credentials for an operation.  Can be either struct uucred for
 * ops called from a credential context or NOCRED/FSCRED for ops
 * called from within the kernel.  It is up to the implementation
 * if it makes a difference between these two and the super-user.
 */
struct puffs_kcred {
	struct uucred	pkcr_uuc;
	uint8_t		pkcr_type;
	uint8_t		pkcr_internal;
};
#define PUFFCRED_TYPE_UUC	1
#define PUFFCRED_TYPE_INTERNAL	2
#define PUFFCRED_CRED_NOCRED	1
#define PUFFCRED_CRED_FSCRED	2


#endif /* _FS_PUFFS_PUFFS_MSGIF_H_ */
