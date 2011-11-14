/*	$NetBSD: puffs.h,v 1.108.4.4 2009/10/27 20:37:38 bouyer Exp $	*/

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

#ifndef _PUFFS_H_
#define _PUFFS_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/queue.h>

#include "puffs_msgif.h"

#include <mntopts.h>
#include <time.h>

#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include <dirent.h>


/* MINIX forwards */
struct flock;
struct dirent;

/* XXX: from sys/fstypes.h
 * Flags for various system call interfaces.
 *
 * waitfor flags to vfs_sync() and getvfsstat()
 */
#define MNT_WAIT        1       /* synchronously wait for I/O to complete */
#define MNT_NOWAIT      2       /* start all I/O, but do not wait for it */
#define MNT_LAZY        3       /* push data not written by filesystem syncer */

#define MNT_RDONLY      0x00000001      /* read only filesystem */
#define MNT_SYNCHRONOUS 0x00000002      /* file system written synchronously */
#define MNT_NOEXEC      0x00000004      /* can't exec from filesystem */
#define MNT_NOSUID      0x00000008      /* don't honor setuid bits on fs */
#define MNT_NODEV       0x00000010      /* don't interpret special files */
#define MNT_UNION       0x00000020      /* union with underlying filesystem */
#define MNT_ASYNC       0x00000040      /* file system written asynchronously */
#define MNT_NOCOREDUMP  0x00008000      /* don't write core dumps to this FS */
#define MNT_IGNORE      0x00100000      /* don't show entry in df */
#define MNT_LOG         0x02000000      /* Use logging */
#define MNT_NOATIME     0x04000000      /* Never update access times in fs */
#define MNT_SYMPERM     0x20000000      /* recognize symlink permission */
#define MNT_NODEVMTIME  0x40000000      /* Never update mod times for devs */
#define MNT_SOFTDEP     0x80000000      /* Use soft dependencies */

/* XXX from machine/param.h */
#define DEV_BSHIFT      9               /* log2(DEV_BSIZE) */
#define DEV_BSIZE       (1 << DEV_BSHIFT)


/* FIXME: move?
 * some stuff from sys/dirent.h.
 */
#define DT_UNKNOWN       0
#define DT_FIFO          1
#define DT_CHR           2
#define DT_DIR           4
#define DT_BLK           6
#define DT_REG           8
#define DT_LNK          10
#define DT_SOCK         12
#define DT_WHT          14
/*
 * The _DIRENT_ALIGN macro returns the alignment of struct dirent.
 * struct direct and struct dirent12 used 4 byte alignment but
 * struct dirent uses 8.
 */
/* FIXME: in mfs code sizeof(long) and not d_ino */
#undef _DIRENT_ALIGN
#define _DIRENT_ALIGN(dp) (sizeof((dp)->d_ino) - 1)
/*
 * The _DIRENT_NAMEOFF macro returns the offset of the d_name field in
 * struct dirent
 */
#define _DIRENT_NAMEOFF(dp) \
    ((char *)(void *)&(dp)->d_name - (char *)(void *)dp)
/*
 * The _DIRENT_RECLEN macro gives the minimum record length which will hold
 * a name of size "namlen".  This requires the amount of space in struct dirent
 * without the d_name field, plus enough space for the name with a terminating
 * null byte (namlen+1), rounded up to a the appropriate byte boundary.
 */
/* FIXME */
#undef _DIRENT_RECLEN
#define _DIRENT_RECLEN(dp, namlen) \
    ((_DIRENT_NAMEOFF(dp) + (namlen) + 1 + _DIRENT_ALIGN(dp)) & \
    ~_DIRENT_ALIGN(dp))
/*
 * The _DIRENT_NEXT macro advances to the next dirent record.
 */
#define _DIRENT_NEXT(dp) ((void *)((char *)(void *)(dp) + (dp)->d_reclen))




/* forwards */
struct puffs_cc;

struct puffs_getreq;
struct puffs_cred;
struct puffs_newinfo;

/* paths */
struct puffs_pathobj {
	void 		*po_path;
	size_t		po_len;
	uint32_t	po_hash;
};

/* for prefix rename */
struct puffs_pathinfo {
	struct puffs_pathobj *pi_old;
	struct puffs_pathobj *pi_new;
};

/* describes one segment cached in the kernel */
struct puffs_kcache {
	off_t	pkc_start;
	off_t	pkc_end;

	LIST_ENTRY(puffs_kcache) pkc_entries;
};

/* XXX: might disappear from here into a private header */
struct puffs_node {
	off_t			pn_size;
	int			pn_flags;
	struct vattr		pn_va;

	void			*pn_data;	/* private data		*/

	struct puffs_pathobj	pn_po;		/* PUFFS_FLAG_BUILDPATH */

	struct puffs_usermount 	*pn_mnt;
	LIST_ENTRY(puffs_node)	pn_entries;

	LIST_HEAD(,puffs_kcache)pn_cacheinfo;	/* PUFFS_KFLAG_CACHE	*/

	/* MINIX fields */
	char			pn_mountpoint; /* true if mounted on */
	int 			pn_count;          /* # times inode used */

};
#define PUFFS_NODE_REMOVED	0x01		/* not on entry list	*/


struct puffs_usermount;

/*
 * megaXXX: these are values from inside _KERNEL
 * need to work on the translation for ALL the necessary values.
 */
#define PUFFS_VNOVAL (-1)

#define PUFFS_IO_APPEND 0x020
#define PUFFS_IO_NDELAY	0x100

#define PUFFS_VEXEC	01
#define PUFFS_VWRITE	02
#define PUFFS_VREAD	04

#define PUFFS_FSYNC_DATAONLY 0x0002
#define PUFFS_FSYNC_CACHE    0x0100

/*
 * Magic constants
 */
#define PUFFS_CC_STACKSHIFT_DEFAULT 18

struct puffs_cn {
	struct puffs_kcn	*pcn_pkcnp;	/* kernel input */
	struct puffs_cred	*pcn_cred;	/* cred used for lookup */

	struct puffs_pathobj	pcn_po_full;	/* PUFFS_FLAG_BUILDPATH */
};
#define pcn_nameiop	pcn_pkcnp->pkcn_nameiop
#define pcn_flags	pcn_pkcnp->pkcn_flags
#define pcn_name	pcn_pkcnp->pkcn_name
#define pcn_namelen	pcn_pkcnp->pkcn_namelen
#define pcn_consume	pcn_pkcnp->pkcn_consume

/*
 * Puffs options to mount
 */
/* kernel */
#define	PUFFSMOPT_NAMECACHE	{ "namecache", 1, PUFFS_KFLAG_NOCACHE_NAME, 1 }
#define	PUFFSMOPT_PAGECACHE	{ "pagecache", 1, PUFFS_KFLAG_NOCACHE_PAGE, 1 }
#define	PUFFSMOPT_CACHE		{ "cache", 1, PUFFS_KFLAG_NOCACHE, 1 }
#define PUFFSMOPT_ALLOPS	{ "allops", 0, PUFFS_KFLAG_ALLOPS, 1 }

/* libpuffs */
#define PUFFSMOPT_DUMP		{ "dump", 0, PUFFS_FLAG_OPDUMP, 1 }

#define PUFFSMOPT_STD							\
	PUFFSMOPT_NAMECACHE,						\
	PUFFSMOPT_PAGECACHE,						\
	PUFFSMOPT_CACHE,						\
	PUFFSMOPT_ALLOPS,						\
	PUFFSMOPT_DUMP

extern const struct mntopt puffsmopts[]; /* puffs.c */

/* callbacks for operations */
struct puffs_ops {
	int (*puffs_fs_unmount)(struct puffs_usermount *, int);
	int (*puffs_fs_statvfs)(struct puffs_usermount *, struct statvfs *);
	int (*puffs_fs_sync)(struct puffs_usermount *, int,
	    const struct puffs_cred *);
	int (*puffs_fs_fhtonode)(struct puffs_usermount *, void *, size_t,
	    struct puffs_newinfo *);
	int (*puffs_fs_nodetofh)(struct puffs_usermount *, puffs_cookie_t,
	    void *, size_t *);
	void (*puffs_fs_suspend)(struct puffs_usermount *, int);

	int (*puffs_node_lookup)(struct puffs_usermount *,
	    puffs_cookie_t, struct puffs_newinfo *, const struct puffs_cn *);
	int (*puffs_node_create)(struct puffs_usermount *,
	    puffs_cookie_t, struct puffs_newinfo *, const struct puffs_cn *,
	    const struct vattr *);
	int (*puffs_node_mknod)(struct puffs_usermount *,
	    puffs_cookie_t, struct puffs_newinfo *, const struct puffs_cn *,
	    const struct vattr *);
	int (*puffs_node_open)(struct puffs_usermount *,
	    puffs_cookie_t, int, const struct puffs_cred *);
	int (*puffs_node_close)(struct puffs_usermount *,
	    puffs_cookie_t, int, const struct puffs_cred *);
	int (*puffs_node_access)(struct puffs_usermount *,
	    puffs_cookie_t, int, const struct puffs_cred *);
	int (*puffs_node_getattr)(struct puffs_usermount *,
	    puffs_cookie_t, struct vattr *, const struct puffs_cred *);
	int (*puffs_node_setattr)(struct puffs_usermount *,
	    puffs_cookie_t, const struct vattr *, const struct puffs_cred *);
	int (*puffs_node_poll)(struct puffs_usermount *, puffs_cookie_t, int *);
	int (*puffs_node_mmap)(struct puffs_usermount *,
	    puffs_cookie_t, vm_prot_t, const struct puffs_cred *);
	int (*puffs_node_fsync)(struct puffs_usermount *,
	    puffs_cookie_t, const struct puffs_cred *, int, off_t, off_t);
	int (*puffs_node_seek)(struct puffs_usermount *,
	    puffs_cookie_t, off_t, off_t, const struct puffs_cred *);
	int (*puffs_node_remove)(struct puffs_usermount *,
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *);
	int (*puffs_node_link)(struct puffs_usermount *,
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *);
	int (*puffs_node_rename)(struct puffs_usermount *,
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *,
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *);
	int (*puffs_node_mkdir)(struct puffs_usermount *,
	    puffs_cookie_t, struct puffs_newinfo *, const struct puffs_cn *,
	    const struct vattr *);
	int (*puffs_node_rmdir)(struct puffs_usermount *,
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *);
	int (*puffs_node_symlink)(struct puffs_usermount *,
	    puffs_cookie_t, struct puffs_newinfo *, const struct puffs_cn *,
	    const struct vattr *,
	    const char *);
	int (*puffs_node_readdir)(struct puffs_usermount *,
	    puffs_cookie_t, struct dirent *, off_t *, size_t *,
	    const struct puffs_cred *, int *, off_t *, size_t *);
	int (*puffs_node_readlink)(struct puffs_usermount *,
	    puffs_cookie_t, const struct puffs_cred *, char *, size_t *);
	int (*puffs_node_reclaim)(struct puffs_usermount *, puffs_cookie_t);
	int (*puffs_node_inactive)(struct puffs_usermount *, puffs_cookie_t);
	int (*puffs_node_print)(struct puffs_usermount *, puffs_cookie_t);
	int (*puffs_node_pathconf)(struct puffs_usermount *,
	    puffs_cookie_t, int, int *);
	int (*puffs_node_advlock)(struct puffs_usermount *,
	    puffs_cookie_t, void *, int, struct flock *, int);
	int (*puffs_node_read)(struct puffs_usermount *, puffs_cookie_t,
	    uint8_t *, off_t, size_t *, const struct puffs_cred *, int);
	int (*puffs_node_write)(struct puffs_usermount *, puffs_cookie_t,
	    uint8_t *, off_t, size_t *, const struct puffs_cred *, int);
	int (*puffs_node_abortop)(struct puffs_usermount *, puffs_cookie_t,
	    const struct puffs_cn *);

#if 0
	/* enable next time this structure is changed */
	void *puffs_ops_spare[32];
#endif
};

typedef	int (*pu_pathbuild_fn)(struct puffs_usermount *,
			       const struct puffs_pathobj *,
			       const struct puffs_pathobj *, size_t,
			       struct puffs_pathobj *);
typedef int (*pu_pathtransform_fn)(struct puffs_usermount *,
				   const struct puffs_pathobj *,
				   const struct puffs_cn *,
				   struct puffs_pathobj *);
typedef int (*pu_pathcmp_fn)(struct puffs_usermount *, struct puffs_pathobj *,
			  struct puffs_pathobj *, size_t, int);
typedef void (*pu_pathfree_fn)(struct puffs_usermount *,
			       struct puffs_pathobj *);
typedef int (*pu_namemod_fn)(struct puffs_usermount *,
			     struct puffs_pathobj *, struct puffs_cn *);

typedef void (*pu_errnotify_fn)(struct puffs_usermount *,
				uint8_t, int, const char *, puffs_cookie_t);

typedef void (*pu_prepost_fn)(struct puffs_usermount *);

typedef struct puffs_node *(*pu_cmap_fn)(struct puffs_usermount *,
					 puffs_cookie_t);

enum {
	PUFFS_STATE_BEFOREMOUNT,	PUFFS_STATE_RUNNING,
	PUFFS_STATE_UNMOUNTING,		PUFFS_STATE_UNMOUNTED
};

#define PUFFS_FLAG_BUILDPATH	0x80000000	/* node paths in pnode */
#define PUFFS_FLAG_OPDUMP	0x40000000	/* dump all operations */
#define PUFFS_FLAG_HASHPATH	0x20000000	/* speedup: hash paths */
#define PUFFS_FLAG_MASK		0xe0000000

#define PUFFS_FLAG_KERN(a)	((a) & PUFFS_KFLAG_MASK)
#define PUFFS_FLAG_LIB(a)	((a) & PUFFS_FLAG_MASK)

/* blocking mode argument */
#define PUFFSDEV_BLOCK 0
#define PUFFSDEV_NONBLOCK 1

#define PUFFS_STACKSIZE_DEFAULT (1<<PUFFS_CC_STACKSHIFT_DEFAULT)
#define PUFFS_STACKSIZE_MIN ((size_t)-1)

#define		DENT_DOT	0
#define		DENT_DOTDOT	1
#define		DENT_ADJ(a)	((a)-2)	/* nth request means dir's n-2th */


/*
 * protos
 */

#define PUFFSOP_PROTOS(fsname)						\
	int fsname##_fs_unmount(struct puffs_usermount *, int);		\
	int fsname##_fs_statvfs(struct puffs_usermount *,		\
	    struct statvfs *);						\
	int fsname##_fs_sync(struct puffs_usermount *, int,		\
	    const struct puffs_cred *cred);				\
	int fsname##_fs_fhtonode(struct puffs_usermount *, void *,	\
	    size_t, struct puffs_newinfo *);				\
	int fsname##_fs_nodetofh(struct puffs_usermount *,		\
	    puffs_cookie_t, void *, size_t *);				\
	void fsname##_fs_suspend(struct puffs_usermount *, int);	\
									\
	int fsname##_node_lookup(struct puffs_usermount *,		\
	    puffs_cookie_t, struct puffs_newinfo *,			\
	    const struct puffs_cn *);					\
	int fsname##_node_create(struct puffs_usermount *,		\
	    puffs_cookie_t, struct puffs_newinfo *,			\
	    const struct puffs_cn *, const struct vattr *);		\
	int fsname##_node_mknod(struct puffs_usermount *,		\
	    puffs_cookie_t, struct puffs_newinfo *,			\
	    const struct puffs_cn *, const struct vattr *);		\
	int fsname##_node_open(struct puffs_usermount *,		\
	    puffs_cookie_t, int, const struct puffs_cred *);		\
	int fsname##_node_close(struct puffs_usermount *,		\
	    puffs_cookie_t, int, const struct puffs_cred *);		\
	int fsname##_node_access(struct puffs_usermount *,		\
	    puffs_cookie_t, int, const struct puffs_cred *);		\
	int fsname##_node_getattr(struct puffs_usermount *,		\
	    puffs_cookie_t, struct vattr *, const struct puffs_cred *);	\
	int fsname##_node_setattr(struct puffs_usermount *,		\
	    puffs_cookie_t, const struct vattr *,			\
	    const struct puffs_cred *);					\
	int fsname##_node_poll(struct puffs_usermount *,		\
	    puffs_cookie_t, int *);					\
	int fsname##_node_mmap(struct puffs_usermount *,		\
	    puffs_cookie_t, vm_prot_t, const struct puffs_cred *);	\
	int fsname##_node_fsync(struct puffs_usermount *,		\
	    puffs_cookie_t, const struct puffs_cred *, int,		\
	    off_t, off_t);						\
	int fsname##_node_seek(struct puffs_usermount *,		\
	    puffs_cookie_t, off_t, off_t, const struct puffs_cred *);	\
	int fsname##_node_remove(struct puffs_usermount *,		\
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *);	\
	int fsname##_node_link(struct puffs_usermount *,		\
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *);	\
	int fsname##_node_rename(struct puffs_usermount *,		\
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *,	\
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *);	\
	int fsname##_node_mkdir(struct puffs_usermount *,		\
	    puffs_cookie_t, struct puffs_newinfo *,			\
	    const struct puffs_cn *, const struct vattr *);		\
	int fsname##_node_rmdir(struct puffs_usermount *,		\
	    puffs_cookie_t, puffs_cookie_t, const struct puffs_cn *);	\
	int fsname##_node_symlink(struct puffs_usermount *,		\
	    puffs_cookie_t, struct puffs_newinfo *,			\
	    const struct puffs_cn *, const struct vattr *,		\
	    const char *);						\
	int fsname##_node_readdir(struct puffs_usermount *,		\
	    puffs_cookie_t, struct dirent *, off_t *, size_t *,		\
	    const struct puffs_cred *, int *, off_t *, size_t *);	\
	int fsname##_node_readlink(struct puffs_usermount *,		\
	    puffs_cookie_t, const struct puffs_cred *, char *,		\
	    size_t *);							\
	int fsname##_node_reclaim(struct puffs_usermount *,		\
	    puffs_cookie_t);						\
	int fsname##_node_inactive(struct puffs_usermount *,		\
	    puffs_cookie_t);						\
	int fsname##_node_print(struct puffs_usermount *,		\
	    puffs_cookie_t);						\
	int fsname##_node_pathconf(struct puffs_usermount *,		\
	    puffs_cookie_t, int, int *);				\
	int fsname##_node_advlock(struct puffs_usermount *,		\
	    puffs_cookie_t, void *, int, struct flock *, int);		\
	int fsname##_node_read(struct puffs_usermount *, puffs_cookie_t,\
	    uint8_t *, off_t, size_t *, const struct puffs_cred *, int);\
	int fsname##_node_write(struct puffs_usermount *,		\
	    puffs_cookie_t, uint8_t *, off_t, size_t *,			\
	    const struct puffs_cred *, int);				\
	int fsname##_node_abortop(struct puffs_usermount *,		\
	    puffs_cookie_t, const struct puffs_cn *);

#define PUFFSOP_INIT(ops)						\
    ops = malloc(sizeof(struct puffs_ops));				\
    memset(ops, 0, sizeof(struct puffs_ops))
#define PUFFSOP_SET(ops, fsname, fsornode, opname)			\
    (ops)->puffs_##fsornode##_##opname = fsname##_##fsornode##_##opname
#define PUFFSOP_SETFSNOP(ops, opname)					\
    (ops)->puffs_fs_##opname = puffs_fsnop_##opname

PUFFSOP_PROTOS(puffs_null)	/* XXX */

#define PUFFS_DEVEL_LIBVERSION 34
#define puffs_init(a,b,c,d,e) \
    _puffs_init(PUFFS_DEVEL_LIBVERSION,a,b,c,d,e)


#define PNPATH(pnode)	((pnode)->pn_po.po_path)
#define PNPLEN(pnode)	((pnode)->pn_po.po_len)
#define PCNPATH(pcnode)	((pcnode)->pcn_po_full.po_path)
#define PCNPLEN(pcnode)	((pcnode)->pcn_po_full.po_len)
#define PCNISDOTDOT(pcnode) \
	((pcnode)->pcn_namelen == 2 && strcmp((pcnode)->pcn_name, "..") == 0)

#define PUFFS_STORE_DCOOKIE(cp, ncp, off)				\
if (cp)	{								\
	*((cp)++) = off;						\
	(*(ncp))++;							\
}

/* mainloop */
typedef void (*puffs_ml_loop_fn)(struct puffs_usermount *);

/* framebuf stuff */
struct puffs_framebuf;
typedef int (*puffs_framev_readframe_fn)(struct puffs_usermount *,
					   struct puffs_framebuf *,
					   int, int *);
typedef	int (*puffs_framev_writeframe_fn)(struct puffs_usermount *,
					    struct puffs_framebuf *,
					    int, int *);
typedef int (*puffs_framev_cmpframe_fn)(struct puffs_usermount *,
					 struct puffs_framebuf *,
					 struct puffs_framebuf *,
					 int *);
typedef void (*puffs_framev_fdnotify_fn)(struct puffs_usermount *, int, int);
typedef void (*puffs_framev_gotframe_fn)(struct puffs_usermount *,
					struct puffs_framebuf *);
typedef void (*puffs_framev_cb)(struct puffs_usermount *,
				struct puffs_framebuf *,
				void *, int);
#define PUFFS_FBIO_READ		0x01
#define PUFFS_FBIO_WRITE	0x02
#define PUFFS_FBIO_ERROR	0x04

#define PUFFS_FBQUEUE_URGENT	0x01


__BEGIN_DECLS

#define PUFFS_DEFER ((void *)-1)
struct puffs_usermount *_puffs_init(int, struct puffs_ops *, const char *,
				    const char *, void *, uint32_t);
int		puffs_mount(struct puffs_usermount *, const char *, int, void*);
int		puffs_exit(struct puffs_usermount *, int);
void		puffs_cancel(struct puffs_usermount *, int);
int		puffs_mainloop(struct puffs_usermount *);


int	puffs_getstate(struct puffs_usermount *);
void	puffs_setstacksize(struct puffs_usermount *, size_t);

void	puffs_ml_setloopfn(struct puffs_usermount *, puffs_ml_loop_fn);
void	puffs_ml_settimeout(struct puffs_usermount *, struct timespec *);

void			puffs_setroot(struct puffs_usermount *,
				      struct puffs_node *);
struct puffs_node 	*puffs_getroot(struct puffs_usermount *);
void			puffs_setrootinfo(struct puffs_usermount *,
					  enum vtype, vsize_t, dev_t); 

void			*puffs_getspecific(struct puffs_usermount *);
void			puffs_setspecific(struct puffs_usermount *, void *);
void			puffs_setmaxreqlen(struct puffs_usermount *, size_t);
size_t			puffs_getmaxreqlen(struct puffs_usermount *);
void			puffs_setfhsize(struct puffs_usermount *, size_t, int);
void			puffs_setmntinfo(struct puffs_usermount *,
					 const char *, const char *);

void			puffs_setncookiehash(struct puffs_usermount *, int);

struct puffs_pathobj	*puffs_getrootpathobj(struct puffs_usermount *);

struct puffs_node	*puffs_pn_new(struct puffs_usermount *, void *);
void			puffs_pn_remove(struct puffs_node *);
void			puffs_pn_put(struct puffs_node *);
struct vattr		*puffs_pn_getvap(struct puffs_node *);
void *			puffs_pn_getpriv(struct puffs_node *);
void			puffs_pn_setpriv(struct puffs_node *, void *);
struct puffs_pathobj	*puffs_pn_getpo(struct puffs_node *);
struct puffs_usermount	*puffs_pn_getmnt(struct puffs_node *);

void	puffs_newinfo_setcookie(struct puffs_newinfo *, puffs_cookie_t);
void	puffs_newinfo_setvtype(struct puffs_newinfo *, enum vtype);
void	puffs_newinfo_setsize(struct puffs_newinfo *, voff_t);
void	puffs_newinfo_setrdev(struct puffs_newinfo *, dev_t);

void			*puffs_pn_getmntspecific(struct puffs_node *);

typedef		void *	(*puffs_nodewalk_fn)(struct puffs_usermount *,
					     struct puffs_node *, void *);
void			*puffs_pn_nodewalk(struct puffs_usermount *,
					   puffs_nodewalk_fn, void *);
void			*puffs_pn_nodeprint(struct puffs_usermount *pu,
					   struct puffs_node *pn, void *arg);
void			puffs_pn_nodeprintall(struct puffs_usermount *pu);

void			puffs_setvattr(struct vattr *, const struct vattr *);
void			puffs_vattr_null(struct vattr *);

void			puffs_null_setops(struct puffs_ops *);

int			puffs_dispatch_create(struct puffs_usermount *,
					      struct puffs_framebuf *,
					      struct puffs_cc **);
int			puffs_dispatch_exec(struct puffs_cc *,
					    struct puffs_framebuf **);

/*
 * generic/dummy routines applicable for some file systems
 */
int  puffs_fsnop_unmount(struct puffs_usermount *, int);
int  puffs_fsnop_statvfs(struct puffs_usermount *, struct statvfs *);
void puffs_zerostatvfs(struct statvfs *);
int  puffs_fsnop_sync(struct puffs_usermount *, int waitfor,
		      const struct puffs_cred *);

int  puffs_genfs_node_getattr(struct puffs_usermount *, puffs_cookie_t,
			      struct vattr *, const struct puffs_cred *);
int  puffs_genfs_node_reclaim(struct puffs_usermount *, puffs_cookie_t);

/*
 * Subroutine stuff
 */

int		puffs_gendotdent(struct dirent **, ino_t, int, size_t *);
int		puffs_nextdent(struct dirent **, const char *, ino_t,
			       uint8_t, size_t *);
int		puffs_vtype2dt(enum vtype);
enum vtype	puffs_mode2vt(mode_t);
void		puffs_stat2vattr(struct vattr *va, const struct stat *);
mode_t		puffs_addvtype2mode(mode_t, enum vtype);


/*
 * credentials & permissions
 */

/* Credential fetch */
int	puffs_cred_getuid(const struct puffs_cred *, uid_t *);
int	puffs_cred_getgid(const struct puffs_cred *, gid_t *);
int	puffs_cred_getgroups(const struct puffs_cred *, gid_t *, short *);

/* Credential check */
bool	puffs_cred_isuid(const struct puffs_cred *, uid_t);
bool	puffs_cred_hasgroup(const struct puffs_cred *, gid_t);
bool	puffs_cred_isregular(const struct puffs_cred *);
bool	puffs_cred_iskernel(const struct puffs_cred *);
bool	puffs_cred_isfs(const struct puffs_cred *);
bool	puffs_cred_isjuggernaut(const struct puffs_cred *);

/* misc */
int	puffs_access(enum vtype, mode_t, uid_t, gid_t, mode_t,
		     const struct puffs_cred *);
int	puffs_access_chown(uid_t, gid_t, uid_t, gid_t,
			   const struct puffs_cred *);
int	puffs_access_chmod(uid_t, gid_t, enum vtype, mode_t,
			   const struct puffs_cred *);
int	puffs_access_times(uid_t, gid_t, mode_t, int,
			   const struct puffs_cred *);


/*
 * Call Context interfaces relevant for user.
 */

void			puffs_cc_yield(struct puffs_cc *);
void			puffs_cc_continue(struct puffs_cc *);
void			puffs_cc_schedule(struct puffs_cc *);
int			puffs_cc_getcaller(struct puffs_cc *,pid_t *,lwpid_t *);
struct puffs_cc		*puffs_cc_getcc(struct puffs_usermount *);

/*
 * Flushing / invalidation routines
 */

int	puffs_inval_namecache_dir(struct puffs_usermount *, puffs_cookie_t);
int	puffs_inval_namecache_all(struct puffs_usermount *);

int	puffs_inval_pagecache_node(struct puffs_usermount *, puffs_cookie_t);
int	puffs_inval_pagecache_node_range(struct puffs_usermount *,
					 puffs_cookie_t, off_t, off_t);
int	puffs_flush_pagecache_node(struct puffs_usermount *, puffs_cookie_t);
int	puffs_flush_pagecache_node_range(struct puffs_usermount *,
					 puffs_cookie_t, off_t, off_t);

/*
 * Path constructicons
 */

int	puffs_stdpath_buildpath(struct puffs_usermount *,
			     const struct puffs_pathobj *,
			     const struct puffs_pathobj *, size_t,
			     struct puffs_pathobj *);
int	puffs_stdpath_cmppath(struct puffs_usermount *, struct puffs_pathobj *,
			   struct puffs_pathobj *, size_t, int);
void	puffs_stdpath_freepath(struct puffs_usermount *,struct puffs_pathobj *);

void	*puffs_path_walkcmp(struct puffs_usermount *,
			    struct puffs_node *, void *);
void	*puffs_path_prefixadj(struct puffs_usermount *,
			      struct puffs_node *, void *);
int	puffs_path_pcnbuild(struct puffs_usermount *,
			    struct puffs_cn *, void *);
void	puffs_path_buildhash(struct puffs_usermount *, struct puffs_pathobj *);
void	puffs_set_pathbuild(struct puffs_usermount *, pu_pathbuild_fn); void	puffs_set_pathtransform(struct puffs_usermount *, pu_pathtransform_fn);
void	puffs_set_pathcmp(struct puffs_usermount *, pu_pathcmp_fn);
void	puffs_set_pathfree(struct puffs_usermount *, pu_pathfree_fn);
void	puffs_set_namemod(struct puffs_usermount *, pu_namemod_fn);

void	puffs_set_errnotify(struct puffs_usermount *, pu_errnotify_fn);
void	puffs_set_prepost(struct puffs_usermount *,
			  pu_prepost_fn, pu_prepost_fn);
void	puffs_set_cmap(struct puffs_usermount *, pu_cmap_fn);

/*
 * Suspension
 */

int	puffs_fs_suspend(struct puffs_usermount *);

/*
 * Frame buffering
 */

void	puffs_framev_init(struct puffs_usermount *,
			  puffs_framev_readframe_fn,
			  puffs_framev_writeframe_fn,
			  puffs_framev_cmpframe_fn,
			  puffs_framev_gotframe_fn,
			  puffs_framev_fdnotify_fn);

struct puffs_framebuf 	*puffs_framebuf_make(void);
void			puffs_framebuf_destroy(struct puffs_framebuf *);
int			puffs_framebuf_dup(struct puffs_framebuf *,
					   struct puffs_framebuf **);
void			puffs_framebuf_recycle(struct puffs_framebuf *);
int			puffs_framebuf_reserve_space(struct puffs_framebuf *,
						     size_t);

int	puffs_framebuf_putdata(struct puffs_framebuf *, const void *, size_t);
int	puffs_framebuf_putdata_atoff(struct puffs_framebuf *, size_t,
				     const void *, size_t);
int	puffs_framebuf_getdata(struct puffs_framebuf *, void *, size_t);
int	puffs_framebuf_getdata_atoff(struct puffs_framebuf *, size_t,
				     void *, size_t);

size_t	puffs_framebuf_telloff(struct puffs_framebuf *);
size_t	puffs_framebuf_tellsize(struct puffs_framebuf *);
size_t	puffs_framebuf_remaining(struct puffs_framebuf *);
int	puffs_framebuf_seekset(struct puffs_framebuf *, size_t);
int	puffs_framebuf_getwindow(struct puffs_framebuf *, size_t,
				 void **, size_t *);

int	puffs_framev_enqueue_cc(struct puffs_cc *, int,
				struct puffs_framebuf *, int);
int	puffs_framev_enqueue_cb(struct puffs_usermount *, int,
				struct puffs_framebuf *,
				puffs_framev_cb, void *, int);
int	puffs_framev_enqueue_justsend(struct puffs_usermount *, int,
				      struct puffs_framebuf *, int, int);
int	puffs_framev_enqueue_directreceive(struct puffs_cc *, int,
					   struct puffs_framebuf *, int);
int	puffs_framev_enqueue_directsend(struct puffs_cc *, int,
					   struct puffs_framebuf *, int);
int	puffs_framev_enqueue_waitevent(struct puffs_cc *, int, int *);
int	puffs_framev_framebuf_ccpromote(struct puffs_framebuf *,
					struct puffs_cc *);

int	puffs_framev_addfd(struct puffs_usermount *, int, int);
int	puffs_framev_enablefd(struct puffs_usermount *, int, int);
int	puffs_framev_disablefd(struct puffs_usermount *, int, int);
int	puffs_framev_removefd(struct puffs_usermount *, int, int);
void	puffs_framev_removeonclose(struct puffs_usermount *, int, int);
void	puffs_framev_unmountonclose(struct puffs_usermount *, int, int);

__END_DECLS

#endif /* _PUFFS_H_ */
