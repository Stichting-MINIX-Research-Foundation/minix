/* $NetBSD: kern_pmf.c,v 1.37 2015/02/13 13:26:50 maxv Exp $ */

/*-
 * Copyright (c) 2007 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: kern_pmf.c,v 1.37 2015/02/13 13:26:50 maxv Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/buf.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/pmf.h>
#include <sys/queue.h>
#include <sys/sched.h>
#include <sys/workqueue.h>
#include <prop/proplib.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/reboot.h>	/* for RB_NOSYNC */
#include <sys/sched.h>
#include <sys/vfs_syscalls.h>

/* XXX ugly special case, but for now the only client */
#include "wsdisplay.h"
#if NWSDISPLAY > 0
#include <dev/wscons/wsdisplayvar.h>
#endif

#ifndef	PMF_DEBUG
#define PMF_DEBUG
#endif

#ifdef PMF_DEBUG
int pmf_debug_event;
int pmf_debug_suspend;
int pmf_debug_suspensor;
int pmf_debug_idle;
int pmf_debug_transition;

#define	PMF_SUSPENSOR_PRINTF(x)		if (pmf_debug_suspensor) printf x
#define	PMF_SUSPEND_PRINTF(x)		if (pmf_debug_suspend) printf x
#define	PMF_EVENT_PRINTF(x)		if (pmf_debug_event) printf x
#define	PMF_IDLE_PRINTF(x)		if (pmf_debug_idle) printf x
#define	PMF_TRANSITION_PRINTF(x)	if (pmf_debug_transition) printf x
#define	PMF_TRANSITION_PRINTF2(y,x)	if (pmf_debug_transition>y) printf x
#else
#define	PMF_SUSPENSOR_PRINTF(x)		do { } while (0)
#define	PMF_SUSPEND_PRINTF(x)		do { } while (0)
#define	PMF_EVENT_PRINTF(x)		do { } while (0)
#define	PMF_IDLE_PRINTF(x)		do { } while (0)
#define	PMF_TRANSITION_PRINTF(x)	do { } while (0)
#define	PMF_TRANSITION_PRINTF2(y,x)	do { } while (0)
#endif

/* #define PMF_DEBUG */

static prop_dictionary_t pmf_platform = NULL;
static struct workqueue *pmf_event_workqueue;
static struct workqueue *pmf_suspend_workqueue;

typedef struct pmf_event_handler {
	TAILQ_ENTRY(pmf_event_handler) pmf_link;
	pmf_generic_event_t pmf_event;
	void (*pmf_handler)(device_t);
	device_t pmf_device;
	bool pmf_global;
} pmf_event_handler_t;

static TAILQ_HEAD(, pmf_event_handler) pmf_all_events =
    TAILQ_HEAD_INITIALIZER(pmf_all_events);

typedef struct pmf_event_workitem {
	struct work				pew_work;
	pmf_generic_event_t			pew_event;
	device_t				pew_device;
} pmf_event_workitem_t;

typedef struct pmf_suspend_workitem {
	struct work	psw_work;
	device_t	psw_dev;
	pmf_qual_t	psw_qual;
} pmf_suspend_workitem_t;

static struct pool pew_pl;

static pmf_event_workitem_t *pmf_event_workitem_get(void);
static void pmf_event_workitem_put(pmf_event_workitem_t *);

bool pmf_device_resume_locked(device_t, const pmf_qual_t *);
bool pmf_device_suspend_locked(device_t, const pmf_qual_t *);
static bool device_pmf_any_suspensor(device_t, devact_level_t);

static bool
complete_suspension(device_t dev, const device_suspensor_t **susp,
    const pmf_qual_t *pqp)
{
	int i;
	pmf_qual_t pq;
	const device_suspensor_t *ds;

	ds = pmf_qual_suspension(pqp);
	KASSERT(ds->ds_delegator != NULL);

	pq = *pqp;
	pq.pq_suspensor = ds->ds_delegator;

	for (i = 0; i < DEVICE_SUSPENSORS_MAX; i++) {
		if (susp[i] != ds)
			continue;
		if (!pmf_device_suspend(dev, &pq))
			return false;
	}
	return true;
}

static void
pmf_suspend_worker(struct work *wk, void *dummy)
{
	pmf_suspend_workitem_t *psw;
	deviter_t di;
	device_t dev;

	psw = (void *)wk;
	KASSERT(wk == &psw->psw_work);
	KASSERT(psw != NULL);

	for (dev = deviter_first(&di, 0); dev != NULL;
	     dev = deviter_next(&di)) {
		if (dev == psw->psw_dev && device_pmf_lock(dev))
			break;
	}
	deviter_release(&di);

	if (dev == NULL)
		return;

	switch (pmf_qual_depth(&psw->psw_qual)) {
	case DEVACT_LEVEL_FULL:
		if (!complete_suspension(dev, dev->dv_class_suspensors,
		    &psw->psw_qual))
			break;
		/*FALLTHROUGH*/
	case DEVACT_LEVEL_DRIVER:
		if (!complete_suspension(dev, dev->dv_driver_suspensors,
		    &psw->psw_qual))
			break;
		/*FALLTHROUGH*/
	case DEVACT_LEVEL_BUS:
		if (!complete_suspension(dev, dev->dv_bus_suspensors,
		    &psw->psw_qual))
			break;
	}
	device_pmf_unlock(dev);
	kmem_free(psw, sizeof(*psw));
}

static void
pmf_event_worker(struct work *wk, void *dummy)
{
	pmf_event_workitem_t *pew;
	pmf_event_handler_t *event;

	pew = (void *)wk;
	KASSERT(wk == &pew->pew_work);
	KASSERT(pew != NULL);
	
	TAILQ_FOREACH(event, &pmf_all_events, pmf_link) {
		if (event->pmf_event != pew->pew_event)
			continue;
		if (event->pmf_device == pew->pew_device || event->pmf_global)
			(*event->pmf_handler)(event->pmf_device);
	}

	pmf_event_workitem_put(pew);
}

static bool
pmf_check_system_drivers(void)
{
	device_t curdev;
	bool unsupported_devs;
	deviter_t di;

	unsupported_devs = false;
	for (curdev = deviter_first(&di, 0); curdev != NULL;
	     curdev = deviter_next(&di)) {
		if (device_pmf_is_registered(curdev))
			continue;
		if (!unsupported_devs)
			printf("Devices without power management support:");
		printf(" %s", device_xname(curdev));
		unsupported_devs = true;
	}
	deviter_release(&di);
	if (unsupported_devs) {
		printf("\n");
		return false;
	}
	return true;
}

bool
pmf_system_bus_resume(const pmf_qual_t *qual)
{
	bool rv;
	device_t curdev;
	deviter_t di;

	aprint_debug("Powering devices:");
	/* D0 handlers are run in order */
	rv = true;
	for (curdev = deviter_first(&di, DEVITER_F_ROOT_FIRST); curdev != NULL;
	     curdev = deviter_next(&di)) {
		if (!device_pmf_is_registered(curdev))
			continue;
		if (device_is_active(curdev) ||
		    !device_is_enabled(curdev))
			continue;

		aprint_debug(" %s", device_xname(curdev));

		if (!device_pmf_bus_resume(curdev, qual)) {
			rv = false;
			aprint_debug("(failed)");
		}
	}
	deviter_release(&di);
	aprint_debug("\n");

	return rv;
}

bool
pmf_system_resume(const pmf_qual_t *qual)
{
	bool rv;
	device_t curdev, parent;
	deviter_t di;

	if (!pmf_check_system_drivers())
		return false;

	aprint_debug("Resuming devices:");
	/* D0 handlers are run in order */
	rv = true;
	for (curdev = deviter_first(&di, DEVITER_F_ROOT_FIRST); curdev != NULL;
	     curdev = deviter_next(&di)) {
		if (device_is_active(curdev) ||
		    !device_is_enabled(curdev))
			continue;
		parent = device_parent(curdev);
		if (parent != NULL &&
		    !device_is_active(parent))
			continue;

		aprint_debug(" %s", device_xname(curdev));

		if (!pmf_device_resume(curdev, qual)) {
			rv = false;
			aprint_debug("(failed)");
		}
	}
	deviter_release(&di);
	aprint_debug(".\n");

	KERNEL_UNLOCK_ONE(0);
#if NWSDISPLAY > 0
	if (rv)
		wsdisplay_handlex(1);
#endif
	return rv;
}

bool
pmf_system_suspend(const pmf_qual_t *qual)
{
	device_t curdev;
	deviter_t di;

	if (!pmf_check_system_drivers())
		return false;
#if NWSDISPLAY > 0
	if (wsdisplay_handlex(0))
		return false;
#endif
	KERNEL_LOCK(1, NULL);

	/*
	 * Flush buffers only if the shutdown didn't do so
	 * already and if there was no panic.
	 */
	if (doing_shutdown == 0 && panicstr == NULL) {
		printf("Flushing disk caches: ");
		do_sys_sync(&lwp0);
		if (buf_syncwait() != 0)
			printf("giving up\n");
		else
			printf("done\n");
	}

	aprint_debug("Suspending devices:");

	for (curdev = deviter_first(&di, DEVITER_F_LEAVES_FIRST);
	     curdev != NULL;
	     curdev = deviter_next(&di)) {
		if (!device_is_active(curdev))
			continue;

		aprint_debug(" %s", device_xname(curdev));

		/* XXX joerg check return value and abort suspend */
		if (!pmf_device_suspend(curdev, qual))
			aprint_debug("(failed)");
	}
	deviter_release(&di);

	aprint_debug(".\n");

	return true;
}

static bool
shutdown_all(int how)
{
	static struct shutdown_state s;
	device_t curdev;
	bool progress = false;

	for (curdev = shutdown_first(&s); curdev != NULL;
	     curdev = shutdown_next(&s)) {
		aprint_debug(" shutting down %s, ", device_xname(curdev));
		if (!device_pmf_is_registered(curdev))
			aprint_debug("skipped.");
#if 0 /* needed? */
		else if (!device_pmf_class_shutdown(curdev, how))
			aprint_debug("failed.");
#endif
		else if (!device_pmf_driver_shutdown(curdev, how))
			aprint_debug("failed.");
		else if (!device_pmf_bus_shutdown(curdev, how))
			aprint_debug("failed.");
		else {
			progress = true;
			aprint_debug("success.");
		}
	}
	return progress;
}

void
pmf_system_shutdown(int how)
{

	if (panicstr != NULL)
		return;

	aprint_debug("Shutting down devices:");
	shutdown_all(how);
}

bool
pmf_set_platform(const char *key, const char *value)
{
	if (pmf_platform == NULL)
		pmf_platform = prop_dictionary_create();
	if (pmf_platform == NULL)
		return false;

	return prop_dictionary_set_cstring(pmf_platform, key, value);
}

const char *
pmf_get_platform(const char *key)
{
	const char *value;

	if (pmf_platform == NULL)
		return NULL;

	if (!prop_dictionary_get_cstring_nocopy(pmf_platform, key, &value))
		return NULL;

	return value;
}

bool
pmf_device_register1(device_t dev,
    bool (*suspend)(device_t, const pmf_qual_t *),
    bool (*resume)(device_t, const pmf_qual_t *),
    bool (*shutdown)(device_t, int))
{
	if (!device_pmf_driver_register(dev, suspend, resume, shutdown))
		return false;

	if (!device_pmf_driver_child_register(dev)) {
		device_pmf_driver_deregister(dev);
		return false;
	}

	return true;
}

void
pmf_device_deregister(device_t dev)
{
	device_pmf_class_deregister(dev);
	device_pmf_bus_deregister(dev);
	device_pmf_driver_deregister(dev);
}

static const device_suspensor_t _device_suspensor_drvctl = {
	  .ds_delegator = NULL
	, .ds_name = "drvctl"
};

static const device_suspensor_t _device_suspensor_self = {
	  .ds_delegator = NULL
	, .ds_name = "self"
};

#if 0
static const device_suspensor_t _device_suspensor_self_delegate = {
	  .ds_delegator = &_device_suspensor_self
	, .ds_name = "self delegate"
};
#endif

static const device_suspensor_t _device_suspensor_system = {
	  .ds_delegator = NULL
	, .ds_name = "system"
};

const device_suspensor_t
    * const device_suspensor_self = &_device_suspensor_self,
#if 0
    * const device_suspensor_self_delegate = &_device_suspensor_self_delegate,
#endif
    * const device_suspensor_system = &_device_suspensor_system,
    * const device_suspensor_drvctl = &_device_suspensor_drvctl;

static const pmf_qual_t _pmf_qual_system = {
	  .pq_actlvl = DEVACT_LEVEL_FULL
	, .pq_suspensor = &_device_suspensor_system
};

static const pmf_qual_t _pmf_qual_drvctl = {
	  .pq_actlvl = DEVACT_LEVEL_FULL
	, .pq_suspensor = &_device_suspensor_drvctl
};

static const pmf_qual_t _pmf_qual_self = {
	  .pq_actlvl = DEVACT_LEVEL_DRIVER
	, .pq_suspensor = &_device_suspensor_self
};

const pmf_qual_t
    * const PMF_Q_DRVCTL = &_pmf_qual_drvctl,
    * const PMF_Q_NONE = &_pmf_qual_system,
    * const PMF_Q_SELF = &_pmf_qual_self;

static bool
device_suspensor_delegates_to(const device_suspensor_t *ds,
    const device_suspensor_t *delegate)
{
	const device_suspensor_t *iter;

	for (iter = delegate->ds_delegator; iter != NULL;
	     iter = iter->ds_delegator) {
		if (ds == iter)
			return true;
	}
	return false;
}

static bool
add_suspensor(device_t dev, const char *kind, const device_suspensor_t **susp,
    const device_suspensor_t *ds)
{
	int i;

	for (i = 0; i < DEVICE_SUSPENSORS_MAX; i++) {
		if (susp[i] == NULL)
			continue;
		if (ds == susp[i]) {
			PMF_SUSPENSOR_PRINTF((
			    "%s: %s-suspended by %s (delegator %s) already\n",
			    device_xname(dev), kind,
			    susp[i]->ds_name,
			    (susp[i]->ds_delegator != NULL) ?
			    susp[i]->ds_delegator->ds_name : "<none>"));
			return true;
		}
		if (device_suspensor_delegates_to(ds, susp[i])) {
			PMF_SUSPENSOR_PRINTF((
			    "%s: %s assumes %s-suspension by %s "
			    "(delegator %s)\n",
			    device_xname(dev), ds->ds_name, kind,
			    susp[i]->ds_name,
			    (susp[i]->ds_delegator != NULL) ?
			    susp[i]->ds_delegator->ds_name : "<none>"));
			susp[i] = ds;
			return true;
		}
	}
	for (i = 0; i < DEVICE_SUSPENSORS_MAX; i++) {
		if (susp[i] == NULL) {
			susp[i] = ds;
			PMF_SUSPENSOR_PRINTF((
			    "%s: newly %s-suspended by %s (delegator %s)\n",
			    device_xname(dev), kind,
			    susp[i]->ds_name,
			    (susp[i]->ds_delegator != NULL) ?
			    susp[i]->ds_delegator->ds_name : "<none>"));
			return true;
		}
	}
	return false;
}

static bool
device_pmf_add_suspensor(device_t dev, const pmf_qual_t *pq)
{
	const device_suspensor_t *ds;

	KASSERT(pq != NULL);

	ds = pmf_qual_suspension(pq);

	KASSERT(ds != NULL);

	if (!add_suspensor(dev, "class", dev->dv_class_suspensors, ds))
		return false;
	if (!add_suspensor(dev, "driver", dev->dv_driver_suspensors, ds))
		return false;
	if (!add_suspensor(dev, "bus", dev->dv_bus_suspensors, ds))
		return false;
	return true;
}

#if 0
static bool
device_pmf_has_suspension(device_t dev, const device_suspensor_t *ds)
{
	int i;

	for (i = 0; i < DEVICE_SUSPENSORS_MAX; i++) {
		if (dev->dv_suspensions[i] == ds)
			return true;
		if (device_suspensor_delegates_to(dev->dv_suspensions[i], ds))
			return true;
	}
	return false;
}
#endif

static bool
any_suspensor(device_t dev, const char *kind, const device_suspensor_t **susp)
{
	int i;
	bool suspended = false;

	for (i = 0; i < DEVICE_SUSPENSORS_MAX; i++) {
		if (susp[i] != NULL) {
			PMF_SUSPENSOR_PRINTF(("%s: %s is suspended by %s "
			    "(delegator %s)\n",
			    device_xname(dev), kind,
			    susp[i]->ds_name,
			    (susp[i]->ds_delegator != NULL) ?
			    susp[i]->ds_delegator->ds_name : "<none>"));
			suspended = true;
		}
	}
	return suspended;
}

static bool
device_pmf_any_suspensor(device_t dev, devact_level_t depth)
{
	switch (depth) {
	case DEVACT_LEVEL_FULL:
		if (any_suspensor(dev, "class", dev->dv_class_suspensors))
			return true;
		/*FALLTHROUGH*/
	case DEVACT_LEVEL_DRIVER:
		if (any_suspensor(dev, "driver", dev->dv_driver_suspensors))
			return true;
		/*FALLTHROUGH*/
	case DEVACT_LEVEL_BUS:
		if (any_suspensor(dev, "bus", dev->dv_bus_suspensors))
			return true;
	}
	return false;
}

static bool
remove_suspensor(device_t dev, const char *kind,
    const device_suspensor_t **susp, const device_suspensor_t *ds)
{
	int i;

	for (i = 0; i < DEVICE_SUSPENSORS_MAX; i++) {
		if (susp[i] == NULL)
			continue;
		if (ds == susp[i] ||
		    device_suspensor_delegates_to(ds, susp[i])) {
			PMF_SUSPENSOR_PRINTF(("%s: %s suspension %s "
			    "(delegator %s) removed by %s\n",
			    device_xname(dev), kind,
			    susp[i]->ds_name,
			    (susp[i]->ds_delegator != NULL)
			        ?  susp[i]->ds_delegator->ds_name
			        : "<none>",
			    ds->ds_name));
			susp[i] = NULL;
			return true;
		}
	}
	return false;
}

static bool
device_pmf_remove_suspensor(device_t dev, const pmf_qual_t *pq)
{
	const device_suspensor_t *ds;

	KASSERT(pq != NULL);

	ds = pmf_qual_suspension(pq);

	KASSERT(ds != NULL);

	if (!remove_suspensor(dev, "class", dev->dv_class_suspensors, ds))
		return false;
	if (!remove_suspensor(dev, "driver", dev->dv_driver_suspensors, ds))
		return false;
	if (!remove_suspensor(dev, "bus", dev->dv_bus_suspensors, ds))
		return false;

	return true;
}

void
pmf_self_suspensor_init(device_t dev, device_suspensor_t *ds,
    pmf_qual_t *pq)
{
	ds->ds_delegator = device_suspensor_self;
	snprintf(ds->ds_name, sizeof(ds->ds_name), "%s-self",
	    device_xname(dev));
	pq->pq_actlvl = DEVACT_LEVEL_DRIVER;
	pq->pq_suspensor = ds;
}

bool
pmf_device_suspend(device_t dev, const pmf_qual_t *qual)
{
	bool rc;

	PMF_TRANSITION_PRINTF(("%s: suspend enter\n", device_xname(dev)));
	if (!device_pmf_is_registered(dev))
		return false;

	if (!device_pmf_lock(dev))
		return false;

	rc = pmf_device_suspend_locked(dev, qual);

	device_pmf_unlock(dev);

	PMF_TRANSITION_PRINTF(("%s: suspend exit\n", device_xname(dev)));
	return rc;
}

bool
pmf_device_suspend_locked(device_t dev, const pmf_qual_t *qual)
{
	if (!device_pmf_add_suspensor(dev, qual))
		return false;

	PMF_TRANSITION_PRINTF2(1, ("%s: class suspend\n", device_xname(dev)));
	if (!device_pmf_class_suspend(dev, qual))
		return false;

	PMF_TRANSITION_PRINTF2(1, ("%s: driver suspend\n", device_xname(dev)));
	if (!device_pmf_driver_suspend(dev, qual))
		return false;

	PMF_TRANSITION_PRINTF2(1, ("%s: bus suspend\n", device_xname(dev)));
	if (!device_pmf_bus_suspend(dev, qual))
		return false;

	return true;
}

bool
pmf_device_resume(device_t dev, const pmf_qual_t *qual)
{
	bool rc;

	PMF_TRANSITION_PRINTF(("%s: resume enter\n", device_xname(dev)));
	if (!device_pmf_is_registered(dev))
		return false;

	if (!device_pmf_lock(dev))
		return false;

	rc = pmf_device_resume_locked(dev, qual);

	device_pmf_unlock(dev);

	PMF_TRANSITION_PRINTF(("%s: resume exit\n", device_xname(dev)));
	return rc;
}

bool
pmf_device_resume_locked(device_t dev, const pmf_qual_t *qual)
{
	device_pmf_remove_suspensor(dev, qual);

	if (device_pmf_any_suspensor(dev, DEVACT_LEVEL_FULL))
		return true;

	PMF_TRANSITION_PRINTF2(1, ("%s: bus resume\n", device_xname(dev)));
	if (!device_pmf_bus_resume(dev, qual))
		return false;

	PMF_TRANSITION_PRINTF2(1, ("%s: driver resume\n", device_xname(dev)));
	if (!device_pmf_driver_resume(dev, qual))
		return false;

	PMF_TRANSITION_PRINTF2(1, ("%s: class resume\n", device_xname(dev)));
	if (!device_pmf_class_resume(dev, qual))
		return false;

	return true;
}

bool
pmf_device_recursive_suspend(device_t dv, const pmf_qual_t *qual)
{
	bool rv = true;
	device_t curdev;
	deviter_t di;
	pmf_qual_t pq;

	pmf_qual_recursive_copy(&pq, qual);

	for (curdev = deviter_first(&di, 0); curdev != NULL;
	     curdev = deviter_next(&di)) {
		if (device_parent(curdev) != dv)
			continue;
		if (!pmf_device_recursive_suspend(curdev, &pq)) {
			rv = false;
			break;
		}
	}
	deviter_release(&di);

	return rv && pmf_device_suspend(dv, qual);
}

void
pmf_qual_recursive_copy(pmf_qual_t *dst, const pmf_qual_t *src)
{
	*dst = *src;
	dst->pq_actlvl = DEVACT_LEVEL_FULL;
}

bool
pmf_device_recursive_resume(device_t dv, const pmf_qual_t *qual)
{
	device_t parent;
	pmf_qual_t pq;

	if (device_is_active(dv))
		return true;

	pmf_qual_recursive_copy(&pq, qual);

	parent = device_parent(dv);
	if (parent != NULL) {
		if (!pmf_device_recursive_resume(parent, &pq))
			return false;
	}

	return pmf_device_resume(dv, qual);
}

bool
pmf_device_descendants_release(device_t dv, const pmf_qual_t *qual)
{
	bool rv = true;
	device_t curdev;
	deviter_t di;

	for (curdev = deviter_first(&di, 0); curdev != NULL;
	     curdev = deviter_next(&di)) {
		if (device_parent(curdev) != dv)
			continue;
		device_pmf_remove_suspensor(curdev, qual);
		if (!pmf_device_descendants_release(curdev, qual)) {
			rv = false;
			break;
		}
	}
	deviter_release(&di);
	return rv;
}

bool
pmf_device_descendants_resume(device_t dv, const pmf_qual_t *qual)
{
	bool rv = true;
	device_t curdev;
	deviter_t di;

	KASSERT(pmf_qual_descend_ok(qual));

	for (curdev = deviter_first(&di, 0); curdev != NULL;
	     curdev = deviter_next(&di)) {
		if (device_parent(curdev) != dv)
			continue;
		if (!pmf_device_resume(curdev, qual) ||
		    !pmf_device_descendants_resume(curdev, qual)) {
			rv = false;
			break;
		}
	}
	deviter_release(&di);
	return rv;
}

bool
pmf_device_subtree_release(device_t dv, const pmf_qual_t *qual)
{
	pmf_qual_t pq;

	device_pmf_remove_suspensor(dv, qual);

	pmf_qual_recursive_copy(&pq, qual);

	return pmf_device_descendants_release(dv, &pq);
}

bool
pmf_device_subtree_resume(device_t dv, const pmf_qual_t *qual)
{
	pmf_qual_t pq;

	if (!pmf_device_subtree_release(dv, qual))
		return false;

	if (!pmf_device_recursive_resume(dv, qual))
		return false;

	pmf_qual_recursive_copy(&pq, qual);

	return pmf_device_descendants_resume(dv, &pq);
}

#include <net/if.h>

static bool
pmf_class_network_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct ifnet *ifp = device_pmf_class_private(dev);
	int s;

	s = splnet();
	(*ifp->if_stop)(ifp, 0);
	splx(s);

	return true;
}

static bool
pmf_class_network_resume(device_t dev, const pmf_qual_t *qual)
{
	struct ifnet *ifp = device_pmf_class_private(dev);
	int s;

	s = splnet();
	if (ifp->if_flags & IFF_UP) {
		ifp->if_flags &= ~IFF_RUNNING;
		if ((*ifp->if_init)(ifp) != 0)
			aprint_normal_ifnet(ifp, "resume failed\n");
		(*ifp->if_start)(ifp);
	}
	splx(s);

	return true;
}

void
pmf_class_network_register(device_t dev, struct ifnet *ifp)
{
	device_pmf_class_register(dev, ifp, pmf_class_network_suspend,
	    pmf_class_network_resume, NULL);
}

bool
pmf_event_inject(device_t dv, pmf_generic_event_t ev)
{
	pmf_event_workitem_t *pew;

	pew = pmf_event_workitem_get();
	if (pew == NULL) {
		PMF_EVENT_PRINTF(("%s: PMF event %d dropped (no memory)\n",
		    dv ? device_xname(dv) : "<anonymous>", ev));
		return false;
	}

	pew->pew_event = ev;
	pew->pew_device = dv;

	workqueue_enqueue(pmf_event_workqueue, &pew->pew_work, NULL);
	PMF_EVENT_PRINTF(("%s: PMF event %d injected\n",
	    dv ? device_xname(dv) : "<anonymous>", ev));

	return true;
}

bool
pmf_event_register(device_t dv, pmf_generic_event_t ev,
    void (*handler)(device_t), bool global)
{
	pmf_event_handler_t *event; 
	
	event = kmem_alloc(sizeof(*event), KM_SLEEP);
	event->pmf_event = ev;
	event->pmf_handler = handler;
	event->pmf_device = dv;
	event->pmf_global = global;
	TAILQ_INSERT_TAIL(&pmf_all_events, event, pmf_link);

	return true;
}

void
pmf_event_deregister(device_t dv, pmf_generic_event_t ev,
    void (*handler)(device_t), bool global)
{
	pmf_event_handler_t *event;

	TAILQ_FOREACH(event, &pmf_all_events, pmf_link) {
		if (event->pmf_event != ev)
			continue;
		if (event->pmf_device != dv)
			continue;
		if (event->pmf_global != global)
			continue;
		if (event->pmf_handler != handler)
			continue;
		TAILQ_REMOVE(&pmf_all_events, event, pmf_link);
		kmem_free(event, sizeof(*event));
		return;
	}
}

struct display_class_softc {
	TAILQ_ENTRY(display_class_softc) dc_link;
	device_t dc_dev;
};

static TAILQ_HEAD(, display_class_softc) all_displays;
static callout_t global_idle_counter;
static int idle_timeout = 30;

static void
input_idle(void *dummy)
{
	PMF_IDLE_PRINTF(("Input idle handler called\n"));
	pmf_event_inject(NULL, PMFE_DISPLAY_OFF);
}

static void
input_activity_handler(device_t dv, devactive_t type)
{
	if (!TAILQ_EMPTY(&all_displays))
		callout_schedule(&global_idle_counter, idle_timeout * hz);
}

static void
pmf_class_input_deregister(device_t dv)
{
	device_active_deregister(dv, input_activity_handler);
}

bool
pmf_class_input_register(device_t dv)
{
	if (!device_active_register(dv, input_activity_handler))
		return false;
	
	device_pmf_class_register(dv, NULL, NULL, NULL,
	    pmf_class_input_deregister);

	return true;
}

static void
pmf_class_display_deregister(device_t dv)
{
	struct display_class_softc *sc = device_pmf_class_private(dv);
	int s;

	s = splsoftclock();
	TAILQ_REMOVE(&all_displays, sc, dc_link);
	if (TAILQ_EMPTY(&all_displays))
		callout_stop(&global_idle_counter);
	splx(s);

	kmem_free(sc, sizeof(*sc));
}

bool
pmf_class_display_register(device_t dv)
{
	struct display_class_softc *sc;
	int s;

	sc = kmem_alloc(sizeof(*sc), KM_SLEEP);

	s = splsoftclock();
	if (TAILQ_EMPTY(&all_displays))
		callout_schedule(&global_idle_counter, idle_timeout * hz);

	TAILQ_INSERT_HEAD(&all_displays, sc, dc_link);
	splx(s);

	device_pmf_class_register(dv, sc, NULL, NULL,
	    pmf_class_display_deregister);

	return true;
}

static void
pmf_event_workitem_put(pmf_event_workitem_t *pew)
{

	KASSERT(pew != NULL);
	pool_put(&pew_pl, pew);
}

static pmf_event_workitem_t *
pmf_event_workitem_get(void)
{

	return pool_get(&pew_pl, PR_NOWAIT);
}

void
pmf_init(void)
{
	int err;

	pool_init(&pew_pl, sizeof(pmf_event_workitem_t), 0, 0, 0,
	    "pewpl", NULL, IPL_HIGH);
	pool_setlowat(&pew_pl, 1);
	pool_sethiwat(&pew_pl, 8);

	KASSERT(pmf_event_workqueue == NULL);
	err = workqueue_create(&pmf_event_workqueue, "pmfevent",
	    pmf_event_worker, NULL, PRI_NONE, IPL_VM, 0);
	if (err)
		panic("couldn't create pmfevent workqueue");

	KASSERT(pmf_suspend_workqueue == NULL);
	err = workqueue_create(&pmf_suspend_workqueue, "pmfsuspend",
	    pmf_suspend_worker, NULL, PRI_NONE, IPL_VM, 0);
	if (err)
		panic("couldn't create pmfsuspend workqueue");


	callout_init(&global_idle_counter, 0);
	callout_setfunc(&global_idle_counter, input_idle, NULL);
}
