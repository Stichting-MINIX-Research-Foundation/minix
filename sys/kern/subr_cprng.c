/*	$NetBSD: subr_cprng.c,v 1.27 2015/04/13 22:43:41 riastradh Exp $ */

/*-
 * Copyright (c) 2011-2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Thor Lancelot Simon and Taylor R. Campbell.
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
__KERNEL_RCSID(0, "$NetBSD: subr_cprng.c,v 1.27 2015/04/13 22:43:41 riastradh Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/condvar.h>
#include <sys/cprng.h>
#include <sys/errno.h>
#include <sys/event.h>		/* XXX struct knote */
#include <sys/fcntl.h>		/* XXX FNONBLOCK */
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/lwp.h>
#include <sys/once.h>
#include <sys/percpu.h>
#include <sys/poll.h>		/* XXX POLLIN/POLLOUT/&c. */
#include <sys/select.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/rndsink.h>
#if DIAGNOSTIC
#include <sys/rngtest.h>
#endif

#include <crypto/nist_ctr_drbg/nist_ctr_drbg.h>

#if defined(__HAVE_CPU_COUNTER)
#include <machine/cpu_counter.h>
#endif

static int sysctl_kern_urnd(SYSCTLFN_PROTO);
static int sysctl_kern_arnd(SYSCTLFN_PROTO);

static void	cprng_strong_generate(struct cprng_strong *, void *, size_t);
static void	cprng_strong_reseed(struct cprng_strong *);
static void	cprng_strong_reseed_from(struct cprng_strong *, const void *,
		    size_t, bool);
#if DIAGNOSTIC
static void	cprng_strong_rngtest(struct cprng_strong *);
#endif

static rndsink_callback_t	cprng_strong_rndsink_callback;

void
cprng_init(void)
{
	static struct sysctllog *random_sysctllog;

	nist_ctr_initialize();

	sysctl_createv(&random_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "urandom",
		       SYSCTL_DESCR("Random integer value"),
		       sysctl_kern_urnd, 0, NULL, 0,
		       CTL_KERN, KERN_URND, CTL_EOL);
	sysctl_createv(&random_sysctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "arandom",
		       SYSCTL_DESCR("n bytes of random data"),
		       sysctl_kern_arnd, 0, NULL, 0,
		       CTL_KERN, KERN_ARND, CTL_EOL);
}

static inline uint32_t
cprng_counter(void)
{
	struct timeval tv;

#if defined(__HAVE_CPU_COUNTER)
	if (cpu_hascounter())
		return cpu_counter32();
#endif
	if (__predict_false(cold)) {
		static int ctr;
		/* microtime unsafe if clock not running yet */
		return ctr++;
	}
	getmicrotime(&tv);
	return (tv.tv_sec * 1000000 + tv.tv_usec);
}

struct cprng_strong {
	char		cs_name[16];
	int		cs_flags;
	kmutex_t	cs_lock;
	percpu_t	*cs_percpu;
	kcondvar_t	cs_cv;
	struct selinfo	cs_selq;
	struct rndsink	*cs_rndsink;
	bool		cs_ready;
	NIST_CTR_DRBG	cs_drbg;

	/* XXX Kludge for /dev/random `information-theoretic' properties.   */
	unsigned int	cs_remaining;
};

struct cprng_strong *
cprng_strong_create(const char *name, int ipl, int flags)
{
	const uint32_t cc = cprng_counter();
	struct cprng_strong *const cprng = kmem_alloc(sizeof(*cprng),
	    KM_SLEEP);

	/*
	 * rndsink_request takes a spin lock at IPL_VM, so we can be no
	 * higher than that.
	 */
	KASSERT(ipl != IPL_SCHED && ipl != IPL_HIGH);

	/* Initialize the easy fields.  */
	(void)strlcpy(cprng->cs_name, name, sizeof(cprng->cs_name));
	cprng->cs_flags = flags;
	mutex_init(&cprng->cs_lock, MUTEX_DEFAULT, ipl);
	cv_init(&cprng->cs_cv, cprng->cs_name);
	selinit(&cprng->cs_selq);
	cprng->cs_rndsink = rndsink_create(NIST_BLOCK_KEYLEN_BYTES,
	    &cprng_strong_rndsink_callback, cprng);

	/* Get some initial entropy.  Record whether it is full entropy.  */
	uint8_t seed[NIST_BLOCK_KEYLEN_BYTES];
	mutex_enter(&cprng->cs_lock);
	cprng->cs_ready = rndsink_request(cprng->cs_rndsink, seed,
	    sizeof(seed));
	if (nist_ctr_drbg_instantiate(&cprng->cs_drbg, seed, sizeof(seed),
		&cc, sizeof(cc), cprng->cs_name, sizeof(cprng->cs_name)))
		/* XXX Fix nist_ctr_drbg API so this can't happen.  */
		panic("cprng %s: NIST CTR_DRBG instantiation failed",
		    cprng->cs_name);
	explicit_memset(seed, 0, sizeof(seed));

	if (ISSET(flags, CPRNG_HARD))
		cprng->cs_remaining = NIST_BLOCK_KEYLEN_BYTES;
	else
		cprng->cs_remaining = 0;

	if (!cprng->cs_ready && !ISSET(flags, CPRNG_INIT_ANY))
		printf("cprng %s: creating with partial entropy\n",
		    cprng->cs_name);
	mutex_exit(&cprng->cs_lock);

	return cprng;
}

void
cprng_strong_destroy(struct cprng_strong *cprng)
{

	/*
	 * Destroy the rndsink first to prevent calls to the callback.
	 */
	rndsink_destroy(cprng->cs_rndsink);

	KASSERT(!cv_has_waiters(&cprng->cs_cv));
#if 0
	KASSERT(!select_has_waiters(&cprng->cs_selq)) /* XXX ? */
#endif

	nist_ctr_drbg_destroy(&cprng->cs_drbg);
	seldestroy(&cprng->cs_selq);
	cv_destroy(&cprng->cs_cv);
	mutex_destroy(&cprng->cs_lock);

	explicit_memset(cprng, 0, sizeof(*cprng)); /* paranoia */
	kmem_free(cprng, sizeof(*cprng));
}

/*
 * Generate some data from cprng.  Block or return zero bytes,
 * depending on flags & FNONBLOCK, if cprng was created without
 * CPRNG_REKEY_ANY.
 */
size_t
cprng_strong(struct cprng_strong *cprng, void *buffer, size_t bytes, int flags)
{
	size_t result;

	/* Caller must loop for more than CPRNG_MAX_LEN bytes.  */
	bytes = MIN(bytes, CPRNG_MAX_LEN);

	mutex_enter(&cprng->cs_lock);

	if (ISSET(cprng->cs_flags, CPRNG_REKEY_ANY)) {
		if (!cprng->cs_ready)
			cprng_strong_reseed(cprng);
	} else {
		while (!cprng->cs_ready) {
			if (ISSET(flags, FNONBLOCK) ||
			    !ISSET(cprng->cs_flags, CPRNG_USE_CV) ||
			    cv_wait_sig(&cprng->cs_cv, &cprng->cs_lock)) {
				result = 0;
				goto out;
			}
		}
	}

	/*
	 * Debit the entropy if requested.
	 *
	 * XXX Kludge for /dev/random `information-theoretic' properties.
	 */
	if (__predict_false(ISSET(cprng->cs_flags, CPRNG_HARD))) {
		KASSERT(0 < cprng->cs_remaining);
		KASSERT(cprng->cs_remaining <= NIST_BLOCK_KEYLEN_BYTES);
		if (bytes < cprng->cs_remaining) {
			cprng->cs_remaining -= bytes;
		} else {
			bytes = cprng->cs_remaining;
			cprng->cs_remaining = NIST_BLOCK_KEYLEN_BYTES;
			cprng->cs_ready = false;
			rndsink_schedule(cprng->cs_rndsink);
		}
		KASSERT(bytes <= NIST_BLOCK_KEYLEN_BYTES);
		KASSERT(0 < cprng->cs_remaining);
		KASSERT(cprng->cs_remaining <= NIST_BLOCK_KEYLEN_BYTES);
	}

	cprng_strong_generate(cprng, buffer, bytes);
	result = bytes;

out:	mutex_exit(&cprng->cs_lock);
	return result;
}

static void	filt_cprng_detach(struct knote *);
static int	filt_cprng_event(struct knote *, long);

static const struct filterops cprng_filtops =
	{ 1, NULL, filt_cprng_detach, filt_cprng_event };

int
cprng_strong_kqfilter(struct cprng_strong *cprng, struct knote *kn)
{

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &cprng_filtops;
		kn->kn_hook = cprng;
		mutex_enter(&cprng->cs_lock);
		SLIST_INSERT_HEAD(&cprng->cs_selq.sel_klist, kn, kn_selnext);
		mutex_exit(&cprng->cs_lock);
		return 0;

	case EVFILT_WRITE:
	default:
		return EINVAL;
	}
}

static void
filt_cprng_detach(struct knote *kn)
{
	struct cprng_strong *const cprng = kn->kn_hook;

	mutex_enter(&cprng->cs_lock);
	SLIST_REMOVE(&cprng->cs_selq.sel_klist, kn, knote, kn_selnext);
	mutex_exit(&cprng->cs_lock);
}

static int
filt_cprng_event(struct knote *kn, long hint)
{
	struct cprng_strong *const cprng = kn->kn_hook;
	int ret;

	if (hint == NOTE_SUBMIT)
		KASSERT(mutex_owned(&cprng->cs_lock));
	else
		mutex_enter(&cprng->cs_lock);
	if (cprng->cs_ready) {
		kn->kn_data = CPRNG_MAX_LEN; /* XXX Too large?  */
		ret = 1;
	} else {
		ret = 0;
	}
	if (hint == NOTE_SUBMIT)
		KASSERT(mutex_owned(&cprng->cs_lock));
	else
		mutex_exit(&cprng->cs_lock);

	return ret;
}

int
cprng_strong_poll(struct cprng_strong *cprng, int events)
{
	int revents;

	if (!ISSET(events, (POLLIN | POLLRDNORM)))
		return 0;

	mutex_enter(&cprng->cs_lock);
	if (cprng->cs_ready) {
		revents = (events & (POLLIN | POLLRDNORM));
	} else {
		selrecord(curlwp, &cprng->cs_selq);
		revents = 0;
	}
	mutex_exit(&cprng->cs_lock);

	return revents;
}

/*
 * XXX Move nist_ctr_drbg_reseed_advised_p and
 * nist_ctr_drbg_reseed_needed_p into the nist_ctr_drbg API and make
 * the NIST_CTR_DRBG structure opaque.
 */
static bool
nist_ctr_drbg_reseed_advised_p(NIST_CTR_DRBG *drbg)
{

	return (drbg->reseed_counter > (NIST_CTR_DRBG_RESEED_INTERVAL / 2));
}

static bool
nist_ctr_drbg_reseed_needed_p(NIST_CTR_DRBG *drbg)
{

	return (drbg->reseed_counter >= NIST_CTR_DRBG_RESEED_INTERVAL);
}

/*
 * Generate some data from the underlying generator.
 */
static void
cprng_strong_generate(struct cprng_strong *cprng, void *buffer, size_t bytes)
{
	const uint32_t cc = cprng_counter();

	KASSERT(bytes <= CPRNG_MAX_LEN);
	KASSERT(mutex_owned(&cprng->cs_lock));

	/*
	 * Generate some data from the NIST CTR_DRBG.  Caller
	 * guarantees reseed if we're not ready, and if we exhaust the
	 * generator, we mark ourselves not ready.  Consequently, this
	 * call to the CTR_DRBG should not fail.
	 */
	if (__predict_false(nist_ctr_drbg_generate(&cprng->cs_drbg, buffer,
		    bytes, &cc, sizeof(cc))))
		panic("cprng %s: NIST CTR_DRBG failed", cprng->cs_name);

	/*
	 * If we've been seeing a lot of use, ask for some fresh
	 * entropy soon.
	 */
	if (__predict_false(nist_ctr_drbg_reseed_advised_p(&cprng->cs_drbg)))
		rndsink_schedule(cprng->cs_rndsink);

	/*
	 * If we just exhausted the generator, inform the next user
	 * that we need a reseed.
	 */
	if (__predict_false(nist_ctr_drbg_reseed_needed_p(&cprng->cs_drbg))) {
		cprng->cs_ready = false;
		rndsink_schedule(cprng->cs_rndsink); /* paranoia */
	}
}

/*
 * Reseed with whatever we can get from the system entropy pool right now.
 */
static void
cprng_strong_reseed(struct cprng_strong *cprng)
{
	uint8_t seed[NIST_BLOCK_KEYLEN_BYTES];

	KASSERT(mutex_owned(&cprng->cs_lock));

	const bool full_entropy = rndsink_request(cprng->cs_rndsink, seed,
	    sizeof(seed));
	cprng_strong_reseed_from(cprng, seed, sizeof(seed), full_entropy);
	explicit_memset(seed, 0, sizeof(seed));
}

/*
 * Reseed with the given seed.  If we now have full entropy, notify waiters.
 */
static void
cprng_strong_reseed_from(struct cprng_strong *cprng,
    const void *seed, size_t bytes, bool full_entropy)
{
	const uint32_t cc = cprng_counter();

	KASSERT(bytes == NIST_BLOCK_KEYLEN_BYTES);
	KASSERT(mutex_owned(&cprng->cs_lock));

	/*
	 * Notify anyone interested in the partiality of entropy in our
	 * seed -- anyone waiting for full entropy, or any system
	 * operators interested in knowing when the entropy pool is
	 * running on fumes.
	 */
	if (full_entropy) {
		if (!cprng->cs_ready) {
			cprng->cs_ready = true;
			cv_broadcast(&cprng->cs_cv);
			selnotify(&cprng->cs_selq, (POLLIN | POLLRDNORM),
			    NOTE_SUBMIT);
		}
	} else {
		/*
		 * XXX Is there is any harm in reseeding with partial
		 * entropy when we had full entropy before?  If so,
		 * remove the conditional on this message.
		 */
		if (!cprng->cs_ready &&
		    !ISSET(cprng->cs_flags, CPRNG_REKEY_ANY))
			printf("cprng %s: reseeding with partial entropy\n",
			    cprng->cs_name);
	}

	if (nist_ctr_drbg_reseed(&cprng->cs_drbg, seed, bytes, &cc, sizeof(cc)))
		/* XXX Fix nist_ctr_drbg API so this can't happen.  */
		panic("cprng %s: NIST CTR_DRBG reseed failed", cprng->cs_name);

#if DIAGNOSTIC
	cprng_strong_rngtest(cprng);
#endif
}

#if DIAGNOSTIC
/*
 * Generate some output and apply a statistical RNG test to it.
 */
static void
cprng_strong_rngtest(struct cprng_strong *cprng)
{

	KASSERT(mutex_owned(&cprng->cs_lock));

	/* XXX Switch to a pool cache instead?  */
	rngtest_t *const rt = kmem_intr_alloc(sizeof(*rt), KM_NOSLEEP);
	if (rt == NULL)
		/* XXX Warn?  */
		return;

	(void)strlcpy(rt->rt_name, cprng->cs_name, sizeof(rt->rt_name));

	if (nist_ctr_drbg_generate(&cprng->cs_drbg, rt->rt_b, sizeof(rt->rt_b),
		NULL, 0))
		panic("cprng %s: NIST CTR_DRBG failed after reseed",
		    cprng->cs_name);

	if (rngtest(rt)) {
		printf("cprng %s: failed statistical RNG test\n",
		    cprng->cs_name);
		/* XXX Not clear that this does any good...  */
		cprng->cs_ready = false;
		rndsink_schedule(cprng->cs_rndsink);
	}

	explicit_memset(rt, 0, sizeof(*rt)); /* paranoia */
	kmem_intr_free(rt, sizeof(*rt));
}
#endif

/*
 * Feed entropy from an rndsink request into the CPRNG for which the
 * request was issued.
 */
static void
cprng_strong_rndsink_callback(void *context, const void *seed, size_t bytes)
{
	struct cprng_strong *const cprng = context;

	mutex_enter(&cprng->cs_lock);
	/* Assume that rndsinks provide only full-entropy output.  */
	cprng_strong_reseed_from(cprng, seed, bytes, true);
	mutex_exit(&cprng->cs_lock);
}

static cprng_strong_t *sysctl_prng;

static int
makeprng(void)
{

	/* can't create in cprng_init(), too early */
	sysctl_prng = cprng_strong_create("sysctl", IPL_NONE,
					  CPRNG_INIT_ANY|CPRNG_REKEY_ANY);
	return 0;
}

/*
 * sysctl helper routine for kern.urandom node. Picks a random number
 * for you.
 */
static int
sysctl_kern_urnd(SYSCTLFN_ARGS)
{
	static ONCE_DECL(control);
	int v, rv;

	RUN_ONCE(&control, makeprng);
	rv = cprng_strong(sysctl_prng, &v, sizeof(v), 0);
	if (rv == sizeof(v)) {
		struct sysctlnode node = *rnode;
		node.sysctl_data = &v;
		return (sysctl_lookup(SYSCTLFN_CALL(&node)));
	}
	else
		return (EIO);	/*XXX*/
}

/*
 * sysctl helper routine for kern.arandom node.  Fills the supplied
 * structure with random data for you.
 *
 * This node was originally declared as type "int" but its implementation
 * in OpenBSD, whence it came, would happily return up to 8K of data if
 * requested.  Evidently this was used to key RC4 in userspace.
 *
 * In NetBSD, the libc stack-smash-protection code reads 64 bytes
 * from here at every program startup.  So though it would be nice
 * to make this node return only 32 or 64 bits, we can't.  Too bad!
 */
static int
sysctl_kern_arnd(SYSCTLFN_ARGS)
{
	int error;
	void *v;
	struct sysctlnode node = *rnode;

	switch (*oldlenp) {
	    case 0:
		return 0;
	    default:
		if (*oldlenp > 256) {
			return E2BIG;
		}
		v = kmem_alloc(*oldlenp, KM_SLEEP);
		cprng_fast(v, *oldlenp);
		node.sysctl_data = v;
		node.sysctl_size = *oldlenp;
		error = sysctl_lookup(SYSCTLFN_CALL(&node));
		kmem_free(v, *oldlenp);
		return error;
	}
}
