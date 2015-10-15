/*	$NetBSD: puffs_sys.h,v 1.89 2015/02/15 20:21:29 manu Exp $	*/

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

#ifndef _PUFFS_SYS_H_
#define _PUFFS_SYS_H_

#include <sys/param.h>
#include <sys/select.h>
#include <sys/kauth.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/pool.h>

#include <fs/puffs/puffs_msgif.h>

#include <miscfs/genfs/genfs_node.h>

extern int (**puffs_vnodeop_p)(void *);
extern int (**puffs_specop_p)(void *);
extern int (**puffs_fifoop_p)(void *);

extern const struct vnodeopv_desc puffs_vnodeop_opv_desc;
extern const struct vnodeopv_desc puffs_specop_opv_desc;
extern const struct vnodeopv_desc puffs_fifoop_opv_desc;
extern const struct vnodeopv_desc puffs_msgop_opv_desc;

extern struct pool puffs_pnpool;
extern struct pool puffs_vapool;

#ifdef DEBUG
#ifndef PUFFSDEBUG
#define PUFFSDEBUG
#endif
#endif

#ifdef PUFFSDEBUG
extern int puffsdebug; /* puffs_subr.c */
#define DPRINTF(x) do { \
		if (puffsdebug > 0) printf x; \
	} while (/*CONSTCOND*/0)
#define DPRINTF_VERBOSE(x) do { \
		if (puffsdebug > 1) printf x; \
	} while (/*CONSTCOND*/0)
#else
#define DPRINTF(x) ((void)0)
#define DPRINTF_VERBOSE(x) ((void)0)
#endif

#define MPTOPUFFSMP(mp) ((struct puffs_mount *)((mp)->mnt_data))
#define PMPTOMP(pmp) (pmp->pmp_mp)
#define VPTOPP(vp) ((struct puffs_node *)(vp)->v_data)
#define VPTOPNC(vp) (((struct puffs_node *)(vp)->v_data)->pn_cookie)
#define VPTOPUFFSMP(vp) ((struct puffs_mount*)((struct puffs_node*)vp->v_data))

/* we don't pass the kernel overlay to userspace */
#define PUFFS_TOFHSIZE(s) ((s)==0 ? (s) : (s)+4)
#define PUFFS_FROMFHSIZE(s) ((s)==0 ? (s) : (s)-4)

#define ALLOPS(pmp) (pmp->pmp_flags & PUFFS_KFLAG_ALLOPS)
#define EXISTSOP(pmp, op) \
 (ALLOPS(pmp) || ((pmp)->pmp_vnopmask[PUFFS_VN_##op]))

#define PUFFS_USE_NAMECACHE(pmp)	\
    (((pmp)->pmp_flags & PUFFS_KFLAG_NOCACHE_NAME) == 0)
#define PUFFS_USE_PAGECACHE(pmp)	\
    (((pmp)->pmp_flags & PUFFS_KFLAG_NOCACHE_PAGE) == 0)
#define PUFFS_USE_FULLPNBUF(pmp)	\
    ((pmp)->pmp_flags & PUFFS_KFLAG_LOOKUP_FULLPNBUF)
#define PUFFS_USE_FS_TTL(pmp)	\
    ((pmp)->pmp_flags & PUFFS_KFLAG_CACHE_FS_TTL)
#define PUFFS_USE_DOTDOTCACHE(pmp)	\
    ((pmp)->pmp_flags & PUFFS_KFLAG_CACHE_DOTDOT)
#define PUFFS_USE_METAFLUSH(pmp)	\
    (((pmp)->pmp_flags & PUFFS_KFLAG_NOFLUSH_META) == 0)

#define PUFFS_WCACHEINFO(pmp)	(__USE(pmp), 0)

struct puffs_newcookie {
	puffs_cookie_t	pnc_cookie;

	LIST_ENTRY(puffs_newcookie) pnc_entries;
};

#define PUFFS_SOPREQ_EXPIRE_TIMEOUT 1000
extern int puffs_sopreq_expire_timeout;

enum puffs_sopreqtype {
	PUFFS_SOPREQSYS_EXIT,
	PUFFS_SOPREQ_FLUSH,
	PUFFS_SOPREQ_UNMOUNT,
	PUFFS_SOPREQ_EXPIRE,
};

struct puffs_sopreq {
	union {
		struct puffs_req preq;
		struct puffs_flush pf;
		puffs_cookie_t ck;
	} psopr_u;

	enum puffs_sopreqtype psopr_sopreq;
	TAILQ_ENTRY(puffs_sopreq) psopr_entries;
	int psopr_at;
};
#define psopr_preq psopr_u.preq
#define psopr_pf psopr_u.pf
#define psopr_ck psopr_u.ck

TAILQ_HEAD(puffs_wq, puffs_msgpark);
LIST_HEAD(puffs_node_hashlist, puffs_node);
struct puffs_mount {
	kmutex_t	 		pmp_lock;

	struct puffs_kargs		pmp_args;
#define pmp_flags pmp_args.pa_flags
#define pmp_vnopmask pmp_args.pa_vnopmask

	struct puffs_wq			pmp_msg_touser;
	int				pmp_msg_touser_count;
	kcondvar_t			pmp_msg_waiter_cv;
	size_t				pmp_msg_maxsize;

	struct puffs_wq			pmp_msg_replywait;

	struct mount			*pmp_mp;

	struct vnode			*pmp_root;
	puffs_cookie_t			pmp_root_cookie;
	enum vtype			pmp_root_vtype;
	vsize_t				pmp_root_vsize;
	dev_t				pmp_root_rdev;

	struct putter_instance		*pmp_pi;

	unsigned int			pmp_refcount;
	kcondvar_t			pmp_refcount_cv;

	kcondvar_t			pmp_unmounting_cv;
	uint8_t				pmp_unmounting;

	uint8_t				pmp_status;
	uint8_t				pmp_suspend;

	uint8_t				*pmp_curput;
	size_t				pmp_curres;
	void				*pmp_curopaq;

	uint64_t			pmp_nextmsgid;

	kmutex_t			pmp_sopmtx;
	kcondvar_t			pmp_sopcv;
	int				pmp_sopthrcount;
	TAILQ_HEAD(, puffs_sopreq)	pmp_sopfastreqs;
	TAILQ_HEAD(, puffs_sopreq)	pmp_sopnodereqs;
	bool				pmp_docompat;
};

#define PUFFSTAT_BEFOREINIT	0
#define PUFFSTAT_MOUNTING	1
#define PUFFSTAT_RUNNING	2
#define PUFFSTAT_DYING		3 /* Do you want your possessions identified? */


#define PNODE_NOREFS	0x001	/* no backend reference			*/
#define PNODE_DYING	0x002	/* NOREFS + inactive			*/
#define PNODE_FAF	0x004	/* issue all operations as FAF		*/
#define PNODE_DOINACT 	0x008	/* if inactive-on-demand, call inactive */
#define PNODE_SOPEXP	0x100	/* Node reclaim postponed in sop thread	*/
#define PNODE_RDIRECT	0x200	/* bypass page cache on read		*/
#define PNODE_WDIRECT	0x400	/* bypass page cache on write		*/

#define PNODE_METACACHE_ATIME	0x10	/* cache atime metadata */
#define PNODE_METACACHE_CTIME	0x20	/* cache atime metadata */
#define PNODE_METACACHE_MTIME	0x40	/* cache atime metadata */
#define PNODE_METACACHE_SIZE	0x80	/* cache atime metadata */
#define PNODE_METACACHE_MASK	0xf0

struct puffs_node {
	struct genfs_node pn_gnode;	/* genfs glue			*/

	kmutex_t	pn_mtx;
	int		pn_refcount;
	int		pn_nlookup;

	puffs_cookie_t	pn_cookie;	/* userspace pnode cookie	*/
	struct vnode	*pn_vp;		/* backpointer to vnode		*/
	uint32_t	pn_stat;	/* node status			*/

	struct selinfo	pn_sel;		/* for selecting on the node	*/
	short		pn_revents;	/* available events		*/

	/* metacache */
	struct timespec	pn_mc_atime;
	struct timespec	pn_mc_ctime;
	struct timespec	pn_mc_mtime;
	u_quad_t	pn_mc_size;

	voff_t		pn_serversize;

	struct lockf *	pn_lockf;

	kmutex_t	pn_sizemtx;	/* size modification mutex	*/
	
	int		pn_cn_timeout;	/* path cache */
	int		pn_cn_grace;	/* grace time before reclaim */
	int		pn_va_timeout;	/* attribute cache */
	struct vattr *	pn_va_cache;	/* attribute cache */
	struct vnode *  pn_parent;	/* parent cache */
};

typedef void (*parkdone_fn)(struct puffs_mount *, struct puffs_req *, void *);

struct puffs_msgpark;
void	puffs_msgif_init(void);
void	puffs_msgif_destroy(void);
int	puffs_msgmem_alloc(size_t, struct puffs_msgpark **, void **, int);
void	puffs_msgmem_release(struct puffs_msgpark *);

void	puffs_sop_thread(void *);

void	puffs_msg_setfaf(struct puffs_msgpark *);
void	puffs_msg_setdelta(struct puffs_msgpark *, size_t);
void	puffs_msg_setinfo(struct puffs_msgpark *, int, int, puffs_cookie_t);
void	puffs_msg_setcall(struct puffs_msgpark *, parkdone_fn, void *);

void	puffs_msg_enqueue(struct puffs_mount *, struct puffs_msgpark *);
int	puffs_msg_wait(struct puffs_mount *, struct puffs_msgpark *);
int	puffs_msg_wait2(struct puffs_mount *, struct puffs_msgpark *,
			struct puffs_node *, struct puffs_node *);

void	puffs_msg_sendresp(struct puffs_mount *, struct puffs_req *, int);

int	puffs_getvnode(struct mount *, puffs_cookie_t, enum vtype,
		       voff_t, dev_t, struct vnode **);
int	puffs_newnode(struct mount *, struct vnode *, struct vnode **,
		      puffs_cookie_t, struct componentname *,
		      enum vtype, dev_t);
void	puffs_putvnode(struct vnode *);

void	puffs_releasenode(struct puffs_node *);
void	puffs_referencenode(struct puffs_node *);

#define PUFFS_NOSUCHCOOKIE (-1)
int	puffs_cookie2vnode(struct puffs_mount *, puffs_cookie_t,
			   struct vnode **);
void	puffs_makecn(struct puffs_kcn *, struct puffs_kcred *,
		     const struct componentname *, int);
void	puffs_credcvt(struct puffs_kcred *, kauth_cred_t);

void	puffs_parkdone_asyncbioread(struct puffs_mount *,
				    struct puffs_req *, void *);
void	puffs_parkdone_asyncbiowrite(struct puffs_mount *,
				     struct puffs_req *, void *);
void	puffs_parkdone_poll(struct puffs_mount *, struct puffs_req *, void *);

void	puffs_mp_reference(struct puffs_mount *);
void	puffs_mp_release(struct puffs_mount *);

void	puffs_gop_size(struct vnode *, off_t, off_t *, int); 
void	puffs_gop_markupdate(struct vnode *, int);

void	puffs_senderr(struct puffs_mount *, int, int, const char *,
		      puffs_cookie_t);

bool	puffs_compat_outgoing(struct puffs_req *, struct puffs_req**, ssize_t*);
void	puffs_compat_incoming(struct puffs_req *, struct puffs_req *);

void	puffs_updatenode(struct puffs_node *, int, voff_t);
#define PUFFS_UPDATEATIME	0x01
#define PUFFS_UPDATECTIME	0x02
#define PUFFS_UPDATEMTIME	0x04
#define PUFFS_UPDATESIZE	0x08

void	puffs_userdead(struct puffs_mount *);

extern int (**puffs_vnodeop_p)(void *);

/* for putter */
int	puffs_msgif_getout(void *, size_t, int, uint8_t **, size_t *, void **);
void	puffs_msgif_releaseout(void *, void *, int);
int	puffs_msgif_dispatch(void *, struct putter_hdr *);
size_t	puffs_msgif_waitcount(void *);
int	puffs_msgif_close(void *);

static __inline int
checkerr(struct puffs_mount *pmp, int error, const char *str)
{

	if (error < 0 || error > ELAST) {
		puffs_senderr(pmp, PUFFS_ERR_ERROR, error, str, NULL);
		error = EPROTO;
	}

	return error;
}

#define PUFFS_MSG_VARS(type, a)						\
	struct puffs_##type##msg_##a *a##_msg;				\
	struct puffs_msgpark *park_##a = NULL

#define PUFFS_MSG_ALLOC(type, a)					\
	puffs_msgmem_alloc(sizeof(struct puffs_##type##msg_##a),	\
	    &park_##a, (void *)& a##_msg, 1)

#define PUFFS_MSG_RELEASE(a) 						\
do {									\
	if (park_##a) puffs_msgmem_release(park_##a);			\
} while (/*CONSTCOND*/0)

#define PUFFS_MSG_ENQUEUEWAIT_NOERROR(pmp, park)			\
do {									\
	puffs_msg_enqueue(pmp, park);					\
	puffs_msg_wait(pmp, park);					\
} while (/*CONSTCOND*/0)

#define PUFFS_MSG_ENQUEUEWAIT2_NOERROR(pmp, park, vp1, vp2)		\
do {									\
	puffs_msg_enqueue(pmp, park);					\
	puffs_msg_wait2(pmp, park, vp1, vp2);				\
} while (/*CONSTCOND*/0)

#define PUFFS_MSG_ENQUEUEWAIT(pmp, park, var)				\
do {									\
	puffs_msg_enqueue(pmp, park);					\
	var = puffs_msg_wait(pmp, park);				\
} while (/*CONSTCOND*/0)

#define PUFFS_MSG_ENQUEUEWAIT2(pmp, park, vp1, vp2, var)		\
do {									\
	puffs_msg_enqueue(pmp, park);					\
	var = puffs_msg_wait2(pmp, park, vp1, vp2);			\
} while (/*CONSTCOND*/0)

#endif /* _PUFFS_SYS_H_ */
