/*	$NetBSD: kern_hook.c,v 1.6 2013/11/22 21:04:11 christos Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2002, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Luke Mewburn.
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
__KERNEL_RCSID(0, "$NetBSD: kern_hook.c,v 1.6 2013/11/22 21:04:11 christos Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>
#include <sys/systm.h>
#include <sys/device.h>

/*
 * A generic linear hook.
 */
struct hook_desc {
	LIST_ENTRY(hook_desc) hk_list;
	void	(*hk_fn)(void *);
	void	*hk_arg;
};
typedef LIST_HEAD(, hook_desc) hook_list_t;

int	powerhook_debug = 0;

static void *
hook_establish(hook_list_t *list, void (*fn)(void *), void *arg)
{
	struct hook_desc *hd;

	hd = malloc(sizeof(*hd), M_DEVBUF, M_NOWAIT);
	if (hd == NULL)
		return (NULL);

	hd->hk_fn = fn;
	hd->hk_arg = arg;
	LIST_INSERT_HEAD(list, hd, hk_list);

	return (hd);
}

static void
hook_disestablish(hook_list_t *list, void *vhook)
{
#ifdef DIAGNOSTIC
	struct hook_desc *hd;

	LIST_FOREACH(hd, list, hk_list) {
                if (hd == vhook)
			break;
	}

	if (hd == NULL)
		panic("hook_disestablish: hook %p not established", vhook);
#endif
	LIST_REMOVE((struct hook_desc *)vhook, hk_list);
	free(vhook, M_DEVBUF);
}

static void
hook_destroy(hook_list_t *list)
{
	struct hook_desc *hd;

	while ((hd = LIST_FIRST(list)) != NULL) {
		LIST_REMOVE(hd, hk_list);
		free(hd, M_DEVBUF);
	}
}

static void
hook_proc_run(hook_list_t *list, struct proc *p)
{
	struct hook_desc *hd;

	LIST_FOREACH(hd, list, hk_list)
		((void (*)(struct proc *, void *))*hd->hk_fn)(p, hd->hk_arg);
}

/*
 * "Shutdown hook" types, functions, and variables.
 *
 * Should be invoked immediately before the
 * system is halted or rebooted, i.e. after file systems unmounted,
 * after crash dump done, etc.
 *
 * Each shutdown hook is removed from the list before it's run, so that
 * it won't be run again.
 */

static hook_list_t shutdownhook_list = LIST_HEAD_INITIALIZER(shutdownhook_list);

void *
shutdownhook_establish(void (*fn)(void *), void *arg)
{
	return hook_establish(&shutdownhook_list, fn, arg);
}

void
shutdownhook_disestablish(void *vhook)
{
	hook_disestablish(&shutdownhook_list, vhook);
}

/*
 * Run shutdown hooks.  Should be invoked immediately before the
 * system is halted or rebooted, i.e. after file systems unmounted,
 * after crash dump done, etc.
 *
 * Each shutdown hook is removed from the list before it's run, so that
 * it won't be run again.
 */
void
doshutdownhooks(void)
{
	struct hook_desc *dp;

	while ((dp = LIST_FIRST(&shutdownhook_list)) != NULL) {
		LIST_REMOVE(dp, hk_list);
		(*dp->hk_fn)(dp->hk_arg);
#if 0
		/*
		 * Don't bother freeing the hook structure,, since we may
		 * be rebooting because of a memory corruption problem,
		 * and this might only make things worse.  It doesn't
		 * matter, anyway, since the system is just about to
		 * reboot.
		 */
		free(dp, M_DEVBUF);
#endif
	}
}

/*
 * "Mountroot hook" types, functions, and variables.
 */

static hook_list_t mountroothook_list=LIST_HEAD_INITIALIZER(mountroothook_list);

void *
mountroothook_establish(void (*fn)(device_t), device_t dev)
{
	return hook_establish(&mountroothook_list, (void (*)(void *))fn, dev);
}

void
mountroothook_disestablish(void *vhook)
{
	hook_disestablish(&mountroothook_list, vhook);
}

void
mountroothook_destroy(void)
{
	hook_destroy(&mountroothook_list);
}

void
domountroothook(device_t therootdev)
{
	struct hook_desc *hd;

	LIST_FOREACH(hd, &mountroothook_list, hk_list) {
		if (hd->hk_arg == therootdev) {
			(*hd->hk_fn)(hd->hk_arg);
			return;
		}
	}
}

static hook_list_t exechook_list = LIST_HEAD_INITIALIZER(exechook_list);

void *
exechook_establish(void (*fn)(struct proc *, void *), void *arg)
{
	return hook_establish(&exechook_list, (void (*)(void *))fn, arg);
}

void
exechook_disestablish(void *vhook)
{
	hook_disestablish(&exechook_list, vhook);
}

/*
 * Run exec hooks.
 */
void
doexechooks(struct proc *p)
{
	hook_proc_run(&exechook_list, p);
}

static hook_list_t exithook_list = LIST_HEAD_INITIALIZER(exithook_list);
extern krwlock_t exec_lock;

void *
exithook_establish(void (*fn)(struct proc *, void *), void *arg)
{
	void *rv;

	rw_enter(&exec_lock, RW_WRITER);
	rv = hook_establish(&exithook_list, (void (*)(void *))fn, arg);
	rw_exit(&exec_lock);
	return rv;
}

void
exithook_disestablish(void *vhook)
{

	rw_enter(&exec_lock, RW_WRITER);
	hook_disestablish(&exithook_list, vhook);
	rw_exit(&exec_lock);
}

/*
 * Run exit hooks.
 */
void
doexithooks(struct proc *p)
{
	hook_proc_run(&exithook_list, p);
}

static hook_list_t forkhook_list = LIST_HEAD_INITIALIZER(forkhook_list);

void *
forkhook_establish(void (*fn)(struct proc *, struct proc *))
{
	return hook_establish(&forkhook_list, (void (*)(void *))fn, NULL);
}

void
forkhook_disestablish(void *vhook)
{
	hook_disestablish(&forkhook_list, vhook);
}

/*
 * Run fork hooks.
 */
void
doforkhooks(struct proc *p2, struct proc *p1)
{
	struct hook_desc *hd;

	LIST_FOREACH(hd, &forkhook_list, hk_list) {
		((void (*)(struct proc *, struct proc *))*hd->hk_fn)
		    (p2, p1);
	}
}

static hook_list_t critpollhook_list = LIST_HEAD_INITIALIZER(critpollhook_list);

void *
critpollhook_establish(void (*fn)(void *), void *arg)
{
	return hook_establish(&critpollhook_list, fn, arg);
}

void
critpollhook_disestablish(void *vhook)
{
	hook_disestablish(&critpollhook_list, vhook);
}

/*
 * Run critical polling hooks.
 */
void
docritpollhooks(void)
{
	struct hook_desc *hd;

	LIST_FOREACH(hd, &critpollhook_list, hk_list) {
		(*hd->hk_fn)(hd->hk_arg);
	}
}

/*
 * "Power hook" types, functions, and variables.
 * The list of power hooks is kept ordered with the last registered hook
 * first.
 * When running the hooks on power down the hooks are called in reverse
 * registration order, when powering up in registration order.
 */
struct powerhook_desc {
	TAILQ_ENTRY(powerhook_desc) sfd_list;
	void	(*sfd_fn)(int, void *);
	void	*sfd_arg;
	char	sfd_name[16];
};

static TAILQ_HEAD(powerhook_head, powerhook_desc) powerhook_list =
    TAILQ_HEAD_INITIALIZER(powerhook_list);

void *
powerhook_establish(const char *name, void (*fn)(int, void *), void *arg)
{
	struct powerhook_desc *ndp;

	ndp = (struct powerhook_desc *)
	    malloc(sizeof(*ndp), M_DEVBUF, M_NOWAIT);
	if (ndp == NULL)
		return (NULL);

	ndp->sfd_fn = fn;
	ndp->sfd_arg = arg;
	strlcpy(ndp->sfd_name, name, sizeof(ndp->sfd_name));
	TAILQ_INSERT_HEAD(&powerhook_list, ndp, sfd_list);

	aprint_error("%s: WARNING: powerhook_establish is deprecated\n", name);
	return (ndp);
}

void
powerhook_disestablish(void *vhook)
{
#ifdef DIAGNOSTIC
	struct powerhook_desc *dp;

	TAILQ_FOREACH(dp, &powerhook_list, sfd_list)
                if (dp == vhook)
			goto found;
	panic("powerhook_disestablish: hook %p not established", vhook);
 found:
#endif

	TAILQ_REMOVE(&powerhook_list, (struct powerhook_desc *)vhook,
	    sfd_list);
	free(vhook, M_DEVBUF);
}

/*
 * Run power hooks.
 */
void
dopowerhooks(int why)
{
	struct powerhook_desc *dp;
	const char *why_name;
	static const char * pwr_names[] = {PWR_NAMES};
	why_name = why < __arraycount(pwr_names) ? pwr_names[why] : "???";

	if (why == PWR_RESUME || why == PWR_SOFTRESUME) {
		TAILQ_FOREACH_REVERSE(dp, &powerhook_list, powerhook_head,
		    sfd_list)
		{
			if (powerhook_debug)
				printf("dopowerhooks %s: %s (%p)\n",
				    why_name, dp->sfd_name, dp);
			(*dp->sfd_fn)(why, dp->sfd_arg);
		}
	} else {
		TAILQ_FOREACH(dp, &powerhook_list, sfd_list) {
			if (powerhook_debug)
				printf("dopowerhooks %s: %s (%p)\n",
				    why_name, dp->sfd_name, dp);
			(*dp->sfd_fn)(why, dp->sfd_arg);
		}
	}

	if (powerhook_debug)
		printf("dopowerhooks: %s done\n", why_name);
}
