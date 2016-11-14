/*	$NetBSD: kern_cfglock.c,v 1.1 2010/08/21 13:17:31 pgoyette Exp $ */

/*-
 * Copyright (c) 2002, 2006, 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Andrew Doran.
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
__KERNEL_RCSID(0, "$NetBSD: kern_cfglock.c,v 1.1 2010/08/21 13:17:31 pgoyette Exp $");

#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/mutex.h>
#include <sys/lwp.h>
#include <sys/systm.h>

static kmutex_t kernconfig_mutex;
static lwp_t *kernconfig_lwp;
static int kernconfig_recurse;

/*
 * Functions for manipulating the kernel configuration lock.  This
 * recursive lock should be used to protect all additions and removals
 * of kernel functionality, such as device configuration and loading
 * of modular kernel components.
 */

void
kernconfig_lock_init(void)
{

	mutex_init(&kernconfig_mutex, MUTEX_DEFAULT, IPL_NONE);
	kernconfig_lwp = NULL;
	kernconfig_recurse = 0;
}

void
kernconfig_lock(void)
{
	lwp_t	*my_lwp;

	/*
	 * It's OK to check this unlocked, since it could only be set to
	 * curlwp by the current thread itself, and not by an interrupt
	 * or any other LWP.
	 */
	KASSERT(!cpu_intr_p());
	my_lwp = curlwp;
	if (kernconfig_lwp == my_lwp) {
		kernconfig_recurse++;
		KASSERT(kernconfig_recurse > 1);
	} else {
		mutex_enter(&kernconfig_mutex);
		kernconfig_lwp = my_lwp;
		kernconfig_recurse = 1;
	}
}

void
kernconfig_unlock(void)
{

	KASSERT(kernconfig_is_held());
	KASSERT(kernconfig_recurse != 0);
	if (--kernconfig_recurse == 0) {
		kernconfig_lwp = NULL;
		mutex_exit(&kernconfig_mutex);
	}
}

bool
kernconfig_is_held(void)
{

	return mutex_owned(&kernconfig_mutex);
}
