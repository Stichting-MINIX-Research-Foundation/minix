/*	$NetBSD: subr_cpufreq.c,v 1.9 2014/02/12 20:20:15 martin Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
__KERNEL_RCSID(0, "$NetBSD: subr_cpufreq.c,v 1.9 2014/02/12 20:20:15 martin Exp $");

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/cpufreq.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/time.h>
#include <sys/xcall.h>

static int	 cpufreq_latency(void);
static uint32_t	 cpufreq_get_max(void);
static uint32_t	 cpufreq_get_min(void);
static uint32_t	 cpufreq_get_raw(struct cpu_info *);
static void	 cpufreq_get_state_raw(uint32_t, struct cpufreq_state *);
static void	 cpufreq_set_raw(struct cpu_info *, uint32_t);
static void	 cpufreq_set_all_raw(uint32_t);

static kmutex_t		cpufreq_lock __cacheline_aligned;
static struct cpufreq  *cf_backend __read_mostly = NULL;

void
cpufreq_init(void)
{

	mutex_init(&cpufreq_lock, MUTEX_DEFAULT, IPL_NONE);
	cf_backend = kmem_zalloc(sizeof(*cf_backend), KM_SLEEP);
}

int
cpufreq_register(struct cpufreq *cf)
{
	uint32_t c, i, j, k, m;
	int rv;

	if (cold != 0)
		return EBUSY;

	KASSERT(cf != NULL);
	KASSERT(cf_backend != NULL);
	KASSERT(cf->cf_get_freq != NULL);
	KASSERT(cf->cf_set_freq != NULL);
	KASSERT(cf->cf_state_count > 0);
	KASSERT(cf->cf_state_count < CPUFREQ_STATE_MAX);

	mutex_enter(&cpufreq_lock);

	if (cf_backend->cf_init != false) {
		mutex_exit(&cpufreq_lock);
		return EALREADY;
	}

	cf_backend->cf_init = true;
	cf_backend->cf_mp = cf->cf_mp;
	cf_backend->cf_cookie = cf->cf_cookie;
	cf_backend->cf_get_freq = cf->cf_get_freq;
	cf_backend->cf_set_freq = cf->cf_set_freq;

	(void)strlcpy(cf_backend->cf_name, cf->cf_name, sizeof(cf->cf_name));

	/*
	 * Sanity check the values and verify descending order.
	 */
	for (c = i = 0; i < cf->cf_state_count; i++) {

		CTASSERT(CPUFREQ_STATE_ENABLED != 0);
		CTASSERT(CPUFREQ_STATE_DISABLED != 0);

		if (cf->cf_state[i].cfs_freq == 0)
			continue;

		if (cf->cf_state[i].cfs_freq > 9999 &&
		    cf->cf_state[i].cfs_freq != CPUFREQ_STATE_ENABLED &&
		    cf->cf_state[i].cfs_freq != CPUFREQ_STATE_DISABLED)
			continue;

		for (j = k = 0; j < i; j++) {

			if (cf->cf_state[i].cfs_freq >=
			    cf->cf_state[j].cfs_freq) {
				k = 1;
				break;
			}
		}

		if (k != 0)
			continue;

		cf_backend->cf_state[c].cfs_index = c;
		cf_backend->cf_state[c].cfs_freq = cf->cf_state[i].cfs_freq;
		cf_backend->cf_state[c].cfs_power = cf->cf_state[i].cfs_power;

		c++;
	}

	cf_backend->cf_state_count = c;

	if (cf_backend->cf_state_count == 0) {
		mutex_exit(&cpufreq_lock);
		cpufreq_deregister();
		return EINVAL;
	}

	rv = cpufreq_latency();

	if (rv != 0) {
		mutex_exit(&cpufreq_lock);
		cpufreq_deregister();
		return rv;
	}

	m = cpufreq_get_max();
	cpufreq_set_all_raw(m);
	mutex_exit(&cpufreq_lock);

	return 0;
}

void
cpufreq_deregister(void)
{

	mutex_enter(&cpufreq_lock);
	memset(cf_backend, 0, sizeof(*cf_backend));
	mutex_exit(&cpufreq_lock);
}

static int
cpufreq_latency(void)
{
	struct cpufreq *cf = cf_backend;
	struct timespec nta, ntb;
	const uint32_t n = 10;
	uint32_t i, j, l, m;
	uint64_t s;

	l = cpufreq_get_min();
	m = cpufreq_get_max();

	/*
	 * For each state, sample the average transition
	 * latency required to set the state for all CPUs.
	 */
	for (i = 0; i < cf->cf_state_count; i++) {

		for (s = 0, j = 0; j < n; j++) {

			/*
			 * Attempt to exclude possible
			 * caching done by the backend.
			 */
			if (i == 0)
				cpufreq_set_all_raw(l);
			else {
				cpufreq_set_all_raw(m);
			}

			nanotime(&nta);
			cpufreq_set_all_raw(cf->cf_state[i].cfs_freq);
			nanotime(&ntb);
			timespecsub(&ntb, &nta, &ntb);

			if (ntb.tv_sec != 0 ||
			    ntb.tv_nsec > CPUFREQ_LATENCY_MAX)
				continue;

			if (s >= UINT64_MAX - CPUFREQ_LATENCY_MAX)
				break;

			/* Convert to microseconds to prevent overflow */
			s += ntb.tv_nsec / 1000;
		}

		/*
		 * Consider the backend unsuitable if
		 * the transition latency was too high.
		 */
		if (s == 0)
			return EMSGSIZE;

		cf->cf_state[i].cfs_latency = s / n;
	}

	return 0;
}

void
cpufreq_suspend(struct cpu_info *ci)
{
	struct cpufreq *cf = cf_backend;
	uint32_t l, s;

	mutex_enter(&cpufreq_lock);

	if (cf->cf_init != true) {
		mutex_exit(&cpufreq_lock);
		return;
	}

	l = cpufreq_get_min();
	s = cpufreq_get_raw(ci);

	cpufreq_set_raw(ci, l);
	cf->cf_state_saved = s;

	mutex_exit(&cpufreq_lock);
}

void
cpufreq_resume(struct cpu_info *ci)
{
	struct cpufreq *cf = cf_backend;

	mutex_enter(&cpufreq_lock);

	if (cf->cf_init != true || cf->cf_state_saved == 0) {
		mutex_exit(&cpufreq_lock);
		return;
	}

	cpufreq_set_raw(ci, cf->cf_state_saved);
	mutex_exit(&cpufreq_lock);
}

uint32_t
cpufreq_get(struct cpu_info *ci)
{
	struct cpufreq *cf = cf_backend;
	uint32_t freq;

	mutex_enter(&cpufreq_lock);

	if (cf->cf_init != true) {
		mutex_exit(&cpufreq_lock);
		return 0;
	}

	freq = cpufreq_get_raw(ci);
	mutex_exit(&cpufreq_lock);

	return freq;
}

static uint32_t
cpufreq_get_max(void)
{
	struct cpufreq *cf = cf_backend;

	KASSERT(cf->cf_init != false);
	KASSERT(mutex_owned(&cpufreq_lock) != 0);

	return cf->cf_state[0].cfs_freq;
}

static uint32_t
cpufreq_get_min(void)
{
	struct cpufreq *cf = cf_backend;

	KASSERT(cf->cf_init != false);
	KASSERT(mutex_owned(&cpufreq_lock) != 0);

	return cf->cf_state[cf->cf_state_count - 1].cfs_freq;
}

static uint32_t
cpufreq_get_raw(struct cpu_info *ci)
{
	struct cpufreq *cf = cf_backend;
	uint32_t freq = 0;
	uint64_t xc;

	KASSERT(cf->cf_init != false);
	KASSERT(mutex_owned(&cpufreq_lock) != 0);

	xc = xc_unicast(0, (*cf->cf_get_freq), cf->cf_cookie, &freq, ci);
	xc_wait(xc);

	return freq;
}

int
cpufreq_get_backend(struct cpufreq *dst)
{
	struct cpufreq *cf = cf_backend;

	mutex_enter(&cpufreq_lock);

	if (cf->cf_init != true || dst == NULL) {
		mutex_exit(&cpufreq_lock);
		return ENODEV;
	}

	memcpy(dst, cf, sizeof(*cf));
	mutex_exit(&cpufreq_lock);

	return 0;
}

int
cpufreq_get_state(uint32_t freq, struct cpufreq_state *cfs)
{
	struct cpufreq *cf = cf_backend;

	mutex_enter(&cpufreq_lock);

	if (cf->cf_init != true || cfs == NULL) {
		mutex_exit(&cpufreq_lock);
		return ENODEV;
	}

	cpufreq_get_state_raw(freq, cfs);
	mutex_exit(&cpufreq_lock);

	return 0;
}

int
cpufreq_get_state_index(uint32_t index, struct cpufreq_state *cfs)
{
	struct cpufreq *cf = cf_backend;

	mutex_enter(&cpufreq_lock);

	if (cf->cf_init != true || cfs == NULL) {
		mutex_exit(&cpufreq_lock);
		return ENODEV;
	}

	if (index >= cf->cf_state_count) {
		mutex_exit(&cpufreq_lock);
		return EINVAL;
	}

	memcpy(cfs, &cf->cf_state[index], sizeof(*cfs));
	mutex_exit(&cpufreq_lock);

	return 0;
}

static void
cpufreq_get_state_raw(uint32_t freq, struct cpufreq_state *cfs)
{
	struct cpufreq *cf = cf_backend;
	uint32_t f, hi, i = 0, lo = 0;

	KASSERT(mutex_owned(&cpufreq_lock) != 0);
	KASSERT(cf->cf_init != false && cfs != NULL);

	hi = cf->cf_state_count;

	while (lo < hi) {

		i = (lo + hi) >> 1;
		f = cf->cf_state[i].cfs_freq;

		if (freq == f)
			break;
		else if (freq > f)
			hi = i;
		else {
			lo = i + 1;
		}
	}

	memcpy(cfs, &cf->cf_state[i], sizeof(*cfs));
}

void
cpufreq_set(struct cpu_info *ci, uint32_t freq)
{
	struct cpufreq *cf = cf_backend;

	mutex_enter(&cpufreq_lock);

	if (__predict_false(cf->cf_init != true)) {
		mutex_exit(&cpufreq_lock);
		return;
	}

	cpufreq_set_raw(ci, freq);
	mutex_exit(&cpufreq_lock);
}

static void
cpufreq_set_raw(struct cpu_info *ci, uint32_t freq)
{
	struct cpufreq *cf = cf_backend;
	uint64_t xc;

	KASSERT(cf->cf_init != false);
	KASSERT(mutex_owned(&cpufreq_lock) != 0);

	xc = xc_unicast(0, (*cf->cf_set_freq), cf->cf_cookie, &freq, ci);
	xc_wait(xc);
}

void
cpufreq_set_all(uint32_t freq)
{
	struct cpufreq *cf = cf_backend;

	mutex_enter(&cpufreq_lock);

	if (__predict_false(cf->cf_init != true)) {
		mutex_exit(&cpufreq_lock);
		return;
	}

	cpufreq_set_all_raw(freq);
	mutex_exit(&cpufreq_lock);
}

static void
cpufreq_set_all_raw(uint32_t freq)
{
	struct cpufreq *cf = cf_backend;
	uint64_t xc;

	KASSERT(cf->cf_init != false);
	KASSERT(mutex_owned(&cpufreq_lock) != 0);

	xc = xc_broadcast(0, (*cf->cf_set_freq), cf->cf_cookie, &freq);
	xc_wait(xc);
}

#ifdef notyet
void
cpufreq_set_higher(struct cpu_info *ci)
{
	cpufreq_set_step(ci, -1);
}

void
cpufreq_set_lower(struct cpu_info *ci)
{
	cpufreq_set_step(ci, 1);
}

static void
cpufreq_set_step(struct cpu_info *ci, int32_t step)
{
	struct cpufreq *cf = cf_backend;
	struct cpufreq_state cfs;
	uint32_t freq;
	int32_t index;

	mutex_enter(&cpufreq_lock);

	if (__predict_false(cf->cf_init != true)) {
		mutex_exit(&cpufreq_lock);
		return;
	}

	freq = cpufreq_get_raw(ci);

	if (__predict_false(freq == 0)) {
		mutex_exit(&cpufreq_lock);
		return;
	}

	cpufreq_get_state_raw(freq, &cfs);
	index = cfs.cfs_index + step;

	if (index < 0 || index >= (int32_t)cf->cf_state_count) {
		mutex_exit(&cpufreq_lock);
		return;
	}

	cpufreq_set_raw(ci, cf->cf_state[index].cfs_freq);
	mutex_exit(&cpufreq_lock);
}
#endif
