/*	$NetBSD: sysmon.c,v 1.28 2015/05/05 09:22:33 pgoyette Exp $	*/

/*-
 * Copyright (c) 2000 Zembu Labs, Inc.
 * All rights reserved.
 *
 * Author: Jason R. Thorpe <thorpej@zembu.com>
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
 *	This product includes software developed by Zembu Labs, Inc.
 * 4. Neither the name of Zembu Labs nor the names of its employees may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ZEMBU LABS, INC. ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WAR-
 * RANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DIS-
 * CLAIMED.  IN NO EVENT SHALL ZEMBU LABS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Clearing house for system monitoring hardware.  We currently
 * handle environmental sensors, watchdog timers, and power management.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon.c,v 1.28 2015/05/05 09:22:33 pgoyette Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/device.h>
#include <sys/once.h>

#include <dev/sysmon/sysmonvar.h>

dev_type_open(sysmonopen);
dev_type_close(sysmonclose);
dev_type_ioctl(sysmonioctl);
dev_type_read(sysmonread);
dev_type_poll(sysmonpoll);
dev_type_kqfilter(sysmonkqfilter);

const struct cdevsw sysmon_cdevsw = {
	.d_open = sysmonopen,
	.d_close = sysmonclose,
	.d_read = sysmonread,
	.d_write = nowrite,
	.d_ioctl = sysmonioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = sysmonpoll,
	.d_mmap = nommap,
	.d_kqfilter = sysmonkqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

static int	sysmon_modcmd(modcmd_t, void *); 
static int	sm_init_once(void);

/*
 * Info about our minor "devices"
 */
static struct sysmon_opvec	*sysmon_opvec_table[] = { NULL, NULL, NULL };
static int			sysmon_refcnt[] = { 0, 0, 0 };
static const char		*sysmon_mod[] = { "sysmon_envsys",
						  "sysmon_wdog",
						  "sysmon_power" };
static kmutex_t sysmon_minor_mtx;

#ifdef _MODULE
static bool	sm_is_attached;
#endif

ONCE_DECL(once_sm);

/*
 * sysmon_attach_minor
 *
 *	Attach a minor device for wdog, power, or envsys.  Manage a
 *	reference count so we can prevent the device from being
 *	detached if there are still users with the minor device opened.
 *
 *	If the opvec argument is NULL, this is a request to detach the
 *	minor device - make sure the refcnt is zero!
 */
int
sysmon_attach_minor(int minor, struct sysmon_opvec *opvec)
{
	int ret;

	mutex_enter(&sysmon_minor_mtx);
	if (opvec) {
		if (sysmon_opvec_table[minor] == NULL) {
			sysmon_refcnt[minor] = 0;
			sysmon_opvec_table[minor] = opvec;
			ret = 0;
		} else
			ret = EEXIST;
	} else {
		if (sysmon_refcnt[minor] == 0) {
			sysmon_opvec_table[minor] = NULL;
			ret = 0;
		} else
			ret = EBUSY;
	}

	mutex_exit(&sysmon_minor_mtx);
	return ret;
}

/*
 * sysmonopen:
 *
 *	Open the system monitor device.
 */
int
sysmonopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	int error;

	mutex_enter(&sysmon_minor_mtx);

	switch (minor(dev)) {
	case SYSMON_MINOR_ENVSYS:
	case SYSMON_MINOR_WDOG:
	case SYSMON_MINOR_POWER:
		if (sysmon_opvec_table[minor(dev)] == NULL) {
			mutex_exit(&sysmon_minor_mtx);
			error = module_autoload(sysmon_mod[minor(dev)],
						MODULE_CLASS_MISC);
			if (error)
				return error;
			mutex_enter(&sysmon_minor_mtx);
			if (sysmon_opvec_table[minor(dev)] == NULL) {
				error = ENODEV;
				break;
			}
		}
		error = (sysmon_opvec_table[minor(dev)]->so_open)(dev, flag,
		    mode, l);
		if (error == 0)
			sysmon_refcnt[minor(dev)]++;
		break;
	default:
		error = ENODEV;
	}

	mutex_exit(&sysmon_minor_mtx);
	return error;
}

/*
 * sysmonclose:
 *
 *	Close the system monitor device.
 */
int
sysmonclose(dev_t dev, int flag, int mode, struct lwp *l)
{
	int error;

	switch (minor(dev)) {
	case SYSMON_MINOR_ENVSYS:
	case SYSMON_MINOR_WDOG:
	case SYSMON_MINOR_POWER:
		if (sysmon_opvec_table[minor(dev)] == NULL)
			error = ENODEV;
		else {
			error = (sysmon_opvec_table[minor(dev)]->so_close)(dev,
			    flag, mode, l);
			if (error == 0) {
				sysmon_refcnt[minor(dev)]--;
				KASSERT(sysmon_refcnt[minor(dev)] >= 0);
			}
		}
		break;
	default:
		error = ENODEV;
	}

	return (error);
}

/*
 * sysmonioctl:
 *
 *	Perform a control request.
 */
int
sysmonioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	int error;

	switch (minor(dev)) {
	case SYSMON_MINOR_ENVSYS:
	case SYSMON_MINOR_WDOG:
	case SYSMON_MINOR_POWER:
		if (sysmon_opvec_table[minor(dev)] == NULL)
			error = ENODEV;
		else
			error = (sysmon_opvec_table[minor(dev)]->so_ioctl)(dev,
			    cmd, data, flag, l);
		break;
	default:
		error = ENODEV;
	}

	return (error);
}

/*
 * sysmonread:
 *
 *	Perform a read request.
 */
int
sysmonread(dev_t dev, struct uio *uio, int flags)
{
	int error;

	switch (minor(dev)) {
	case SYSMON_MINOR_POWER:
		if (sysmon_opvec_table[minor(dev)] == NULL)
			error = ENODEV;
		else
			error = (sysmon_opvec_table[minor(dev)]->so_read)(dev,
			    uio, flags);
		break;
	default:
		error = ENODEV;
	}

	return (error);
}

/*
 * sysmonpoll:
 *
 *	Poll the system monitor device.
 */
int
sysmonpoll(dev_t dev, int events, struct lwp *l)
{
	int rv;

	switch (minor(dev)) {
	case SYSMON_MINOR_POWER:
		if (sysmon_opvec_table[minor(dev)] == NULL)
			rv = events;
		else
			rv = (sysmon_opvec_table[minor(dev)]->so_poll)(dev,
			    events, l);
		break;
	default:
		rv = events;
	}

	return (rv);
}

/*
 * sysmonkqfilter:
 *
 *	Kqueue filter for the system monitor device.
 */
int
sysmonkqfilter(dev_t dev, struct knote *kn)
{
	int error;

	switch (minor(dev)) {
	case SYSMON_MINOR_POWER:
		if (sysmon_opvec_table[minor(dev)] == NULL)
			error = ENODEV;
		else
			error = (sysmon_opvec_table[minor(dev)]->so_filter)(dev,
			    kn);
		break;
	default:
		error = 1;
	}

	return (error);
}

MODULE(MODULE_CLASS_DRIVER, sysmon, "");

static int
sm_init_once(void)
{

	mutex_init(&sysmon_minor_mtx, MUTEX_DEFAULT, IPL_NONE);

	return 0;
}

int
sysmon_init(void)
{
	int error;
#ifdef _MODULE
	devmajor_t bmajor, cmajor;
#endif

	error = RUN_ONCE(&once_sm, sm_init_once);

#ifdef _MODULE
	mutex_enter(&sysmon_minor_mtx);
	if (!sm_is_attached) {
		bmajor = cmajor = -1;
		error = devsw_attach("sysmon", NULL, &bmajor,
				&sysmon_cdevsw, &cmajor);
		sm_is_attached = (error != 0);
	}
	mutex_exit(&sysmon_minor_mtx);
#endif

	return error;
}

int
sysmon_fini(void)
{
	int error = 0;

	if ((sysmon_opvec_table[SYSMON_MINOR_ENVSYS] != NULL) ||
	    (sysmon_opvec_table[SYSMON_MINOR_WDOG] != NULL) ||
	    (sysmon_opvec_table[SYSMON_MINOR_POWER] != NULL))
		error = EBUSY;

#ifdef _MODULE
	if (error == 0) {
		mutex_enter(&sysmon_minor_mtx);
		sm_is_attached = false;
		error = devsw_detach(NULL, &sysmon_cdevsw);
		mutex_exit(&sysmon_minor_mtx);
	}
#endif

	return error;
}

static
int   
sysmon_modcmd(modcmd_t cmd, void *arg)
{
	int ret;
 
	switch (cmd) { 
	case MODULE_CMD_INIT:
		ret = sysmon_init();
		break;
 
	case MODULE_CMD_FINI: 
		ret = sysmon_fini(); 
		break;
 
	case MODULE_CMD_STAT:
	default: 
		ret = ENOTTY;
	}
 
	return ret;
}
