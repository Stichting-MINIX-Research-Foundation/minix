/*	$NetBSD: rndpseudo.c,v 1.35 2015/08/20 14:40:17 christos Exp $	*/

/*-
 * Copyright (c) 1997-2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Graff <explorer@flame.org>, Thor Lancelot Simon, and
 * Taylor R. Campbell.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rndpseudo.c,v 1.35 2015/08/20 14:40:17 christos Exp $");

#if defined(_KERNEL_OPT)
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/conf.h>
#include <sys/cprng.h>
#include <sys/cpu.h>
#include <sys/evcnt.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/percpu.h>
#include <sys/poll.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/rnd.h>
#include <sys/rndpool.h>
#include <sys/rndsource.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <dev/rnd_private.h>

#include "ioconf.h"

#if defined(__HAVE_CPU_COUNTER)
#include <machine/cpu_counter.h>
#endif

#ifdef RND_DEBUG
#define	DPRINTF(l,x)      if (rnd_debug & (l)) printf x
#else
#define	DPRINTF(l,x)
#endif

/*
 * list devices attached
 */
#if 0
#define	RND_VERBOSE
#endif

/*
 * The size of a temporary buffer for reading and writing entropy.
 */
#define	RND_TEMP_BUFFER_SIZE	512

static pool_cache_t rnd_temp_buffer_cache __read_mostly;

/*
 * Per-open state -- a lazily initialized CPRNG.
 */
struct rnd_ctx {
	struct cprng_strong	*rc_cprng;
	bool			rc_hard;
};

static pool_cache_t rnd_ctx_cache __read_mostly;

/*
 * The per-CPU RNGs used for short requests
 */
static percpu_t *percpu_urandom_cprng __read_mostly;


dev_type_open(rndopen);

const struct cdevsw rnd_cdevsw = {
	.d_open = rndopen,
	.d_close = noclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

static int rnd_read(struct file *, off_t *, struct uio *, kauth_cred_t, int);
static int rnd_write(struct file *, off_t *, struct uio *, kauth_cred_t, int);
static int rnd_ioctl(struct file *, u_long, void *);
static int rnd_poll(struct file *, int);
static int rnd_stat(struct file *, struct stat *);
static int rnd_close(struct file *);
static int rnd_kqfilter(struct file *, struct knote *);

const struct fileops rnd_fileops = {
	.fo_read = rnd_read,
	.fo_write = rnd_write,
	.fo_ioctl = rnd_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = rnd_poll,
	.fo_stat = rnd_stat,
	.fo_close = rnd_close,
	.fo_kqfilter = rnd_kqfilter,
	.fo_restart = fnullop_restart
};

static struct evcnt rndpseudo_soft = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "rndpseudo", "open soft");
static struct evcnt rndpseudo_hard = EVCNT_INITIALIZER(EVCNT_TYPE_MISC,
    NULL, "rndpseudo", "open hard");
EVCNT_ATTACH_STATIC(rndpseudo_soft);
EVCNT_ATTACH_STATIC(rndpseudo_hard);

/*
 * Generate a 32-bit counter.
 */
static inline uint32_t
rndpseudo_counter(void)
{
	struct bintime bt;
	uint32_t ret;

#if defined(__HAVE_CPU_COUNTER)
	if (cpu_hascounter())
		return (cpu_counter32());
#endif

	binuptime(&bt);
	ret = bt.sec;
	ret ^= bt.sec >> 32;
	ret ^= bt.frac;
	ret ^= bt.frac >> 32;

	return ret;
}

/*
 * Used by ioconf.c to attach the rnd pseudo-device.
 * `Attach' the random device.  We use the timing of this event as
 * another potential source of initial entropy.
 */
void
rndattach(int num)
{
	uint32_t c;

	/* Trap unwary players who don't call rnd_init() early.  */
	KASSERT(rnd_ready);

	rnd_temp_buffer_cache = pool_cache_init(RND_TEMP_BUFFER_SIZE, 0, 0, 0,
	    "rndtemp", NULL, IPL_NONE, NULL, NULL, NULL);
	rnd_ctx_cache = pool_cache_init(sizeof(struct rnd_ctx), 0, 0, 0,
	    "rndctx", NULL, IPL_NONE, NULL, NULL, NULL);
	percpu_urandom_cprng = percpu_alloc(sizeof(struct cprng_strong *));

	/* Mix in another counter.  */
	c = rndpseudo_counter();
	rnd_add_data(NULL, &c, sizeof(c), 1);
}

int
rndopen(dev_t dev, int flags, int fmt, struct lwp *l)
{
	bool hard;
	struct file *fp;
	int fd;
	int error;

	switch (minor(dev)) {
	case RND_DEV_URANDOM:
		hard = false;
		rndpseudo_soft.ev_count++;
		break;

	case RND_DEV_RANDOM:
		hard = true;
		rndpseudo_hard.ev_count++;
		break;

	default:
		return ENXIO;
	}

	error = fd_allocfile(&fp, &fd);
	if (error)
		return error;

	/*
	 * Allocate a context, but don't create a CPRNG yet -- do that
	 * lazily because it consumes entropy from the system entropy
	 * pool, which (currently) has the effect of depleting it and
	 * causing readers from /dev/random to block.  If this is
	 * /dev/urandom and the process is about to send only short
	 * reads to it, then we will be using a per-CPU CPRNG anyway.
	 */
	struct rnd_ctx *const ctx = pool_cache_get(rnd_ctx_cache, PR_WAITOK);
	ctx->rc_cprng = NULL;
	ctx->rc_hard = hard;

	error = fd_clone(fp, fd, flags, &rnd_fileops, ctx);
	KASSERT(error == EMOVEFD);

	return error;
}

/*
 * Fetch a /dev/u?random context's CPRNG, or create and save one if
 * necessary.
 */
static struct cprng_strong *
rnd_ctx_cprng(struct rnd_ctx *ctx)
{
	struct cprng_strong *cprng, *tmp = NULL;

	/* Fast path: if someone has already allocated a CPRNG, use it.  */
	cprng = ctx->rc_cprng;
	if (__predict_true(cprng != NULL)) {
		/* Make sure the CPU hasn't prefetched cprng's guts.  */
		membar_consumer();
		goto out;
	}

	/* Slow path: create a CPRNG.  Allocate before taking locks.  */
	char name[64];
	struct lwp *const l = curlwp;
	(void)snprintf(name, sizeof(name), "%d %"PRIu64" %u",
	    (int)l->l_proc->p_pid, l->l_ncsw, l->l_cpticks);
	const int flags = (ctx->rc_hard? (CPRNG_USE_CV | CPRNG_HARD) :
	    (CPRNG_INIT_ANY | CPRNG_REKEY_ANY));
	tmp = cprng_strong_create(name, IPL_NONE, flags);

	/* Publish cprng's guts before the pointer to them.  */
	membar_producer();

	/* Attempt to publish tmp, unless someone beat us.  */
	cprng = atomic_cas_ptr(&ctx->rc_cprng, NULL, tmp);
	if (__predict_false(cprng != NULL)) {
		/* Make sure the CPU hasn't prefetched cprng's guts.  */
		membar_consumer();
		goto out;
	}

	/* Published.  Commit tmp.  */
	cprng = tmp;
	tmp = NULL;

out:	if (tmp != NULL)
		cprng_strong_destroy(tmp);
	KASSERT(cprng != NULL);
	return cprng;
}

/*
 * Fetch a per-CPU CPRNG, or create and save one if necessary.
 */
static struct cprng_strong *
rnd_percpu_cprng(void)
{
	struct cprng_strong **cprngp, *cprng, *tmp = NULL;

	/* Fast path: if there already is a CPRNG for this CPU, use it.  */
	cprngp = percpu_getref(percpu_urandom_cprng);
	cprng = *cprngp;
	if (__predict_true(cprng != NULL))
		goto out;
	percpu_putref(percpu_urandom_cprng);

	/*
	 * Slow path: create a CPRNG named by this CPU.
	 *
	 * XXX The CPU of the name may be different from the CPU to
	 * which it is assigned, because we need to choose a name and
	 * allocate a cprng while preemption is enabled.  This could be
	 * fixed by changing the cprng_strong API (e.g., by adding a
	 * cprng_strong_setname or by separating allocation from
	 * initialization), but it's not clear that's worth the
	 * trouble.
	 */
	char name[32];
	(void)snprintf(name, sizeof(name), "urandom%u", cpu_index(curcpu()));
	tmp = cprng_strong_create(name, IPL_NONE,
	    (CPRNG_INIT_ANY | CPRNG_REKEY_ANY));

	/* Try again, but we may have been preempted and lost a race.  */
	cprngp = percpu_getref(percpu_urandom_cprng);
	cprng = *cprngp;
	if (__predict_false(cprng != NULL))
		goto out;

	/* Commit the CPRNG we just created.  */
	cprng = tmp;
	tmp = NULL;
	*cprngp = cprng;

out:	percpu_putref(percpu_urandom_cprng);
	if (tmp != NULL)
		cprng_strong_destroy(tmp);
	KASSERT(cprng != NULL);
	return cprng;
}

static int
rnd_read(struct file *fp, off_t *offp, struct uio *uio, kauth_cred_t cred,
    int flags)
{
	int error = 0;

	DPRINTF(RND_DEBUG_READ,
	    ("Random: Read of %zu requested, flags 0x%08x\n",
		uio->uio_resid, flags));

	if (uio->uio_resid == 0)
		return 0;

	struct rnd_ctx *const ctx = fp->f_rndctx;
	uint8_t *const buf = pool_cache_get(rnd_temp_buffer_cache, PR_WAITOK);

	/*
	 * Choose a CPRNG to use -- either the per-open CPRNG, if this
	 * is /dev/random or a long read, or the per-CPU one otherwise.
	 *
	 * XXX NIST_BLOCK_KEYLEN_BYTES is a detail of the cprng(9)
	 * implementation and as such should not be mentioned here.
	 */
	struct cprng_strong *const cprng =
	    ((ctx->rc_hard || (uio->uio_resid > NIST_BLOCK_KEYLEN_BYTES))?
		rnd_ctx_cprng(ctx) : rnd_percpu_cprng());

	/*
	 * Generate the data in RND_TEMP_BUFFER_SIZE chunks.
	 */
	while (uio->uio_resid > 0) {
		const size_t n_req = MIN(uio->uio_resid, RND_TEMP_BUFFER_SIZE);

		CTASSERT(RND_TEMP_BUFFER_SIZE <= CPRNG_MAX_LEN);
		const size_t n_read = cprng_strong(cprng, buf, n_req,
		    ((ctx->rc_hard && ISSET(fp->f_flag, FNONBLOCK))?
			FNONBLOCK : 0));

		/*
		 * Equality will hold unless this is /dev/random, in
		 * which case we get only as many bytes as are left
		 * from the CPRNG's `information-theoretic strength'
		 * since the last rekey.
		 */
		KASSERT(n_read <= n_req);
		KASSERT(ctx->rc_hard || (n_read == n_req));

		error = uiomove(buf, n_read, uio);
		if (error)
			goto out;

		/*
		 * For /dev/urandom:  Reads always succeed in full, no
		 * matter how many iterations that takes.  (XXX But
		 * this means the computation can't be interrupted,
		 * wihch seems suboptimal.)
		 *
		 * For /dev/random, nonblocking:  Reads succeed with as
		 * many bytes as a single request can return without
		 * blocking, or fail with EAGAIN if a request would
		 * block.  (There is no sense in trying multiple
		 * requests because if the first one didn't fill the
		 * buffer, the second one would almost certainly
		 * block.)
		 *
		 * For /dev/random, blocking:  Reads succeed with as
		 * many bytes as a single request -- which may block --
		 * can return if uninterrupted, or fail with EINTR if
		 * the request is interrupted.
		 */
		KASSERT((0 < n_read) || ctx->rc_hard);
		if (ctx->rc_hard) {
			if (0 < n_read)
				error = 0;
			else if (ISSET(fp->f_flag, FNONBLOCK))
				error = EAGAIN;
			else
				error = EINTR;
			goto out;
		}
	}

out:	pool_cache_put(rnd_temp_buffer_cache, buf);
	return error;
}

static int
rnd_write(struct file *fp, off_t *offp, struct uio *uio,
	   kauth_cred_t cred, int flags)
{
	uint8_t *bf;
	int n, ret = 0, estimate_ok = 0, estimate = 0, added = 0;

	ret = kauth_authorize_device(cred,
	    KAUTH_DEVICE_RND_ADDDATA, NULL, NULL, NULL, NULL);
	if (ret) {
		return (ret);
	}
	estimate_ok = !kauth_authorize_device(cred,
	    KAUTH_DEVICE_RND_ADDDATA_ESTIMATE, NULL, NULL, NULL, NULL);

	DPRINTF(RND_DEBUG_WRITE,
	    ("Random: Write of %zu requested\n", uio->uio_resid));

	if (uio->uio_resid == 0)
		return (0);
	ret = 0;
	bf = pool_cache_get(rnd_temp_buffer_cache, PR_WAITOK);
	while (uio->uio_resid > 0) {
		/*
		 * Don't flood the pool.
		 */
		if (added > RND_POOLWORDS * sizeof(int)) {
#ifdef RND_VERBOSE
			printf("rnd: added %d already, adding no more.\n",
			       added);
#endif
			break;
		}
		n = min(RND_TEMP_BUFFER_SIZE, uio->uio_resid);

		ret = uiomove((void *)bf, n, uio);
		if (ret != 0)
			break;

		if (estimate_ok) {
			/*
			 * Don't cause samples to be discarded by taking
			 * the pool's entropy estimate to the max.
			 */
			if (added > RND_POOLWORDS / 2)
				estimate = 0;
			else
				estimate = n * NBBY / 2;
#ifdef RND_VERBOSE
			printf("rnd: adding on write, %d bytes, estimate %d\n",
			       n, estimate);
#endif
		} else {
#ifdef RND_VERBOSE
			printf("rnd: kauth says no entropy.\n");
#endif
		}

		/*
		 * Mix in the bytes.
		 */
		rnd_add_data(NULL, bf, n, estimate);

		added += n;
		DPRINTF(RND_DEBUG_WRITE, ("Random: Copied in %d bytes\n", n));
	}
	pool_cache_put(rnd_temp_buffer_cache, bf);
	return (ret);
}

int
rnd_ioctl(struct file *fp, u_long cmd, void *addr)
{

	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		return 0;
	default:
		return rnd_system_ioctl(fp, cmd, addr);
	}
}

static int
rnd_poll(struct file *fp, int events)
{
	struct rnd_ctx *const ctx = fp->f_rndctx;
	int revents;

	/*
	 * We are always writable.
	 */
	revents = events & (POLLOUT | POLLWRNORM);

	/*
	 * Save some work if not checking for reads.
	 */
	if ((events & (POLLIN | POLLRDNORM)) == 0)
		return revents;

	/*
	 * For /dev/random, ask the CPRNG, which may require creating
	 * one.  For /dev/urandom, we're always readable.
	 */
	if (ctx->rc_hard)
		revents |= cprng_strong_poll(rnd_ctx_cprng(ctx), events);
	else
		revents |= (events & (POLLIN | POLLRDNORM));

	return revents;
}

static int
rnd_stat(struct file *fp, struct stat *st)
{
	struct rnd_ctx *const ctx = fp->f_rndctx;

	/* XXX lock, if cprng allocated?  why? */
	memset(st, 0, sizeof(*st));
	st->st_dev = makedev(cdevsw_lookup_major(&rnd_cdevsw),
	    (ctx->rc_hard? RND_DEV_RANDOM : RND_DEV_URANDOM));
	/* XXX leave atimespect, mtimespec, ctimespec = 0? */

	st->st_uid = kauth_cred_geteuid(fp->f_cred);
	st->st_gid = kauth_cred_getegid(fp->f_cred);
	st->st_mode = S_IFCHR;
	return 0;
}

static int
rnd_close(struct file *fp)
{
	struct rnd_ctx *const ctx = fp->f_rndctx;

	if (ctx->rc_cprng != NULL)
		cprng_strong_destroy(ctx->rc_cprng);
	fp->f_rndctx = NULL;
	pool_cache_put(rnd_ctx_cache, ctx);

	return 0;
}

static int
rnd_kqfilter(struct file *fp, struct knote *kn)
{
	struct rnd_ctx *const ctx = fp->f_rndctx;

	return cprng_strong_kqfilter(rnd_ctx_cprng(ctx), kn);
}
