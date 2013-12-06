/*	$NetBSD: puffs_priv.h,v 1.45 2012/04/18 00:57:22 manu Exp $	*/

/*
 * Copyright (c) 2006, 2007, 2008 Antti Kantee.  All Rights Reserved.
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

#ifndef _PUFFS_PRIVATE_H_
#define _PUFFS_PRIVATE_H_

#include <sys/types.h>
#include <fs/puffs/puffs_msgif.h>

#if !defined(__minix)
#include <pthread.h>
#endif /* !defined(__minix) */
#include <puffs.h>
#include <ucontext.h>

#if defined(__minix)

/* XXX: MINIX */
#define IGN_PERM            0
#define CHK_PERM            1
#define SU_UID          ((uid_t) 0)     /* super_user's uid_t */

/* XXX: MINIX */
#define ATIME            002    /* set if atime field needs updating */
#define CTIME            004    /* set if ctime field needs updating */
#define MTIME            010    /* set if mtime field needs updating */

#define REQ_READ_SUPER   28

#define NUL(str,l,m) mfs_nul_f(__FILE__,__LINE__,(str), (l), (m))

#else
extern pthread_mutex_t pu_lock;
#define PU_LOCK() pthread_mutex_lock(&pu_lock)
#define PU_UNLOCK() pthread_mutex_unlock(&pu_lock)
#endif /* defined(__minix) */
#if defined(__minix)
#define PU_LOCK() /* nothing */
#define PU_UNLOCK()  /* nothing */
#endif /* defined(__minix) */

#define PU_CMAP(pu, c) (pu->pu_cmap ? pu->pu_cmap(pu,c) : (struct puffs_node*)c)

struct puffs_framectrl {
	puffs_framev_readframe_fn rfb;
	puffs_framev_writeframe_fn wfb;
	puffs_framev_cmpframe_fn cmpfb;
	puffs_framev_gotframe_fn gotfb;
	puffs_framev_fdnotify_fn fdnotfn;
};

struct puffs_fctrl_io {
	struct puffs_framectrl *fctrl;

	int io_fd;
	int stat;

	int rwait;
	int wwait;

	struct puffs_framebuf *cur_in;

	TAILQ_HEAD(, puffs_framebuf) snd_qing;	/* queueing to be sent */
	TAILQ_HEAD(, puffs_framebuf) res_qing;	/* q'ing for rescue */
	LIST_HEAD(, puffs_fbevent) ev_qing;	/* q'ing for events */

	LIST_ENTRY(puffs_fctrl_io) fio_entries;
};
#define FIO_WR		0x01
#define FIO_WRGONE	0x02
#define FIO_RDGONE	0x04
#define FIO_DEAD	0x08
#define FIO_ENABLE_R	0x10
#define FIO_ENABLE_W	0x20

#define FIO_EN_WRITE(fio)				\
    (!(fio->stat & FIO_WR)				\
      && ((!TAILQ_EMPTY(&fio->snd_qing)			\
            && (fio->stat & FIO_ENABLE_W))		\
         || fio->wwait))

#define FIO_RM_WRITE(fio)			\
    ((fio->stat & FIO_WR)			\
      && (((TAILQ_EMPTY(&fio->snd_qing)		\
        || (fio->stat & FIO_ENABLE_W) == 0))	\
	&& (fio->wwait == 0)))


/*
 * usermount: describes one file system instance
 */
struct puffs_usermount {
	struct puffs_ops	pu_ops;

	int			pu_fd;
	size_t			pu_maxreqlen;

	uint32_t		pu_flags;
	int			pu_cc_stackshift;

	ucontext_t		pu_mainctx;
#define PUFFS_CCMAXSTORE 32
	int			pu_cc_nstored;

	int			pu_kq;
	int			pu_state;
#define PU_STATEMASK	0x00ff
#define PU_INLOOP	0x0100
#define PU_ASYNCFD	0x0200
#define PU_HASKQ	0x0400
#define PU_PUFFSDAEMON	0x0800
#define PU_MAINRESTORE	0x1000
#define PU_DONEXIT	0x2000
#define PU_SETSTATE(pu, s) (pu->pu_state = (s) | (pu->pu_state & ~PU_STATEMASK))
#define PU_SETSFLAG(pu, s) (pu->pu_state |= (s))
#define PU_CLRSFLAG(pu, s) \
    (pu->pu_state = ((pu->pu_state & ~(s)) | (pu->pu_state & PU_STATEMASK)))
	int			pu_dpipe[2];

	struct puffs_node	*pu_pn_root;

	LIST_HEAD(, puffs_node)	pu_pnodelst;
#if defined(__minix) // LSC TO KEEP??
	LIST_HEAD(, puffs_node)	pu_pnode_removed_lst;
#endif /* defined(__minix) */

	LIST_HEAD(, puffs_cc)	pu_ccmagazin;
	TAILQ_HEAD(, puffs_cc)	pu_lazyctx;
	TAILQ_HEAD(, puffs_cc)	pu_sched;

	pu_cmap_fn		pu_cmap;

	pu_pathbuild_fn		pu_pathbuild;
	pu_pathtransform_fn	pu_pathtransform;
	pu_pathcmp_fn		pu_pathcmp;
	pu_pathfree_fn		pu_pathfree;
	pu_namemod_fn		pu_namemod;

	pu_errnotify_fn		pu_errnotify;

	pu_prepost_fn		pu_oppre;
	pu_prepost_fn		pu_oppost;

	struct puffs_framectrl	pu_framectrl[2];
#define PU_FRAMECTRL_FS   0
#define PU_FRAMECTRL_USER 1
	LIST_HEAD(, puffs_fctrl_io) pu_ios;
	LIST_HEAD(, puffs_fctrl_io) pu_ios_rmlist;
	struct kevent		*pu_evs;
	size_t			pu_nevs;

	puffs_ml_loop_fn	pu_ml_lfn;
	struct timespec		pu_ml_timeout;
	struct timespec		*pu_ml_timep;

	struct puffs_kargs	*pu_kargp;

	uint64_t		pu_nextreq;
	void			*pu_privdata;
};

/* call context */

struct puffs_cc;
typedef void (*puffs_ccfunc)(struct puffs_cc *);

struct puffs_cc {
	struct puffs_usermount	*pcc_pu;
	struct puffs_framebuf	*pcc_pb;

	/* real cc */
	union {
		struct {
			ucontext_t	uc;		/* "continue"	*/
			ucontext_t	uc_ret;		/* "yield" 	*/
		} real;
		struct {
			puffs_ccfunc	func;
			void		*farg;
		} fake;
	} pcc_u;

	pid_t			pcc_pid;
	lwpid_t			pcc_lid;

	int			pcc_flags;

	TAILQ_ENTRY(puffs_cc)	pcc_schedent;
	LIST_ENTRY(puffs_cc)	pcc_rope;
};
#define pcc_uc		pcc_u.real.uc
#define pcc_uc_ret 	pcc_u.real.uc_ret
#define pcc_func	pcc_u.fake.func
#define pcc_farg	pcc_u.fake.farg
#define PCC_DONE	0x01
#define PCC_BORROWED	0x02
#define PCC_HASCALLER	0x04
#define PCC_MLCONT	0x08

struct puffs_newinfo {
	void		**pni_cookie;
	enum vtype	*pni_vtype;
	voff_t		*pni_size;
	dev_t		*pni_rdev;
	struct vattr	*pni_va;
	struct timespec	*pni_va_ttl;
	struct timespec	*pni_cn_ttl;
};

#define PUFFS_MAKEKCRED(to, from)					\
	/*LINTED: tnilxnaht, the cast is ok */				\
	const struct puffs_kcred *to = (const void *)from
#define PUFFS_MAKECRED(to, from)					\
	/*LINTED: tnilxnaht, the cast is ok */				\
	const struct puffs_cred *to = (const void *)from
#define PUFFS_KCREDTOCRED(to, from)					\
	/*LINTED: tnilxnaht, the cast is ok */				\
	to = (void *)from

__BEGIN_DECLS

void	puffs__framev_input(struct puffs_usermount *, struct puffs_framectrl *,
			   struct puffs_fctrl_io *);
int	puffs__framev_output(struct puffs_usermount *, struct puffs_framectrl*,
			    struct puffs_fctrl_io *);
void	puffs__framev_exit(struct puffs_usermount *);
void	puffs__framev_readclose(struct puffs_usermount *,
			       struct puffs_fctrl_io *, int);
void	puffs__framev_writeclose(struct puffs_usermount *,
				struct puffs_fctrl_io *, int);
void	puffs__framev_notify(struct puffs_fctrl_io *, int);
void	*puffs__framebuf_getdataptr(struct puffs_framebuf *);
int	puffs__framev_addfd_ctrl(struct puffs_usermount *, int, int,
				 struct puffs_framectrl *);
void	puffs__framebuf_moveinfo(struct puffs_framebuf *,
				 struct puffs_framebuf *);

void	puffs__theloop(struct puffs_cc *);
void	puffs__ml_dispatch(struct puffs_usermount *, struct puffs_framebuf *);

int	puffs__cc_create(struct puffs_usermount *, puffs_ccfunc,
			 struct puffs_cc **);
void	puffs__cc_cont(struct puffs_cc *);
void	puffs__cc_destroy(struct puffs_cc *, int);
void	puffs__cc_setcaller(struct puffs_cc *, pid_t, lwpid_t);
void	puffs__goto(struct puffs_cc *);
int	puffs__cc_savemain(struct puffs_usermount *);
int	puffs__cc_restoremain(struct puffs_usermount *);
void	puffs__cc_exit(struct puffs_usermount *);

int	puffs__fsframe_read(struct puffs_usermount *, struct puffs_framebuf *,
			    int, int *);
int	puffs__fsframe_write(struct puffs_usermount *, struct puffs_framebuf *,
			    int, int *);
int	puffs__fsframe_cmp(struct puffs_usermount *, struct puffs_framebuf *,
			   struct puffs_framebuf *, int *);
void	puffs__fsframe_gotframe(struct puffs_usermount *,
			        struct puffs_framebuf *);

__END_DECLS

#endif /* _PUFFS_PRIVATE_H_ */
