/*	$NetBSD: sysmon_wdog.c,v 1.28 2015/06/05 00:53:47 matt Exp $	*/

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
 * Watchdog timer framework for sysmon.  Hardware (and software)
 * watchdog timers can register themselves here to provide a
 * watchdog function, which provides an abstract interface to the
 * user.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon_wdog.c,v 1.28 2015/06/05 00:53:47 matt Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/once.h>

#include <dev/sysmon/sysmonvar.h>

static LIST_HEAD(, sysmon_wdog) sysmon_wdog_list =
    LIST_HEAD_INITIALIZER(&sysmon_wdog_list);
static int sysmon_wdog_count;
static kmutex_t sysmon_wdog_list_mtx, sysmon_wdog_mtx;
static kcondvar_t sysmon_wdog_cv;
static struct sysmon_wdog *sysmon_armed_wdog;
static callout_t sysmon_wdog_callout;
static void *sysmon_wdog_sdhook;
static void *sysmon_wdog_cphook;

struct sysmon_wdog *sysmon_wdog_find(const char *);
void	sysmon_wdog_release(struct sysmon_wdog *);
int	sysmon_wdog_setmode(struct sysmon_wdog *, int, u_int);
void	sysmon_wdog_ktickle(void *);
void	sysmon_wdog_critpoll(void *);
void	sysmon_wdog_shutdown(void *);
void	sysmon_wdog_ref(struct sysmon_wdog *);

static struct sysmon_opvec sysmon_wdog_opvec = {    
        sysmonopen_wdog, sysmonclose_wdog, sysmonioctl_wdog,
        NULL, NULL, NULL
};

MODULE(MODULE_CLASS_MISC, sysmon_wdog, "sysmon");

ONCE_DECL(once_wdog);

static int
wdog_preinit(void)
{

	mutex_init(&sysmon_wdog_list_mtx, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sysmon_wdog_mtx, MUTEX_DEFAULT, IPL_SOFTCLOCK);
	cv_init(&sysmon_wdog_cv, "wdogref");
	callout_init(&sysmon_wdog_callout, 0);

	return 0;
}

int
sysmon_wdog_init(void)
{
	int error;

	(void)RUN_ONCE(&once_wdog, wdog_preinit);

	sysmon_wdog_sdhook = shutdownhook_establish(sysmon_wdog_shutdown, NULL);
	if (sysmon_wdog_sdhook == NULL)
		printf("WARNING: unable to register watchdog shutdown hook\n");
	sysmon_wdog_cphook = critpollhook_establish(sysmon_wdog_critpoll, NULL);
	if (sysmon_wdog_cphook == NULL)
		printf("WARNING: unable to register watchdog critpoll hook\n");

	error = sysmon_attach_minor(SYSMON_MINOR_WDOG, &sysmon_wdog_opvec);

	return error;
}

int
sysmon_wdog_fini(void)
{
	int error;

	if ( ! LIST_EMPTY(&sysmon_wdog_list))
		return EBUSY;

	error = sysmon_attach_minor(SYSMON_MINOR_WDOG, NULL);

	if (error == 0) {
		callout_destroy(&sysmon_wdog_callout);
		critpollhook_disestablish(sysmon_wdog_cphook);
		shutdownhook_disestablish(sysmon_wdog_sdhook);
		cv_destroy(&sysmon_wdog_cv);
		mutex_destroy(&sysmon_wdog_mtx);
		mutex_destroy(&sysmon_wdog_list_mtx);
	}

	return error;
}

/*
 * sysmonopen_wdog:
 *
 *	Open the system monitor device.
 */
int
sysmonopen_wdog(dev_t dev, int flag, int mode, struct lwp *l)
{

	return 0;
}

/*
 * sysmonclose_wdog:
 *
 *	Close the system monitor device.
 */
int
sysmonclose_wdog(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct sysmon_wdog *smw;
	int error = 0;

	/*
	 * If this is the last close, and there is a watchdog
	 * running in UTICKLE mode, we need to disable it,
	 * otherwise the system will reset in short order.
	 *
	 * XXX Maybe we should just go into KTICKLE mode?
	 */
	mutex_enter(&sysmon_wdog_mtx);
	if ((smw = sysmon_armed_wdog) != NULL) {
		if ((smw->smw_mode & WDOG_MODE_MASK) == WDOG_MODE_UTICKLE) {
			error = sysmon_wdog_setmode(smw,
			    WDOG_MODE_DISARMED, smw->smw_period);
			if (error) {
				printf("WARNING: UNABLE TO DISARM "
				    "WATCHDOG %s ON CLOSE!\n",
				    smw->smw_name);
				/*
				 * ...we will probably reboot soon.
				 */
			}
		}
	}
	mutex_exit(&sysmon_wdog_mtx);

	return error;
}

/*
 * sysmonioctl_wdog:
 *
 *	Perform a watchdog control request.
 */
int
sysmonioctl_wdog(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct sysmon_wdog *smw;
	int error = 0;

	switch (cmd) {
	case WDOGIOC_GMODE:
	    {
		struct wdog_mode *wm = (void *) data;

		wm->wm_name[sizeof(wm->wm_name) - 1] = '\0';
		smw = sysmon_wdog_find(wm->wm_name);
		if (smw == NULL) {
			error = ESRCH;
			break;
		}

		wm->wm_mode = smw->smw_mode;
		wm->wm_period = smw->smw_period;
		sysmon_wdog_release(smw);
		break;
	    }

	case WDOGIOC_SMODE:
	    {
		struct wdog_mode *wm = (void *) data;

		if ((flag & FWRITE) == 0) {
			error = EPERM;
			break;
		}

		wm->wm_name[sizeof(wm->wm_name) - 1] = '\0';
		smw = sysmon_wdog_find(wm->wm_name);
		if (smw == NULL) {
			error = ESRCH;
			break;
		}

		if (wm->wm_mode & ~(WDOG_MODE_MASK|WDOG_FEATURE_MASK))
			error = EINVAL;
		else {
			mutex_enter(&sysmon_wdog_mtx);
			error = sysmon_wdog_setmode(smw, wm->wm_mode,
			    wm->wm_period);
			mutex_exit(&sysmon_wdog_mtx);
		}

		sysmon_wdog_release(smw);
		break;
	    }

	case WDOGIOC_WHICH:
	    {
		struct wdog_mode *wm = (void *) data;

		mutex_enter(&sysmon_wdog_mtx);
		if ((smw = sysmon_armed_wdog) != NULL) {
			strcpy(wm->wm_name, smw->smw_name);
			wm->wm_mode = smw->smw_mode;
			wm->wm_period = smw->smw_period;
		} else
			error = ESRCH;
		mutex_exit(&sysmon_wdog_mtx);
		break;
	    }

	case WDOGIOC_TICKLE:
		if ((flag & FWRITE) == 0) {
			error = EPERM;
			break;
		}

		mutex_enter(&sysmon_wdog_mtx);
		if ((smw = sysmon_armed_wdog) != NULL) {
			error = (*smw->smw_tickle)(smw);
			if (error == 0)
				smw->smw_tickler = l->l_proc->p_pid;
		} else
			error = ESRCH;
		mutex_exit(&sysmon_wdog_mtx);
		break;

	case WDOGIOC_GTICKLER:
		if ((smw = sysmon_armed_wdog) != NULL)
			*(pid_t *)data = smw->smw_tickler;
		else
			error = ESRCH;
		break;

	case WDOGIOC_GWDOGS:
	    {
		struct wdog_conf *wc = (void *) data;
		char *cp;
		int i;

		mutex_enter(&sysmon_wdog_list_mtx);
		if (wc->wc_names == NULL)
			wc->wc_count = sysmon_wdog_count;
		else {
			for (i = 0, cp = wc->wc_names,
			       smw = LIST_FIRST(&sysmon_wdog_list);
			     i < sysmon_wdog_count && smw != NULL && error == 0;
			     i++, cp += WDOG_NAMESIZE,
			       smw = LIST_NEXT(smw, smw_list))
				error = copyout(smw->smw_name, cp,
				    strlen(smw->smw_name) + 1);
			wc->wc_count = i;
		}
		mutex_exit(&sysmon_wdog_list_mtx);
		break;
	    }

	default:
		error = ENOTTY;
	}

	return error;
}

/*
 * sysmon_wdog_register:
 *
 *	Register a watchdog device.
 */
int
sysmon_wdog_register(struct sysmon_wdog *smw)
{
	struct sysmon_wdog *lsmw;
	int error = 0;

	(void)RUN_ONCE(&once_wdog, wdog_preinit);

	mutex_enter(&sysmon_wdog_list_mtx);

	LIST_FOREACH(lsmw, &sysmon_wdog_list, smw_list) {
		if (strcmp(lsmw->smw_name, smw->smw_name) == 0) {
			error = EEXIST;
			goto out;
		}
	}

	smw->smw_mode = WDOG_MODE_DISARMED;
	smw->smw_tickler = (pid_t) -1;
	smw->smw_refcnt = 0;
	sysmon_wdog_count++;
	LIST_INSERT_HEAD(&sysmon_wdog_list, smw, smw_list);

 out:
	mutex_exit(&sysmon_wdog_list_mtx);
	return error;
}

/*
 * sysmon_wdog_unregister:
 *
 *	Unregister a watchdog device.
 */
int
sysmon_wdog_unregister(struct sysmon_wdog *smw)
{
	int rc = 0;

	mutex_enter(&sysmon_wdog_list_mtx);
	while (smw->smw_refcnt > 0 && rc == 0) {
		aprint_debug("%s: %d users remain\n", smw->smw_name,
		    smw->smw_refcnt);
		rc = cv_wait_sig(&sysmon_wdog_cv, &sysmon_wdog_list_mtx);
	}
	if (rc == 0) {
		sysmon_wdog_count--;
		LIST_REMOVE(smw, smw_list);
	}
	mutex_exit(&sysmon_wdog_list_mtx);
	return rc;
}

/*
 * sysmon_wdog_critpoll:
 *
 *	Perform critical operations during long polling periods
 */
void
sysmon_wdog_critpoll(void *arg)
{
	struct sysmon_wdog *smw = sysmon_armed_wdog;

	if (smw == NULL)
		return;

	if ((smw->smw_mode & WDOG_MODE_MASK) == WDOG_MODE_KTICKLE) {
		if ((*smw->smw_tickle)(smw) != 0) {
			printf("WARNING: KERNEL TICKLE OF WATCHDOG %s "
			    "FAILED!\n", smw->smw_name);
		}
	}
}

/*
 * sysmon_wdog_find:
 *
 *	Find a watchdog device.  We increase the reference
 *	count on a match.
 */
struct sysmon_wdog *
sysmon_wdog_find(const char *name)
{
	struct sysmon_wdog *smw;

	mutex_enter(&sysmon_wdog_list_mtx);

	LIST_FOREACH(smw, &sysmon_wdog_list, smw_list) {
		if (strcmp(smw->smw_name, name) == 0)
			break;
	}

	if (smw != NULL)
		smw->smw_refcnt++;

	mutex_exit(&sysmon_wdog_list_mtx);
	return smw;
}

/*
 * sysmon_wdog_release:
 *
 *	Release a watchdog device.
 */
void
sysmon_wdog_release(struct sysmon_wdog *smw)
{

	mutex_enter(&sysmon_wdog_list_mtx);
	KASSERT(smw->smw_refcnt != 0);
	smw->smw_refcnt--;
	cv_signal(&sysmon_wdog_cv);
	mutex_exit(&sysmon_wdog_list_mtx);
}

void
sysmon_wdog_ref(struct sysmon_wdog *smw)
{
	mutex_enter(&sysmon_wdog_list_mtx);
	smw->smw_refcnt++;
	mutex_exit(&sysmon_wdog_list_mtx);
}

/*
 * sysmon_wdog_setmode:
 *
 *	Set the mode of a watchdog device.
 */
int
sysmon_wdog_setmode(struct sysmon_wdog *smw, int mode, u_int period)
{
	u_int operiod = smw->smw_period;
	int omode = smw->smw_mode;
	int error = 0;

	smw->smw_period = period;
	smw->smw_mode = mode;

	switch (mode & WDOG_MODE_MASK) {
	case WDOG_MODE_DISARMED:
		if (smw != sysmon_armed_wdog) {
			error = EINVAL;
			goto out;
		}
		break;

	case WDOG_MODE_KTICKLE:
	case WDOG_MODE_UTICKLE:
	case WDOG_MODE_ETICKLE:
		if (sysmon_armed_wdog != NULL) {
			error = EBUSY;
			goto out;
		}
		break;

	default:
		error = EINVAL;
		goto out;
	}

	error = (*smw->smw_setmode)(smw);

 out:
	if (error) {
		smw->smw_period = operiod;
		smw->smw_mode = omode;
	} else {
		if ((mode & WDOG_MODE_MASK) == WDOG_MODE_DISARMED) {
			sysmon_armed_wdog = NULL;
			smw->smw_tickler = (pid_t) -1;
			sysmon_wdog_release(smw);
			if ((omode & WDOG_MODE_MASK) == WDOG_MODE_KTICKLE)
				callout_stop(&sysmon_wdog_callout);
		} else {
			sysmon_armed_wdog = smw;
			sysmon_wdog_ref(smw);
			if ((mode & WDOG_MODE_MASK) == WDOG_MODE_KTICKLE) {
				callout_reset(&sysmon_wdog_callout,
				    WDOG_PERIOD_TO_TICKS(smw->smw_period) / 2,
				    sysmon_wdog_ktickle, NULL);
			}
		}
	}
	return error;
}

/*
 * sysmon_wdog_ktickle:
 *
 *	Kernel watchdog tickle routine.
 */
void
sysmon_wdog_ktickle(void *arg)
{
	struct sysmon_wdog *smw;

	mutex_enter(&sysmon_wdog_mtx);
	if ((smw = sysmon_armed_wdog) != NULL) {
		if ((*smw->smw_tickle)(smw) != 0) {
			printf("WARNING: KERNEL TICKLE OF WATCHDOG %s "
			    "FAILED!\n", smw->smw_name);
			/*
			 * ...we will probably reboot soon.
			 */
		}
		callout_reset(&sysmon_wdog_callout,
		    WDOG_PERIOD_TO_TICKS(smw->smw_period) / 2,
		    sysmon_wdog_ktickle, NULL);
	}
	mutex_exit(&sysmon_wdog_mtx);
}

/*
 * sysmon_wdog_shutdown:
 *
 *	Perform shutdown-time operations.
 */
void
sysmon_wdog_shutdown(void *arg)
{
	struct sysmon_wdog *smw;

	/*
	 * XXX Locking here?  I don't think it's necessary.
	 */

	if ((smw = sysmon_armed_wdog) != NULL) {
		if (sysmon_wdog_setmode(smw, WDOG_MODE_DISARMED,
		    smw->smw_period))
			printf("WARNING: FAILED TO SHUTDOWN WATCHDOG %s!\n",
			    smw->smw_name);
	}
}
static
int   
sysmon_wdog_modcmd(modcmd_t cmd, void *arg)
{
        int ret;
  
        switch (cmd) {
        case MODULE_CMD_INIT:
                ret = sysmon_wdog_init();
                break;
 
        case MODULE_CMD_FINI:
                ret = sysmon_wdog_fini();
                break; 
   
        case MODULE_CMD_STAT:
        default:
                ret = ENOTTY;
        }
  
        return ret; 
}
