/* $NetBSD: kern_drvctl.c,v 1.39 2015/08/20 09:45:45 christos Exp $ */

/*
 * Copyright (c) 2004
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
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
__KERNEL_RCSID(0, "$NetBSD: kern_drvctl.c,v 1.39 2015/08/20 09:45:45 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/event.h>
#include <sys/kmem.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/drvctlio.h>
#include <sys/devmon.h>
#include <sys/stat.h>
#include <sys/kauth.h>
#include <sys/lwp.h>

#include "ioconf.h"

struct drvctl_event {
	TAILQ_ENTRY(drvctl_event) dce_link;
	prop_dictionary_t	dce_event;
};

TAILQ_HEAD(drvctl_queue, drvctl_event);

static struct drvctl_queue	drvctl_eventq;		/* FIFO */
static kcondvar_t		drvctl_cond;
static kmutex_t			drvctl_lock;
static int			drvctl_nopen = 0, drvctl_eventcnt = 0;
static struct selinfo		drvctl_rdsel;

#define DRVCTL_EVENTQ_DEPTH	64	/* arbitrary queue limit */

dev_type_open(drvctlopen);

const struct cdevsw drvctl_cdevsw = {
	.d_open = drvctlopen,
	.d_close = nullclose,
	.d_read = nullread,
	.d_write = nullwrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static int	drvctl_read(struct file *, off_t *, struct uio *,
			    kauth_cred_t, int);
static int	drvctl_write(struct file *, off_t *, struct uio *,
			     kauth_cred_t, int);
static int	drvctl_ioctl(struct file *, u_long, void *);
static int	drvctl_poll(struct file *, int);
static int	drvctl_stat(struct file *, struct stat *);
static int	drvctl_close(struct file *);

static const struct fileops drvctl_fileops = {
	.fo_read = drvctl_read,
	.fo_write = drvctl_write,
	.fo_ioctl = drvctl_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = drvctl_poll,
	.fo_stat = drvctl_stat,
	.fo_close = drvctl_close,
	.fo_kqfilter = fnullop_kqfilter,
	.fo_restart = fnullop_restart,
};

#define MAXLOCATORS 100

static int drvctl_command(struct lwp *, struct plistref *, u_long, int);
static int drvctl_getevent(struct lwp *, struct plistref *, u_long, int);

void
drvctl_init(void)
{
	TAILQ_INIT(&drvctl_eventq);
	mutex_init(&drvctl_lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&drvctl_cond, "devmon");
	selinit(&drvctl_rdsel);
}

void
devmon_insert(const char *event, prop_dictionary_t ev)
{
	struct drvctl_event *dce, *odce;

	mutex_enter(&drvctl_lock);

	if (drvctl_nopen == 0) {
		prop_object_release(ev);
		mutex_exit(&drvctl_lock);
		return;
	}

	/* Fill in mandatory member */
	if (!prop_dictionary_set_cstring_nocopy(ev, "event", event)) {
		prop_object_release(ev);
		mutex_exit(&drvctl_lock);
		return;
	}

	dce = kmem_alloc(sizeof(*dce), KM_SLEEP);
	if (dce == NULL) {
		prop_object_release(ev);
		mutex_exit(&drvctl_lock);
		return;
	}

	dce->dce_event = ev;

	if (drvctl_eventcnt == DRVCTL_EVENTQ_DEPTH) {
		odce = TAILQ_FIRST(&drvctl_eventq);
		TAILQ_REMOVE(&drvctl_eventq, odce, dce_link);
		prop_object_release(odce->dce_event);
		kmem_free(odce, sizeof(*odce));
		--drvctl_eventcnt;
	}

	TAILQ_INSERT_TAIL(&drvctl_eventq, dce, dce_link);
	++drvctl_eventcnt;
	cv_broadcast(&drvctl_cond);
	selnotify(&drvctl_rdsel, 0, 0);

	mutex_exit(&drvctl_lock);
}

int
drvctlopen(dev_t dev, int flags, int mode, struct lwp *l)
{
	struct file *fp;
	int fd;
	int ret;

	ret = fd_allocfile(&fp, &fd);
	if (ret)
		return ret;

	/* XXX setup context */
	mutex_enter(&drvctl_lock);
	ret = fd_clone(fp, fd, flags, &drvctl_fileops, /* context */NULL);
	++drvctl_nopen;
	mutex_exit(&drvctl_lock);

	return ret;
}

static int
pmdevbyname(u_long cmd, struct devpmargs *a)
{
	device_t d;

	if ((d = device_find_by_xname(a->devname)) == NULL)
		return ENXIO;

	switch (cmd) {
	case DRVSUSPENDDEV:
		return pmf_device_recursive_suspend(d, PMF_Q_DRVCTL) ? 0 : EBUSY;
	case DRVRESUMEDEV:
		if (a->flags & DEVPM_F_SUBTREE) {
			return pmf_device_subtree_resume(d, PMF_Q_DRVCTL)
			    ? 0 : EBUSY;
		} else {
			return pmf_device_recursive_resume(d, PMF_Q_DRVCTL)
			    ? 0 : EBUSY;
		}
	default:
		return EPASSTHROUGH;
	}
}

static int
listdevbyname(struct devlistargs *l)
{
	device_t d, child;
	deviter_t di;
	int cnt = 0, idx, error = 0;

	if (*l->l_devname == '\0')
		d = NULL;
	else if (memchr(l->l_devname, 0, sizeof(l->l_devname)) == NULL)
		return EINVAL;
	else if ((d = device_find_by_xname(l->l_devname)) == NULL)
		return ENXIO;

	for (child = deviter_first(&di, 0); child != NULL;
	     child = deviter_next(&di)) {
		if (device_parent(child) != d)
			continue;
		idx = cnt++;
		if (l->l_childname == NULL || idx >= l->l_children)
			continue;
		error = copyoutstr(device_xname(child), l->l_childname[idx],
				sizeof(l->l_childname[idx]), NULL);
		if (error != 0)
			break;
	}
	deviter_release(&di);

	l->l_children = cnt;
	return error;
}

static int
detachdevbyname(const char *devname)
{
	device_t d;

	if ((d = device_find_by_xname(devname)) == NULL)
		return ENXIO;

#ifndef XXXFULLRISK
	/*
	 * If the parent cannot be notified, it might keep
	 * pointers to the detached device.
	 * There might be a private notification mechanism,
	 * but better play it safe here.
	 */
	if (d->dv_parent && !d->dv_parent->dv_cfattach->ca_childdetached)
		return ENOTSUP;
#endif
	return config_detach(d, 0);
}

static int
rescanbus(const char *busname, const char *ifattr,
	  int numlocators, const int *locators)
{
	int i, rc;
	device_t d;
	const struct cfiattrdata * const *ap;

	/* XXX there should be a way to get limits and defaults (per device)
	   from config generated data */
	int locs[MAXLOCATORS];
	for (i = 0; i < MAXLOCATORS; i++)
		locs[i] = -1;

	for (i = 0; i < numlocators;i++)
		locs[i] = locators[i];

	if ((d = device_find_by_xname(busname)) == NULL)
		return ENXIO;

	/*
	 * must support rescan, and must have something
	 * to attach to
	 */
	if (!d->dv_cfattach->ca_rescan ||
	    !d->dv_cfdriver->cd_attrs)
		return ENODEV;

	/* allow to omit attribute if there is exactly one */
	if (!ifattr) {
		if (d->dv_cfdriver->cd_attrs[1])
			return EINVAL;
		ifattr = d->dv_cfdriver->cd_attrs[0]->ci_name;
	} else {
		/* check for valid attribute passed */
		for (ap = d->dv_cfdriver->cd_attrs; *ap; ap++)
			if (!strcmp((*ap)->ci_name, ifattr))
				break;
		if (!*ap)
			return EINVAL;
	}

	rc = (*d->dv_cfattach->ca_rescan)(d, ifattr, locs);
	config_deferred(NULL);
	return rc;
}

static int
drvctl_read(struct file *fp, off_t *offp, struct uio *uio, kauth_cred_t cred,
    int flags)
{
	return ENODEV;
}

static int
drvctl_write(struct file *fp, off_t *offp, struct uio *uio, kauth_cred_t cred,
    int flags)
{
	return ENODEV;
}

static int
drvctl_ioctl(struct file *fp, u_long cmd, void *data)
{
	int res;
	char *ifattr;
	int *locs;
	size_t locs_sz = 0; /* XXXgcc */

	switch (cmd) {
	case DRVSUSPENDDEV:
	case DRVRESUMEDEV:
#define d ((struct devpmargs *)data)
		res = pmdevbyname(cmd, d);
#undef d
		break;
	case DRVLISTDEV:
		res = listdevbyname((struct devlistargs *)data);
		break;
	case DRVDETACHDEV:
#define d ((struct devdetachargs *)data)
		res = detachdevbyname(d->devname);
#undef d
		break;
	case DRVRESCANBUS:
#define d ((struct devrescanargs *)data)
		d->busname[sizeof(d->busname) - 1] = '\0';

		/* XXX better copyin? */
		if (d->ifattr[0]) {
			d->ifattr[sizeof(d->ifattr) - 1] = '\0';
			ifattr = d->ifattr;
		} else
			ifattr = 0;

		if (d->numlocators) {
			if (d->numlocators > MAXLOCATORS)
				return EINVAL;
			locs_sz = d->numlocators * sizeof(int);
			locs = kmem_alloc(locs_sz, KM_SLEEP);
			res = copyin(d->locators, locs, locs_sz);
			if (res) {
				kmem_free(locs, locs_sz);
				return res;
			}
		} else
			locs = NULL;
		res = rescanbus(d->busname, ifattr, d->numlocators, locs);
		if (locs)
			kmem_free(locs, locs_sz);
#undef d
		break;
	case DRVCTLCOMMAND:
	    	res = drvctl_command(curlwp, (struct plistref *)data, cmd,
		    fp->f_flag);
	    	break;
	case DRVGETEVENT:
		res = drvctl_getevent(curlwp, (struct plistref *)data, cmd,
		    fp->f_flag);
		break;
	default:
		return EPASSTHROUGH;
	}
	return res;
}

static int
drvctl_stat(struct file *fp, struct stat *st)
{
	(void)memset(st, 0, sizeof(*st));
	st->st_uid = kauth_cred_geteuid(fp->f_cred);
	st->st_gid = kauth_cred_getegid(fp->f_cred);
	return 0;
}

static int
drvctl_poll(struct file *fp, int events)
{
	int revents = 0;

	if (!TAILQ_EMPTY(&drvctl_eventq))
		revents |= events & (POLLIN | POLLRDNORM);
	else
		selrecord(curlwp, &drvctl_rdsel);

	return revents;
}

static int
drvctl_close(struct file *fp)
{
	struct drvctl_event *dce;

	/* XXX free context */
	mutex_enter(&drvctl_lock);
	KASSERT(drvctl_nopen > 0);
	--drvctl_nopen;
	if (drvctl_nopen == 0) {
		/* flush queue */
		while ((dce = TAILQ_FIRST(&drvctl_eventq)) != NULL) {
			TAILQ_REMOVE(&drvctl_eventq, dce, dce_link);
			KASSERT(drvctl_eventcnt > 0);
			--drvctl_eventcnt;
			prop_object_release(dce->dce_event);
			kmem_free(dce, sizeof(*dce));
		}
	}
	mutex_exit(&drvctl_lock);

	return 0;
}

void
drvctlattach(int arg __unused)
{
}

/*****************************************************************************
 * Driver control command processing engine
 *****************************************************************************/

static int
drvctl_command_get_properties(struct lwp *l,
			      prop_dictionary_t command_dict,
			      prop_dictionary_t results_dict)
{
	prop_dictionary_t args_dict;
	prop_string_t devname_string;
	device_t dev;
	deviter_t di;
	
	args_dict = prop_dictionary_get(command_dict, "drvctl-arguments");
	if (args_dict == NULL)
		return EINVAL;

	devname_string = prop_dictionary_get(args_dict, "device-name");
	if (devname_string == NULL)
		return EINVAL;
	
	for (dev = deviter_first(&di, 0); dev != NULL;
	     dev = deviter_next(&di)) {
		if (prop_string_equals_cstring(devname_string,
					       device_xname(dev))) {
			prop_dictionary_set(results_dict, "drvctl-result-data",
			    device_properties(dev));
			break;
		}
	}

	deviter_release(&di);

	if (dev == NULL)
		return ESRCH;

	return 0;
}

struct drvctl_command_desc {
	const char *dcd_name;		/* command name */
	int (*dcd_func)(struct lwp *,	/* handler function */
			prop_dictionary_t,
			prop_dictionary_t);
	int dcd_rw;			/* read or write required */
};

static const struct drvctl_command_desc drvctl_command_table[] = {
	{ .dcd_name = "get-properties",
	  .dcd_func = drvctl_command_get_properties,
	  .dcd_rw   = FREAD,
	},

	{ .dcd_name = NULL }
};

static int
drvctl_command(struct lwp *l, struct plistref *pref, u_long ioctl_cmd,
	       int fflag)
{
	prop_dictionary_t command_dict, results_dict;
	prop_string_t command_string;
	const struct drvctl_command_desc *dcd;
	int error;

	error = prop_dictionary_copyin_ioctl(pref, ioctl_cmd, &command_dict);
	if (error)
		return error;

	results_dict = prop_dictionary_create();
	if (results_dict == NULL) {
		prop_object_release(command_dict);
		return ENOMEM;
	}
	
	command_string = prop_dictionary_get(command_dict, "drvctl-command");
	if (command_string == NULL) {
		error = EINVAL;
		goto out;
	}

	for (dcd = drvctl_command_table; dcd->dcd_name != NULL; dcd++) {
		if (prop_string_equals_cstring(command_string,
					       dcd->dcd_name))
			break;
	}

	if (dcd->dcd_name == NULL) {
		error = EINVAL;
		goto out;
	}

	if ((fflag & dcd->dcd_rw) == 0) {
		error = EPERM;
		goto out;
	}

	error = (*dcd->dcd_func)(l, command_dict, results_dict);

	prop_dictionary_set_int32(results_dict, "drvctl-error", error);

	error = prop_dictionary_copyout_ioctl(pref, ioctl_cmd, results_dict);
 out:
	prop_object_release(command_dict);
	prop_object_release(results_dict);
	return error;
}

static int
drvctl_getevent(struct lwp *l, struct plistref *pref, u_long ioctl_cmd,
	        int fflag)
{
	struct drvctl_event *dce;
	int ret;

	if ((fflag & (FREAD|FWRITE)) != (FREAD|FWRITE))
		return EPERM;

	mutex_enter(&drvctl_lock);
	while ((dce = TAILQ_FIRST(&drvctl_eventq)) == NULL) {
		if (fflag & O_NONBLOCK) {
			mutex_exit(&drvctl_lock);
			return EWOULDBLOCK;
		}

		ret = cv_wait_sig(&drvctl_cond, &drvctl_lock);
		if (ret) {
			mutex_exit(&drvctl_lock);
			return ret;
		}
	}
	TAILQ_REMOVE(&drvctl_eventq, dce, dce_link);
	KASSERT(drvctl_eventcnt > 0);
	--drvctl_eventcnt;
	mutex_exit(&drvctl_lock);

	ret = prop_dictionary_copyout_ioctl(pref, ioctl_cmd, dce->dce_event);

	prop_object_release(dce->dce_event);
	kmem_free(dce, sizeof(*dce));

	return ret;
}
