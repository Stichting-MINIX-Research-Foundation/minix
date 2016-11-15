/*	$NetBSD: sysmonvar.h,v 1.49 2015/04/23 23:22:03 pgoyette Exp $	*/

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

#ifndef _DEV_SYSMON_SYSMONVAR_H_
#define	_DEV_SYSMON_SYSMONVAR_H_

#include <sys/param.h>
#include <sys/envsys.h>
#include <sys/wdog.h>
#include <sys/power.h>
#include <sys/queue.h>
#include <sys/callout.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/rndsource.h>

struct lwp;
struct proc;
struct knote;
struct uio;
struct workqueue;

#define	SYSMON_MINOR_ENVSYS	0
#define	SYSMON_MINOR_WDOG	1
#define	SYSMON_MINOR_POWER	2

/*****************************************************************************
 * Environmental sensor support
 *****************************************************************************/

/*
 * Thresholds/limits that are being monitored
 */

enum envsys_lims {
	ENVSYS_LIM_CRITMAX,
	ENVSYS_LIM_WARNMAX,
	ENVSYS_LIM_WARNMIN,
	ENVSYS_LIM_CRITMIN,
	ENVSYS_LIM_LASTLIM
};

struct sysmon_envsys_lim {
	int32_t critmax;
	int32_t warnmax;
	int32_t warnmin;
	int32_t critmin;
};

typedef union {
	int32_t sel_limit_list[ENVSYS_LIM_LASTLIM];
	struct sysmon_envsys_lim sel_limits;
} sysmon_envsys_lim_t;

#define	sel_critmax sel_limits.critmax
#define	sel_warnmax sel_limits.warnmax
#define	sel_warnmin sel_limits.warnmin
#define	sel_critmin sel_limits.critmin

/* struct used by a sensor */
struct envsys_data {
	TAILQ_ENTRY(envsys_data)	sensors_head;
	uint32_t	sensor;		/* sensor number */
	uint32_t	units;		/* type of sensor */
	uint32_t	state;		/* sensor state */
	uint32_t	flags;		/* sensor flags */
	uint32_t	rpms;		/* for fans, nominal RPMs */
	int32_t		rfact;		/* for volts, factor x 10^4 */
	int32_t		value_cur;	/* current value */
	int32_t		value_prev;	/* previous value */
	int32_t		value_max;	/* max value */
	int32_t		value_min;	/* min value */
	int32_t		private;	/* private data for drivers */
	sysmon_envsys_lim_t limits;	/* thresholds for monitoring */
	uint32_t	upropset;	/* userland property set? */
	krndsource_t	rnd_src;	/* source element for rnd(4) */
	char		desc[ENVSYS_DESCLEN];	/* sensor description */
};

typedef struct envsys_data envsys_data_t;

/* sensor flags */
#define ENVSYS_FPERCENT 	0x00000001	/* sensor wants a percentage */
#define ENVSYS_FVALID_MAX	0x00000002	/* max value is ok */
#define ENVSYS_FVALID_MIN	0x00000004	/* min value is ok */
#define ENVSYS_F_OBSOLETE	0x00000008
#define ENVSYS_FCHANGERFACT	0x00000010	/* sensor can change rfact */

/* monitoring flags */
#define ENVSYS_FMONCRITICAL	0x00000020	/* monitor a critical state */
#define ENVSYS_FMONLIMITS	0x00000040	/* monitor limits/thresholds */
#define ENVSYS_FMONSTCHANGED	0x00000400	/* monitor a battery/drive state */
#define ENVSYS_FMONANY	\
	(ENVSYS_FMONCRITICAL | ENVSYS_FMONLIMITS | ENVSYS_FMONSTCHANGED)
#define ENVSYS_FMONNOTSUPP	0x00000800	/* monitoring not supported */
#define ENVSYS_FNEED_REFRESH	0x00001000	/* sensor needs refreshing */
#define ENVSYS_FHAS_ENTROPY	0x00002000	/* sensor provides entropy
						   for rnd(4) */

/*
 * Properties that can be set in upropset (and in the event_limit's
 * flags field)
 */
#define	PROP_CRITMAX		0x0001
#define	PROP_CRITMIN		0x0002
#define	PROP_WARNMAX		0x0004
#define	PROP_WARNMIN		0x0008
#define	PROP_BATTCAP		0x0010
#define	PROP_BATTWARN		0x0020
#define	PROP_BATTHIGH		0x0040
#define	PROP_BATTMAX		0x0080
#define	PROP_DESC		0x0100
#define	PROP_RFACT		0x0200

#define	PROP_DRIVER_LIMITS	0x8000
#define	PROP_CAP_LIMITS		(PROP_BATTCAP  | PROP_BATTWARN | \
				 PROP_BATTHIGH | PROP_BATTMAX)
#define	PROP_VAL_LIMITS		(PROP_CRITMAX  | PROP_CRITMIN | \
				 PROP_WARNMAX  | PROP_WARNMIN)
#define	PROP_LIMITS		(PROP_CAP_LIMITS | PROP_VAL_LIMITS)
struct sme_event;

struct sysmon_envsys {
	const char *sme_name;		/* envsys device name */
	u_int sme_nsensors;		/* sensors count, from driver */
	u_int sme_fsensor;		/* sensor index base, from sysmon */
#define SME_SENSOR_IDX(sme, idx) 	((idx) - (sme)->sme_fsensor)
	int sme_class;			/* class of device */
#define SME_CLASS_BATTERY	1		/* device is a battery */
#define SME_CLASS_ACADAPTER	2		/* device is an AC adapter */
	int sme_flags;			/* additional flags */
#define SME_FLAG_BUSY 		0x00000001 	/* device busy */
#define SME_DISABLE_REFRESH	0x00000002	/* disable sme_refresh */
#define SME_CALLOUT_INITIALIZED	0x00000004	/* callout was initialized */
#define SME_INIT_REFRESH        0x00000008      /* call sme_refresh() after
						   interrupts are enabled in
						   the autoconf(9) process. */
#define SME_POLL_ONLY           0x00000010      /* only poll sme_refresh */

	void *sme_cookie;		/* for ENVSYS back-end */

	/* 
	 * Function callback to receive data from device.
	 */
	void (*sme_refresh)(struct sysmon_envsys *, envsys_data_t *);

	/*
	 * Function callbacks to exchange limit/threshold values
	 * with device
	 */
	void (*sme_set_limits)(struct sysmon_envsys *, envsys_data_t *,
			       sysmon_envsys_lim_t *, uint32_t *);
	void (*sme_get_limits)(struct sysmon_envsys *, envsys_data_t *,
			       sysmon_envsys_lim_t *, uint32_t *);

	struct workqueue *sme_wq;	/* the workqueue for the events */
	struct callout sme_callout;	/* for the events */
	uint64_t sme_events_timeout;	/* the timeout used in the callout */

	/* 
	 * linked list for the sysmon envsys devices.
	 */
	LIST_ENTRY(sysmon_envsys) sme_list;

	/* 
	 * linked list for the events that a device maintains.
	 */
	LIST_HEAD(, sme_event) sme_events_list;

	/*
	 * tailq for the sensors that a device maintains.
	 */
	TAILQ_HEAD(, envsys_data) sme_sensors_list;

	/*
	 * Locking/synchronization.
	 */
	int sme_busy;			/* number of items on workqueue,
					   sme_mtx or sme_work_mtx to read,
					   both to write */
	kmutex_t sme_mtx;
	kmutex_t sme_work_mtx;
	kcondvar_t sme_condvar;
};

int	sysmonopen_envsys(dev_t, int, int, struct lwp *);
int	sysmonclose_envsys(dev_t, int, int, struct lwp *);
int	sysmonioctl_envsys(dev_t, u_long, void *, int, struct lwp *);

struct sysmon_envsys 	*sysmon_envsys_create(void);
void 			sysmon_envsys_destroy(struct sysmon_envsys *);

int	sysmon_envsys_register(struct sysmon_envsys *);
void	sysmon_envsys_unregister(struct sysmon_envsys *);

int	sysmon_envsys_sensor_attach(struct sysmon_envsys *, envsys_data_t *);
int	sysmon_envsys_sensor_detach(struct sysmon_envsys *, envsys_data_t *);

uint32_t	sysmon_envsys_get_max_value(bool (*)(const envsys_data_t*), bool);

void	sysmon_envsys_sensor_event(struct sysmon_envsys *, envsys_data_t *,
				   int);

void	sysmon_envsys_refresh_sensor(struct sysmon_envsys *, envsys_data_t *);

typedef	bool (*sysmon_envsys_callback_t)(const struct sysmon_envsys *,
					 const envsys_data_t *, void*);

void	sysmon_envsys_foreach_sensor(sysmon_envsys_callback_t, void *, bool);

int	sysmon_envsys_update_limits(struct sysmon_envsys *, envsys_data_t *);

int	sysmon_envsys_init(void);
int	sysmon_envsys_fini(void);

/*****************************************************************************
 * Watchdog timer support
 *****************************************************************************/

struct sysmon_wdog {
	const char *smw_name;		/* watchdog device name */

	LIST_ENTRY(sysmon_wdog) smw_list;

	void *smw_cookie;		/* for watchdog back-end */
	int (*smw_setmode)(struct sysmon_wdog *);
	int (*smw_tickle)(struct sysmon_wdog *);
	u_int smw_period;		/* timer period (in seconds) */
	int smw_mode;			/* timer mode */
	u_int smw_refcnt;		/* references */
	pid_t smw_tickler;		/* last process to tickle */
};

int	sysmonopen_wdog(dev_t, int, int, struct lwp *);
int	sysmonclose_wdog(dev_t, int, int, struct lwp *);
int	sysmonioctl_wdog(dev_t, u_long, void *, int, struct lwp *);

int     sysmon_wdog_setmode(struct sysmon_wdog *, int, u_int);
int     sysmon_wdog_register(struct sysmon_wdog *);
int     sysmon_wdog_unregister(struct sysmon_wdog *);

int	sysmon_wdog_init(void);
int	sysmon_wdog_fini(void);

/*****************************************************************************
 * Power management support
 *****************************************************************************/

struct sysmon_pswitch {
	const char *smpsw_name;		/* power switch name */
	int smpsw_type;			/* power switch type */

	LIST_ENTRY(sysmon_pswitch) smpsw_list;
};

int	sysmonopen_power(dev_t, int, int, struct lwp *);
int	sysmonclose_power(dev_t, int, int, struct lwp *);
int	sysmonread_power(dev_t, struct uio *, int);
int	sysmonpoll_power(dev_t, int, struct lwp *);
int	sysmonkqfilter_power(dev_t, struct knote *);
int	sysmonioctl_power(dev_t, u_long, void *, int, struct lwp *);

void	sysmon_power_settype(const char *);

int	sysmon_pswitch_register(struct sysmon_pswitch *);
void	sysmon_pswitch_unregister(struct sysmon_pswitch *);

void	sysmon_pswitch_event(struct sysmon_pswitch *, int);
void	sysmon_penvsys_event(struct penvsys_state *, int);

int	sysmon_power_init(void);
int	sysmon_power_fini(void);

/*
 * Interface to sysmon common code used for autoloading
 */
struct sysmon_opvec {
	int (*so_open)(dev_t, int, int, struct lwp*);
	int (*so_close)(dev_t, int, int, struct lwp*);
	int (*so_ioctl)(dev_t, u_long, void *, int, struct lwp*);
	int (*so_read)(dev_t, struct uio*, int);
	int (*so_poll)(dev_t, int, struct lwp*);
	int (*so_filter)(dev_t, struct knote*);
};

int	sysmon_init(void);
int	sysmon_fini(void);
int	sysmon_attach_minor(int, struct sysmon_opvec*);

#endif /* _DEV_SYSMON_SYSMONVAR_H_ */
