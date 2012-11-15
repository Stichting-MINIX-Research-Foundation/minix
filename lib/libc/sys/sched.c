/*	$NetBSD: sched.c,v 1.4 2012/03/18 02:04:39 christos Exp $	*/

/*
 * Copyright (c) 2008, Mindaugas Rasiukevicius <rmind at NetBSD org>
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
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: sched.c,v 1.4 2012/03/18 02:04:39 christos Exp $");

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>

/* All LWPs in the process */
#define	P_ALL_LWPS		0

/*
 * Scheduling parameters.
 */

int
sched_setparam(pid_t pid, const struct sched_param *param)
{
	struct sched_param sp;

	memset(&sp, 0, sizeof(struct sched_param));
	sp.sched_priority = param->sched_priority;
	return _sched_setparam(pid, P_ALL_LWPS, SCHED_NONE, &sp);
}

int
sched_getparam(pid_t pid, struct sched_param *param)
{

	return _sched_getparam(pid, P_ALL_LWPS, NULL, param);
}

int
sched_setscheduler(pid_t pid, int policy, const struct sched_param *param)
{
	struct sched_param sp;
	int ret, old_policy;

	ret = _sched_getparam(pid, P_ALL_LWPS, &old_policy, &sp);
	if (ret < 0)
		return ret;

	memset(&sp, 0, sizeof(struct sched_param));
	sp.sched_priority = param->sched_priority;
	ret = _sched_setparam(pid, P_ALL_LWPS, policy, &sp);
	if (ret < 0)
		return ret;

	return old_policy;
}

int
sched_getscheduler(pid_t pid)
{
	struct sched_param sp;
	int ret, policy;

	ret = _sched_getparam(pid, P_ALL_LWPS, &policy, &sp);
	if (ret < 0)
		return ret;

	return policy;
}

/*
 * Scheduling priorities.
 */

int
sched_get_priority_max(int policy)
{

	if (policy < SCHED_OTHER || policy > SCHED_RR) {
		errno = EINVAL;
		return -1;
	}
	return (int)sysconf(_SC_SCHED_PRI_MAX);
}

int
sched_get_priority_min(int policy)
{

	if (policy < SCHED_OTHER || policy > SCHED_RR) {
		errno = EINVAL;
		return -1;
	}
	return (int)sysconf(_SC_SCHED_PRI_MIN);
}

int
/*ARGSUSED*/
sched_rr_get_interval(pid_t pid, struct timespec *interval)
{

	if (pid && kill(pid, 0) == -1)
		return -1;
	interval->tv_sec = 0;
	interval->tv_nsec = sysconf(_SC_SCHED_RT_TS) * 1000;
	return 0;
}

/*
 * Process affinity.
 */

int
sched_getaffinity_np(pid_t pid, size_t size, cpuset_t *cpuset)
{

	return _sched_getaffinity(pid, P_ALL_LWPS, size, cpuset);
}

int
sched_setaffinity_np(pid_t pid, size_t size, cpuset_t *cpuset)
{

	return _sched_setaffinity(pid, P_ALL_LWPS, size, cpuset);
}
