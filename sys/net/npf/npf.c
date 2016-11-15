/*	$NetBSD: npf.c,v 1.23 2015/08/20 14:40:19 christos Exp $	*/

/*-
 * Copyright (c) 2009-2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This material is based upon work partially supported by The
 * NetBSD Foundation under a contract with Mindaugas Rasiukevicius.
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

/*
 * NPF main: dynamic load/initialisation and unload routines.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: npf.c,v 1.23 2015/08/20 14:40:19 christos Exp $");

#include <sys/param.h>
#include <sys/types.h>

#include <sys/atomic.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/kmem.h>
#include <sys/lwp.h>
#include <sys/module.h>
#include <sys/percpu.h>
#include <sys/rwlock.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include "npf_impl.h"
#include "npf_conn.h"

#include "ioconf.h"

/*
 * Module and device structures.
 */
MODULE(MODULE_CLASS_DRIVER, npf, NULL);

static int	npf_fini(void);
static int	npf_dev_open(dev_t, int, int, lwp_t *);
static int	npf_dev_close(dev_t, int, int, lwp_t *);
static int	npf_dev_ioctl(dev_t, u_long, void *, int, lwp_t *);
static int	npf_dev_poll(dev_t, int, lwp_t *);
static int	npf_dev_read(dev_t, struct uio *, int);

static int	npfctl_stats(void *);

static percpu_t *		npf_stats_percpu	__read_mostly;
static struct sysctllog *	npf_sysctl		__read_mostly;

const struct cdevsw npf_cdevsw = {
	.d_open = npf_dev_open,
	.d_close = npf_dev_close,
	.d_read = npf_dev_read,
	.d_write = nowrite,
	.d_ioctl = npf_dev_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = npf_dev_poll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

static int
npf_init(void)
{
#ifdef _MODULE
	devmajor_t bmajor = NODEVMAJOR, cmajor = NODEVMAJOR;
#endif
	int error = 0;

	npf_stats_percpu = percpu_alloc(NPF_STATS_SIZE);
	npf_sysctl = NULL;

	npf_bpf_sysinit();
	npf_worker_sysinit();
	npf_tableset_sysinit();
	npf_conn_sysinit();
	npf_nat_sysinit();
	npf_alg_sysinit();
	npf_ext_sysinit();

	/* Load empty configuration. */
	npf_pfil_register(true);
	npf_config_init();

#ifdef _MODULE
	/* Attach /dev/npf device. */
	error = devsw_attach("npf", NULL, &bmajor, &npf_cdevsw, &cmajor);
	if (error) {
		/* It will call devsw_detach(), which is safe. */
		(void)npf_fini();
	}
#endif
	return error;
}

static int
npf_fini(void)
{
	/* At first, detach device and remove pfil hooks. */
#ifdef _MODULE
	devsw_detach(NULL, &npf_cdevsw);
#endif
	npf_pfil_unregister(true);
	npf_config_fini();

	/* Finally, safe to destroy the subsystems. */
	npf_ext_sysfini();
	npf_alg_sysfini();
	npf_nat_sysfini();
	npf_conn_sysfini();
	npf_tableset_sysfini();
	npf_bpf_sysfini();

	/* Note: worker is the last. */
	npf_worker_sysfini();

	if (npf_sysctl) {
		sysctl_teardown(&npf_sysctl);
	}
	percpu_free(npf_stats_percpu, NPF_STATS_SIZE);

	return 0;
}

/*
 * Module interface.
 */
static int
npf_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		return npf_init();
	case MODULE_CMD_FINI:
		return npf_fini();
	case MODULE_CMD_AUTOUNLOAD:
		if (npf_autounload_p()) {
			return EBUSY;
		}
		break;
	default:
		return ENOTTY;
	}
	return 0;
}

void
npfattach(int nunits)
{

	/* Void. */
}

static int
npf_dev_open(dev_t dev, int flag, int mode, lwp_t *l)
{

	/* Available only for super-user. */
	if (kauth_authorize_network(l->l_cred, KAUTH_NETWORK_FIREWALL,
	    KAUTH_REQ_NETWORK_FIREWALL_FW, NULL, NULL, NULL)) {
		return EPERM;
	}
	return 0;
}

static int
npf_dev_close(dev_t dev, int flag, int mode, lwp_t *l)
{

	return 0;
}

static int
npf_dev_ioctl(dev_t dev, u_long cmd, void *data, int flag, lwp_t *l)
{
	int error;

	/* Available only for super-user. */
	if (kauth_authorize_network(l->l_cred, KAUTH_NETWORK_FIREWALL,
	    KAUTH_REQ_NETWORK_FIREWALL_FW, NULL, NULL, NULL)) {
		return EPERM;
	}

	switch (cmd) {
	case IOC_NPF_TABLE:
		error = npfctl_table(data);
		break;
	case IOC_NPF_RULE:
		error = npfctl_rule(cmd, data);
		break;
	case IOC_NPF_STATS:
		error = npfctl_stats(data);
		break;
	case IOC_NPF_SAVE:
		error = npfctl_save(cmd, data);
		break;
	case IOC_NPF_SWITCH:
		error = npfctl_switch(data);
		break;
	case IOC_NPF_LOAD:
		error = npfctl_load(cmd, data);
		break;
	case IOC_NPF_VERSION:
		*(int *)data = NPF_VERSION;
		error = 0;
		break;
	default:
		error = ENOTTY;
		break;
	}
	return error;
}

static int
npf_dev_poll(dev_t dev, int events, lwp_t *l)
{

	return ENOTSUP;
}

static int
npf_dev_read(dev_t dev, struct uio *uio, int flag)
{

	return ENOTSUP;
}

bool
npf_autounload_p(void)
{
	return !npf_pfil_registered_p() && npf_default_pass();
}

/*
 * NPF statistics interface.
 */

void
npf_stats_inc(npf_stats_t st)
{
	uint64_t *stats = percpu_getref(npf_stats_percpu);
	stats[st]++;
	percpu_putref(npf_stats_percpu);
}

void
npf_stats_dec(npf_stats_t st)
{
	uint64_t *stats = percpu_getref(npf_stats_percpu);
	stats[st]--;
	percpu_putref(npf_stats_percpu);
}

static void
npf_stats_collect(void *mem, void *arg, struct cpu_info *ci)
{
	uint64_t *percpu_stats = mem, *full_stats = arg;
	int i;

	for (i = 0; i < NPF_STATS_COUNT; i++) {
		full_stats[i] += percpu_stats[i];
	}
}

/*
 * npfctl_stats: export collected statistics.
 */
static int
npfctl_stats(void *data)
{
	uint64_t *fullst, *uptr = *(uint64_t **)data;
	int error;

	fullst = kmem_zalloc(NPF_STATS_SIZE, KM_SLEEP);
	percpu_foreach(npf_stats_percpu, npf_stats_collect, fullst);
	error = copyout(fullst, uptr, NPF_STATS_SIZE);
	kmem_free(fullst, NPF_STATS_SIZE);
	return error;
}
