/*	$NetBSD: sysmon_power.c,v 1.56 2015/08/24 22:50:33 pooka Exp $	*/

/*-
 * Copyright (c) 2007 Juan Romero Pardines.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Power management framework for sysmon.
 *
 * We defer to a power management daemon running in userspace, since
 * power management is largely a policy issue.  This merely provides
 * for power management event notification to that daemon.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon_power.c,v 1.56 2015/08/24 22:50:33 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_netbsd.h"
#endif

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/vnode.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/rndsource.h>
#include <sys/module.h>
#include <sys/once.h>

#include <dev/sysmon/sysmonvar.h>
#include <prop/proplib.h>

MODULE(MODULE_CLASS_MISC, sysmon_power, "sysmon");

/*
 * Singly linked list for dictionaries to be stored/sent.
 */
struct power_event_dictionary {
	SIMPLEQ_ENTRY(power_event_dictionary) pev_dict_head;
	prop_dictionary_t dict;
	int flags;
};

struct power_event_description {
	int type;
	const char *desc;
};

/*
 * Available events for power switches.
 */
static const struct power_event_description pswitch_event_desc[] = {
	{ PSWITCH_EVENT_PRESSED, 	"pressed" },
	{ PSWITCH_EVENT_RELEASED,	"released" },
	{ -1, NULL }
};

/*
 * Available script names for power switches.
 */
static const struct power_event_description pswitch_type_desc[] = {
	{ PSWITCH_TYPE_POWER, 		"power_button" },
	{ PSWITCH_TYPE_SLEEP, 		"sleep_button" },
	{ PSWITCH_TYPE_LID, 		"lid_switch" },
	{ PSWITCH_TYPE_RESET, 		"reset_button" },
	{ PSWITCH_TYPE_ACADAPTER,	"acadapter" },
	{ PSWITCH_TYPE_HOTKEY,		"hotkey_button" },
	{ PSWITCH_TYPE_RADIO,		"radio_button" },
	{ -1, NULL }
};

/*
 * Available events for envsys(4).
 */
static const struct power_event_description penvsys_event_desc[] = {
	{ PENVSYS_EVENT_NORMAL, 	"normal" },
	{ PENVSYS_EVENT_CRITICAL,	"critical" },
	{ PENVSYS_EVENT_CRITOVER,	"critical-over" },
	{ PENVSYS_EVENT_CRITUNDER,	"critical-under" },
	{ PENVSYS_EVENT_WARNOVER,	"warning-over" },
	{ PENVSYS_EVENT_WARNUNDER,	"warning-under" },
	{ PENVSYS_EVENT_BATT_CRIT,	"critical-capacity" },
	{ PENVSYS_EVENT_BATT_WARN,	"warning-capacity" },
	{ PENVSYS_EVENT_BATT_HIGH,	"high-capacity" },
	{ PENVSYS_EVENT_BATT_MAX,	"maximum-capacity" },
	{ PENVSYS_EVENT_STATE_CHANGED,	"state-changed" },
	{ PENVSYS_EVENT_LOW_POWER,	"low-power" },
	{ -1, NULL }
};

/*
 * Available script names for envsys(4).
 */
static const struct power_event_description penvsys_type_desc[] = {
	{ PENVSYS_TYPE_BATTERY,		"sensor_battery" },
	{ PENVSYS_TYPE_DRIVE,		"sensor_drive" },
	{ PENVSYS_TYPE_FAN,		"sensor_fan" },
	{ PENVSYS_TYPE_INDICATOR,	"sensor_indicator" },
	{ PENVSYS_TYPE_POWER,		"sensor_power" },
	{ PENVSYS_TYPE_RESISTANCE,	"sensor_resistance" },
	{ PENVSYS_TYPE_TEMP,		"sensor_temperature" },
	{ PENVSYS_TYPE_VOLTAGE,		"sensor_voltage" },
	{ -1, NULL }
};

#define SYSMON_MAX_POWER_EVENTS		32
#define SYSMON_POWER_DICTIONARY_BUSY	0x01
#define SYSMON_POWER_DICTIONARY_READY	0x02

static power_event_t sysmon_power_event_queue[SYSMON_MAX_POWER_EVENTS];
static int sysmon_power_event_queue_head;
static int sysmon_power_event_queue_tail;
static int sysmon_power_event_queue_count;

static krndsource_t sysmon_rndsource;

static SIMPLEQ_HEAD(, power_event_dictionary) pev_dict_list =
    SIMPLEQ_HEAD_INITIALIZER(pev_dict_list);

static struct selinfo sysmon_power_event_queue_selinfo;
static struct lwp *sysmon_power_daemon;

static kmutex_t sysmon_power_event_queue_mtx;
static kcondvar_t sysmon_power_event_queue_cv;

static char sysmon_power_type[32];

static int sysmon_power_make_dictionary(prop_dictionary_t, void *, int, int);
static int sysmon_power_daemon_task(struct power_event_dictionary *,
				    void *, int);
static void sysmon_power_destroy_dictionary(struct power_event_dictionary *);

static struct sysmon_opvec sysmon_power_opvec = {
	sysmonopen_power, sysmonclose_power, sysmonioctl_power,
	sysmonread_power, sysmonpoll_power, sysmonkqfilter_power
};

#define	SYSMON_NEXT_EVENT(x)		(((x) + 1) % SYSMON_MAX_POWER_EVENTS)

ONCE_DECL(once_power);

static int
power_preinit(void)
{

	mutex_init(&sysmon_power_event_queue_mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sysmon_power_event_queue_cv, "smpower");

	return 0;
}

/*
 * sysmon_power_init:
 *
 * 	Initializes the mutexes and condition variables in the
 * 	boot process via module initialization process.
 */
int
sysmon_power_init(void)
{
	int error;

	(void)RUN_ONCE(&once_power, power_preinit);

	selinit(&sysmon_power_event_queue_selinfo);

	rnd_attach_source(&sysmon_rndsource, "system-power",
			  RND_TYPE_POWER, RND_FLAG_DEFAULT);

	error = sysmon_attach_minor(SYSMON_MINOR_POWER, &sysmon_power_opvec);

	return error;
}

int
sysmon_power_fini(void)
{
	int error;

	if (sysmon_power_daemon != NULL)
		error = EBUSY;
	else
		error = sysmon_attach_minor(SYSMON_MINOR_POWER, NULL);

	if (error == 0) {
		rnd_detach_source(&sysmon_rndsource);
		seldestroy(&sysmon_power_event_queue_selinfo);
		cv_destroy(&sysmon_power_event_queue_cv);
		mutex_destroy(&sysmon_power_event_queue_mtx);
	}

	return error;
}

/*
 * sysmon_queue_power_event:
 *
 *	Enqueue a power event for the power management daemon.  Returns
 *	non-zero if we were able to enqueue a power event.
 */
static int
sysmon_queue_power_event(power_event_t *pev)
{
	KASSERT(mutex_owned(&sysmon_power_event_queue_mtx));

	if (sysmon_power_event_queue_count == SYSMON_MAX_POWER_EVENTS)
		return 0;

	sysmon_power_event_queue[sysmon_power_event_queue_head] = *pev;
	sysmon_power_event_queue_head =
	    SYSMON_NEXT_EVENT(sysmon_power_event_queue_head);
	sysmon_power_event_queue_count++;

	return 1;
}

/*
 * sysmon_get_power_event:
 *
 *	Get a power event from the queue.  Returns non-zero if there
 *	is an event available.
 */
static int
sysmon_get_power_event(power_event_t *pev)
{
	KASSERT(mutex_owned(&sysmon_power_event_queue_mtx));

	if (sysmon_power_event_queue_count == 0)
		return 0;

	*pev = sysmon_power_event_queue[sysmon_power_event_queue_tail];
	sysmon_power_event_queue_tail =
	    SYSMON_NEXT_EVENT(sysmon_power_event_queue_tail);
	sysmon_power_event_queue_count--;

	return 1;
}

/*
 * sysmon_power_event_queue_flush:
 *
 *	Flush the event queue, and reset all state.
 */
static void
sysmon_power_event_queue_flush(void)
{
	KASSERT(mutex_owned(&sysmon_power_event_queue_mtx));

	sysmon_power_event_queue_head = 0;
	sysmon_power_event_queue_tail = 0;
	sysmon_power_event_queue_count = 0;
}

/*
 * sysmon_power_daemon_task:
 *
 *	Assign required power event members and sends a signal
 *	to the process to notify that an event was enqueued successfully.
 */
static int
sysmon_power_daemon_task(struct power_event_dictionary *ped,
			 void *pev_data, int event)
{
	power_event_t pev;
	int rv, error = 0;

	if (!ped || !ped->dict || !pev_data)
		return EINVAL;

	mutex_enter(&sysmon_power_event_queue_mtx);
	
	switch (event) {
	/* 
	 * Power switch events.
	 */
	case PSWITCH_EVENT_PRESSED:
	case PSWITCH_EVENT_RELEASED:
	    {

		struct sysmon_pswitch *pswitch =
		    (struct sysmon_pswitch *)pev_data;

		pev.pev_type = POWER_EVENT_SWITCH_STATE_CHANGE;
#ifdef COMPAT_40
		pev.pev_switch.psws_state = event;
		pev.pev_switch.psws_type = pswitch->smpsw_type;

		if (pswitch->smpsw_name) {
			(void)strlcpy(pev.pev_switch.psws_name,
			          pswitch->smpsw_name,
			          sizeof(pev.pev_switch.psws_name));
		}
#endif
		error = sysmon_power_make_dictionary(ped->dict,
						     pswitch,
						     event,
						     pev.pev_type);
		if (error) {
			mutex_exit(&sysmon_power_event_queue_mtx);
			goto out;
		}

		break;
	    }

	/* 
	 * ENVSYS events.
	 */
	case PENVSYS_EVENT_NORMAL:
	case PENVSYS_EVENT_CRITICAL:
	case PENVSYS_EVENT_CRITUNDER:
	case PENVSYS_EVENT_CRITOVER:
	case PENVSYS_EVENT_WARNUNDER:
	case PENVSYS_EVENT_WARNOVER:
	case PENVSYS_EVENT_BATT_CRIT:
	case PENVSYS_EVENT_BATT_WARN:
	case PENVSYS_EVENT_BATT_HIGH:
	case PENVSYS_EVENT_BATT_MAX:
	case PENVSYS_EVENT_STATE_CHANGED:
	case PENVSYS_EVENT_LOW_POWER:
	    {
		struct penvsys_state *penvsys =
		    (struct penvsys_state *)pev_data;

		pev.pev_type = POWER_EVENT_ENVSYS_STATE_CHANGE;

		error = sysmon_power_make_dictionary(ped->dict,
						     penvsys,
						     event, 
						     pev.pev_type);
		if (error) {
			mutex_exit(&sysmon_power_event_queue_mtx);
			goto out;
		}

		break;
	    }
	default:
		error = ENOTTY;
		mutex_exit(&sysmon_power_event_queue_mtx);
		goto out;
	}

	/*
	 * Enqueue the event.
	 */
	rv = sysmon_queue_power_event(&pev);
	if (rv == 0) {
		printf("%s: WARNING: state change event %d lost; "
		    "queue full\n", __func__, pev.pev_type);
		mutex_exit(&sysmon_power_event_queue_mtx);
		error = EINVAL;
		goto out;
	} else {
		/*
		 * Notify the daemon that an event is ready and its
		 * dictionary is ready to be fetched.
		 */
		ped->flags |= SYSMON_POWER_DICTIONARY_READY;
		SIMPLEQ_INSERT_TAIL(&pev_dict_list, ped, pev_dict_head);
		cv_broadcast(&sysmon_power_event_queue_cv);
		mutex_exit(&sysmon_power_event_queue_mtx);
		selnotify(&sysmon_power_event_queue_selinfo, 0, 0);
	}

out:
	return error;
}

/*
 * sysmonopen_power:
 *
 *	Open the system monitor device.
 */
int
sysmonopen_power(dev_t dev, int flag, int mode, struct lwp *l)
{
	int error = 0;

	mutex_enter(&sysmon_power_event_queue_mtx);
	if (sysmon_power_daemon != NULL)
		error = EBUSY;
	else {
		sysmon_power_daemon = l;
		sysmon_power_event_queue_flush();
	}
	mutex_exit(&sysmon_power_event_queue_mtx);

	return error;
}

/*
 * sysmonclose_power:
 *
 *	Close the system monitor device.
 */
int
sysmonclose_power(dev_t dev, int flag, int mode, struct lwp *l)
{
	int count;

	mutex_enter(&sysmon_power_event_queue_mtx);
	count = sysmon_power_event_queue_count;
	sysmon_power_daemon = NULL;
	sysmon_power_event_queue_flush();
	mutex_exit(&sysmon_power_event_queue_mtx);

	if (count)
		printf("WARNING: %d power event%s lost by exiting daemon\n",
		    count, count > 1 ? "s" : "");

	return 0;
}

/*
 * sysmonread_power:
 *
 *	Read the system monitor device.
 */
int
sysmonread_power(dev_t dev, struct uio *uio, int flags)
{
	power_event_t pev;
	int rv;

	/* We only allow one event to be read at a time. */
	if (uio->uio_resid != POWER_EVENT_MSG_SIZE)
		return EINVAL;

	mutex_enter(&sysmon_power_event_queue_mtx);
	for (;;) {
		if (sysmon_get_power_event(&pev)) {
			rv =  uiomove(&pev, POWER_EVENT_MSG_SIZE, uio);
			break;
		}

		if (flags & IO_NDELAY) {
			rv = EWOULDBLOCK;
			break;
		}

		cv_wait(&sysmon_power_event_queue_cv,
			&sysmon_power_event_queue_mtx);
	}
	mutex_exit(&sysmon_power_event_queue_mtx);

	return rv;
}

/*
 * sysmonpoll_power:
 *
 *	Poll the system monitor device.
 */
int
sysmonpoll_power(dev_t dev, int events, struct lwp *l)
{
	int revents;

	revents = events & (POLLOUT | POLLWRNORM);

	/* Attempt to save some work. */
	if ((events & (POLLIN | POLLRDNORM)) == 0)
		return revents;

	mutex_enter(&sysmon_power_event_queue_mtx);
	if (sysmon_power_event_queue_count)
		revents |= events & (POLLIN | POLLRDNORM);
	else
		selrecord(l, &sysmon_power_event_queue_selinfo);
	mutex_exit(&sysmon_power_event_queue_mtx);

	return revents;
}

static void
filt_sysmon_power_rdetach(struct knote *kn)
{

	mutex_enter(&sysmon_power_event_queue_mtx);
	SLIST_REMOVE(&sysmon_power_event_queue_selinfo.sel_klist,
	    kn, knote, kn_selnext);
	mutex_exit(&sysmon_power_event_queue_mtx);
}

static int
filt_sysmon_power_read(struct knote *kn, long hint)
{

	mutex_enter(&sysmon_power_event_queue_mtx);
	kn->kn_data = sysmon_power_event_queue_count;
	mutex_exit(&sysmon_power_event_queue_mtx);

	return kn->kn_data > 0;
}

static const struct filterops sysmon_power_read_filtops =
    { 1, NULL, filt_sysmon_power_rdetach, filt_sysmon_power_read };

static const struct filterops sysmon_power_write_filtops =
    { 1, NULL, filt_sysmon_power_rdetach, filt_seltrue };

/*
 * sysmonkqfilter_power:
 *
 *	Kqueue filter for the system monitor device.
 */
int
sysmonkqfilter_power(dev_t dev, struct knote *kn)
{
	struct klist *klist;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sysmon_power_event_queue_selinfo.sel_klist;
		kn->kn_fop = &sysmon_power_read_filtops;
		break;

	case EVFILT_WRITE:
		klist = &sysmon_power_event_queue_selinfo.sel_klist;
		kn->kn_fop = &sysmon_power_write_filtops;
		break;

	default:
		return EINVAL;
	}

	mutex_enter(&sysmon_power_event_queue_mtx);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mutex_exit(&sysmon_power_event_queue_mtx);

	return 0;
}

/*
 * sysmonioctl_power:
 *
 *	Perform a power management control request.
 */
int
sysmonioctl_power(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	int error = 0;

	switch (cmd) {
	case POWER_IOC_GET_TYPE:
	case POWER_IOC_GET_TYPE_WITH_LOSSAGE:
	    {
		struct power_type *power_type = (void *) data;

		(void)strlcpy(power_type->power_type,
			      sysmon_power_type,
			      sizeof(power_type->power_type));
		break;
	    }
	case POWER_EVENT_RECVDICT:
	    {
		struct plistref *plist = (struct plistref *)data;
		struct power_event_dictionary *ped;

		/*
		 * Get the first dictionary enqueued and mark it
		 * as busy.
		 */
		mutex_enter(&sysmon_power_event_queue_mtx);
		ped = SIMPLEQ_FIRST(&pev_dict_list);
		if (!ped || !ped->dict) {
			mutex_exit(&sysmon_power_event_queue_mtx);
			error = ENOTSUP;
			break;
		}

		if ((ped->flags & SYSMON_POWER_DICTIONARY_READY) == 0) {
			mutex_exit(&sysmon_power_event_queue_mtx);
			error = EINVAL;
			break;
		}

		if (ped->flags & SYSMON_POWER_DICTIONARY_BUSY) {
			mutex_exit(&sysmon_power_event_queue_mtx);
			error = EBUSY;
			break;
		}

		ped->flags |= SYSMON_POWER_DICTIONARY_BUSY;
		mutex_exit(&sysmon_power_event_queue_mtx);

		/*
		 * Send it now.
		 */
		error = prop_dictionary_copyout_ioctl(plist,
						      cmd,
						      ped->dict);

		/*
		 * Remove the dictionary now that we don't need it.
		 */
		mutex_enter(&sysmon_power_event_queue_mtx);
		ped->flags &= ~SYSMON_POWER_DICTIONARY_BUSY;
		ped->flags &= ~SYSMON_POWER_DICTIONARY_READY;
		SIMPLEQ_REMOVE_HEAD(&pev_dict_list, pev_dict_head);
		mutex_exit(&sysmon_power_event_queue_mtx);
		sysmon_power_destroy_dictionary(ped);

		break;
	    }
	default:
		error = ENOTTY;
	}

	return error;
}

/*
 * sysmon_power_make_dictionary:
 *
 * 	Adds the properties for an event in a dictionary.
 */
int
sysmon_power_make_dictionary(prop_dictionary_t dict, void *power_data,
			     int event, int type)
{
	int i;

	KASSERT(mutex_owned(&sysmon_power_event_queue_mtx));

	switch (type) {
	/*
	 * create the dictionary for a power switch event.
	 */
	case POWER_EVENT_SWITCH_STATE_CHANGE:
	    {
		const struct power_event_description *peevent =
		    pswitch_event_desc;
		const struct power_event_description *petype =
		    pswitch_type_desc;
		struct sysmon_pswitch *smpsw =
		    (struct sysmon_pswitch *)power_data;
		const char *pwrtype = "pswitch";

#define SETPROP(key, str)						\
do {									\
	if ((str) != NULL && !prop_dictionary_set_cstring(dict,		\
						  (key),		\
						  (str))) {		\
		printf("%s: failed to set %s\n", __func__, (str));	\
		return EINVAL;						\
	}								\
} while (/* CONSTCOND */ 0)


		SETPROP("driver-name", smpsw->smpsw_name);

		for (i = 0; peevent[i].type != -1; i++)
			if (peevent[i].type == event)
				break;

		SETPROP("powerd-event-name", peevent[i].desc);

		for (i = 0; petype[i].type != -1; i++)
			if (petype[i].type == smpsw->smpsw_type)
				break;

		SETPROP("powerd-script-name", petype[i].desc);
		SETPROP("power-type", pwrtype);
		break;
	    }
	/*
	 * create a dictionary for power envsys event.
	 */
	case POWER_EVENT_ENVSYS_STATE_CHANGE:
	    {
		const struct power_event_description *peevent =
			penvsys_event_desc;
		const struct power_event_description *petype =
			penvsys_type_desc;
		struct penvsys_state *pes =
		    (struct penvsys_state *)power_data;
		const char *pwrtype = "envsys";

		SETPROP("driver-name", pes->pes_dvname);
		SETPROP("sensor-name", pes->pes_sensname);
		SETPROP("state-description", pes->pes_statedesc);

		for (i = 0; peevent[i].type != -1; i++)
			if (peevent[i].type == event)
				break;

		SETPROP("powerd-event-name", peevent[i].desc);

		for (i = 0; petype[i].type != -1; i++)
			if (petype[i].type == pes->pes_type)
				break;

		SETPROP("powerd-script-name", petype[i].desc);
		SETPROP("power-type", pwrtype);
		break;
	    }
	default:
		return ENOTSUP;
	}

	return 0;
}

/*
 * sysmon_power_destroy_dictionary:
 *
 * 	Destroys a power_event_dictionary object and all its
 * 	properties in the dictionary.
 */
static void
sysmon_power_destroy_dictionary(struct power_event_dictionary *ped)
{
	prop_object_iterator_t iter;
	prop_object_t obj;

	KASSERT(ped != NULL);
	KASSERT((ped->flags & SYSMON_POWER_DICTIONARY_BUSY) == 0);

	iter = prop_dictionary_iterator(ped->dict);
	if (iter == NULL)
		return;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_remove(ped->dict,
		    prop_dictionary_keysym_cstring_nocopy(obj));
		prop_object_iterator_reset(iter);
	}

	prop_object_iterator_release(iter);
	prop_object_release(ped->dict);

	kmem_free(ped, sizeof(*ped));
}

/*
 * sysmon_power_settype:
 *
 *	Sets the back-end power management type.  This information can
 *	be used by the power management daemon.
 */
void
sysmon_power_settype(const char *type)
{

	/*
	 * Don't bother locking this; it's going to be set
	 * during autoconfiguration, and then only read from
	 * then on.
	 */
	(void)strlcpy(sysmon_power_type, type, sizeof(sysmon_power_type));
}

#define PENVSYS_SHOWSTATE(str)						\
	do {								\
		printf("%s: %s limit on '%s'\n",			\
		    pes->pes_dvname, (str), pes->pes_sensname);		\
	} while (/* CONSTCOND */ 0)

/*
 * sysmon_penvsys_event:
 *
 * 	Puts an event onto the sysmon power queue and sends the
 * 	appropriate event if the daemon is running, otherwise a
 * 	message is shown.
 */
void
sysmon_penvsys_event(struct penvsys_state *pes, int event)
{
	struct power_event_dictionary *ped;
	const char *mystr = NULL;

	KASSERT(pes != NULL);

	rnd_add_uint32(&sysmon_rndsource, pes->pes_type);

	if (sysmon_power_daemon != NULL) {
		/*
		 * Create a dictionary for the new event.
		 */
		ped = kmem_zalloc(sizeof(*ped), KM_NOSLEEP);
		if (!ped)
			return;
		ped->dict = prop_dictionary_create();

		if (sysmon_power_daemon_task(ped, pes, event) == 0)
			return;
		/* We failed */
		prop_object_release(ped->dict);
		kmem_free(ped, sizeof(*ped));
	}

	switch (pes->pes_type) {
	case PENVSYS_TYPE_BATTERY:
		switch (event) {
		case PENVSYS_EVENT_LOW_POWER:
			printf("sysmon: LOW POWER! SHUTTING DOWN.\n");
			cpu_reboot(RB_POWERDOWN, NULL);
			break;
		case PENVSYS_EVENT_STATE_CHANGED:
			printf("%s: state changed on '%s' to '%s'\n",
			    pes->pes_dvname, pes->pes_sensname,
			    pes->pes_statedesc);
			break;
		case PENVSYS_EVENT_BATT_CRIT:
			mystr = "critical capacity";
			PENVSYS_SHOWSTATE(mystr);
			break;
		case PENVSYS_EVENT_BATT_WARN:
			mystr = "warning capacity";
			PENVSYS_SHOWSTATE(mystr);
			break;
		case PENVSYS_EVENT_BATT_HIGH:
			mystr = "high capacity";
			PENVSYS_SHOWSTATE(mystr);
			break;
		case PENVSYS_EVENT_BATT_MAX:
			mystr = "maximum capacity";
			PENVSYS_SHOWSTATE(mystr);
			break;
		case PENVSYS_EVENT_NORMAL:
			printf("%s: normal capacity on '%s'\n",
			    pes->pes_dvname, pes->pes_sensname);
			break;
		}
		break;
	case PENVSYS_TYPE_FAN:
	case PENVSYS_TYPE_INDICATOR:
	case PENVSYS_TYPE_TEMP:
	case PENVSYS_TYPE_POWER:
	case PENVSYS_TYPE_RESISTANCE:
	case PENVSYS_TYPE_VOLTAGE:
		switch (event) {
		case PENVSYS_EVENT_CRITICAL:
			mystr = "critical";
			PENVSYS_SHOWSTATE(mystr);
			break;
		case PENVSYS_EVENT_CRITOVER:
			mystr = "critical over";
			PENVSYS_SHOWSTATE(mystr);
			break;
		case PENVSYS_EVENT_CRITUNDER:
			mystr = "critical under";
			PENVSYS_SHOWSTATE(mystr);
			break;
		case PENVSYS_EVENT_WARNOVER:
			mystr = "warning over";
			PENVSYS_SHOWSTATE(mystr);
			break;
		case PENVSYS_EVENT_WARNUNDER:
			mystr = "warning under";
			PENVSYS_SHOWSTATE(mystr);
			break;
		case PENVSYS_EVENT_NORMAL:
			printf("%s: normal state on '%s'\n",
			    pes->pes_dvname, pes->pes_sensname);
			break;
		default:
			printf("%s: unknown event\n", __func__);
		}
		break;
	case PENVSYS_TYPE_DRIVE:
		switch (event) {
		case PENVSYS_EVENT_STATE_CHANGED:
			printf("%s: state changed on '%s' to '%s'\n",
			    pes->pes_dvname, pes->pes_sensname,
			    pes->pes_statedesc);
			break;
		case PENVSYS_EVENT_NORMAL:
			printf("%s: normal state on '%s' (%s)\n",
			    pes->pes_dvname, pes->pes_sensname,
			    pes->pes_statedesc);
			break;
		}
		break;
	default:
		printf("%s: unknown power type\n", __func__);
		break;
	}
}

/*
 * sysmon_pswitch_register:
 *
 *	Register a power switch device.
 */
int
sysmon_pswitch_register(struct sysmon_pswitch *smpsw)
{
	(void)RUN_ONCE(&once_power, power_preinit);

	return 0;
}

/*
 * sysmon_pswitch_unregister:
 *
 *	Unregister a power switch device.
 */
void
sysmon_pswitch_unregister(struct sysmon_pswitch *smpsw)
{
	/* nada */
}

/*
 * sysmon_pswitch_event:
 *
 *	Register an event on a power switch device.
 */
void
sysmon_pswitch_event(struct sysmon_pswitch *smpsw, int event)
{
	struct power_event_dictionary *ped = NULL;

	KASSERT(smpsw != NULL);

	/*
	 * For pnp specific events, we don't care if the power daemon
	 * is running or not
	 */
	if (smpsw->smpsw_type == PSWITCH_TYPE_LID) {
		switch (event) {
		case PSWITCH_EVENT_PRESSED:
			pmf_event_inject(NULL, PMFE_CHASSIS_LID_CLOSE);
			break;
		case PSWITCH_EVENT_RELEASED:
			pmf_event_inject(NULL, PMFE_CHASSIS_LID_OPEN);
			break;
		default:
			break;
		}
	}

	if (sysmon_power_daemon != NULL) {
		/*
		 * Create a new dictionary for the event.
		 */
		ped = kmem_zalloc(sizeof(*ped), KM_NOSLEEP);
		if (!ped)
			return;
		ped->dict = prop_dictionary_create();

		if (sysmon_power_daemon_task(ped, smpsw, event) == 0)
			return;
		/* We failed */
		prop_object_release(ped->dict);
		kmem_free(ped, sizeof(*ped));
	}
	
	switch (smpsw->smpsw_type) {
	case PSWITCH_TYPE_POWER:
		if (event != PSWITCH_EVENT_PRESSED) {
			/* just ignore it */
			return;
		}

		/*
		 * Attempt a somewhat graceful shutdown of the system,
		 * as if the user has issued a reboot(2) call with
		 * RB_POWERDOWN.
		 */
		printf("%s: power button pressed, shutting down!\n",
		    smpsw->smpsw_name);
		cpu_reboot(RB_POWERDOWN, NULL);
		break;

	case PSWITCH_TYPE_RESET:
		if (event != PSWITCH_EVENT_PRESSED) {
			/* just ignore it */
			return;
		}

		/*
		 * Attempt a somewhat graceful reboot of the system,
		 * as if the user had issued a reboot(2) call.
		 */
		printf("%s: reset button pressed, rebooting!\n",
		    smpsw->smpsw_name);
		cpu_reboot(0, NULL);
		break;

	case PSWITCH_TYPE_SLEEP:
		if (event != PSWITCH_EVENT_PRESSED) {
			/* just ignore it */
			return;
		}

		/*
		 * Try to enter a "sleep" state.
		 */
		/* XXX */
		printf("%s: sleep button pressed.\n", smpsw->smpsw_name);
		break;

	case PSWITCH_TYPE_HOTKEY:
		/*
		 * Eat up the event, there's nothing we can do
		 */
		break;

	case PSWITCH_TYPE_LID:
		switch (event) {
		case PSWITCH_EVENT_PRESSED:
			/*
			 * Try to enter a "standby" state.
			 */
			/* XXX */
			printf("%s: lid closed.\n", smpsw->smpsw_name);
			break;

		case PSWITCH_EVENT_RELEASED:
			/*
			 * Come out of "standby" state.
			 */
			/* XXX */
			printf("%s: lid opened.\n", smpsw->smpsw_name);
			break;

		default:
			printf("%s: unknown lid switch event: %d\n",
			    smpsw->smpsw_name, event);
		}
		break;

	case PSWITCH_TYPE_ACADAPTER:
		switch (event) {
		case PSWITCH_EVENT_PRESSED:
			/*
			 * Come out of power-save state.
			 */
			aprint_normal("%s: AC adapter online.\n",
			    smpsw->smpsw_name);
			break;

		case PSWITCH_EVENT_RELEASED:
			/*
			 * Try to enter a power-save state.
			 */
			aprint_normal("%s: AC adapter offline.\n",
			    smpsw->smpsw_name);
			break;
		}
		break;

	}
}

static
int   
sysmon_power_modcmd(modcmd_t cmd, void *arg)
{
	int ret;
 
	switch (cmd) { 
	case MODULE_CMD_INIT:
		ret = sysmon_power_init();
		break;
 
	case MODULE_CMD_FINI: 
		ret = sysmon_power_fini();
		break;
 
	case MODULE_CMD_STAT:
	default: 
		ret = ENOTTY;
	}

	return ret;
}

