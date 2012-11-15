/*-
 * Copyright (c) 2008 Ed Schouten <ed@FreeBSD.org>
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
__RCSID("$NetBSD: posix_spawn_sched.c,v 1.1 2012/02/11 23:31:24 martin Exp $");

#include "namespace.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>

/*
 * Spawn attributes
 */

int
posix_spawnattr_init(posix_spawnattr_t *ret)
{
	if (ret == NULL)
		return -1;
	
	memset(ret, 0, sizeof(posix_spawnattr_t));
	return (0);
}

int
posix_spawnattr_destroy(posix_spawnattr_t *sa)
{
	if (sa == NULL)
		return -1;

	return (0);
}

int
posix_spawnattr_getflags(const posix_spawnattr_t * __restrict sa,
    short * __restrict flags)
{
	*flags = sa->sa_flags;
	return (0);
}

int
posix_spawnattr_getpgroup(const posix_spawnattr_t * __restrict sa,
    pid_t * __restrict pgroup)
{
	*pgroup = sa->sa_pgroup;
	return (0);
}

int
posix_spawnattr_getschedparam(const posix_spawnattr_t * __restrict sa,
    struct sched_param * __restrict schedparam)
{
	*schedparam = sa->sa_schedparam;
	return (0);
}

int
posix_spawnattr_getschedpolicy(const posix_spawnattr_t * __restrict sa,
    int * __restrict schedpolicy)
{
	*schedpolicy = sa->sa_schedpolicy;
	return (0);
}

int
posix_spawnattr_getsigdefault(const posix_spawnattr_t * __restrict sa,
    sigset_t * __restrict sigdefault)
{
	*sigdefault = sa->sa_sigdefault;
	return (0);
}

int
posix_spawnattr_getsigmask(const posix_spawnattr_t * __restrict sa,
    sigset_t * __restrict sigmask)
{
	*sigmask = sa->sa_sigmask;
	return (0);
}

int
posix_spawnattr_setflags(posix_spawnattr_t *sa, short flags)
{
	sa->sa_flags = flags;
	return (0);
}

int
posix_spawnattr_setpgroup(posix_spawnattr_t *sa, pid_t pgroup)
{
	sa->sa_pgroup = pgroup;
	return (0);
}

int
posix_spawnattr_setschedparam(posix_spawnattr_t * __restrict sa,
    const struct sched_param * __restrict schedparam)
{
	sa->sa_schedparam = *schedparam;
	return (0);
}

int
posix_spawnattr_setschedpolicy(posix_spawnattr_t *sa, int schedpolicy)
{
	sa->sa_schedpolicy = schedpolicy;
	return (0);
}

int
posix_spawnattr_setsigdefault(posix_spawnattr_t * __restrict sa,
    const sigset_t * __restrict sigdefault)
{
	sa->sa_sigdefault = *sigdefault;
	return (0);
}

int
posix_spawnattr_setsigmask(posix_spawnattr_t * __restrict sa,
    const sigset_t * __restrict sigmask)
{
	sa->sa_sigmask = *sigmask;
	return (0);
}
