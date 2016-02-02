/* readclock - manipulate the hardware real time clock */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <lib.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/callnr.h>
#include <minix/log.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/com.h>
#include <minix/type.h>
#include <minix/safecopies.h>

#include "readclock.h"

static struct rtc rtc;

static struct log log = {
	.name = "readclock",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* functions for transfering struct tm to/from this driver and calling proc. */
static int fetch_t(endpoint_t who_e, vir_bytes rtcdev_tm, struct tm *t);
static int store_t(endpoint_t who_e, vir_bytes rtcdev_tm, struct tm *t);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init(int type, sef_init_info_t * info);

int
main(int argc, char **argv)
{
	int r;
	endpoint_t caller;
	struct tm t;
	message m;
	int ipc_status, reply_status;

	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE) {

		/* Receive Message */
		r = sef_receive_status(ANY, &m, &ipc_status);
		if (r != OK) {
			log_warn(&log, "sef_receive_status() failed\n");
			continue;
		}

		if (is_ipc_notify(ipc_status)) {

			/* Do not reply to notifications. */
			continue;
		}

		caller = m.m_source;

		log_debug(&log, "Got message 0x%x from 0x%x\n", m.m_type,
		    caller);

		switch (m.m_type) {
		case RTCDEV_GET_TIME:
			/* Any user can read the time */
			reply_status = rtc.get_time(&t, m.m_lc_readclock_rtcdev.flags);
			if (reply_status != OK) {
				break;
			}

			/* write results back to calling process */
			reply_status =
			    store_t(caller, m.m_lc_readclock_rtcdev.tm, &t);
			break;

		case RTCDEV_SET_TIME:
			/* Only super user is allowed to set the time */
			if (getnuid(caller) == SUPER_USER) {
				/* read time from calling process */
				reply_status =
				    fetch_t(caller, m.m_lc_readclock_rtcdev.tm,
				    &t);
				if (reply_status != OK) {
					break;
				}

				reply_status =
				    rtc.set_time(&t, m.m_lc_readclock_rtcdev.flags);
			} else {
				reply_status = EPERM;
			}
			break;

		case RTCDEV_PWR_OFF:
			/* Only PM is allowed to set the power off time */
			if (caller == PM_PROC_NR) {
				reply_status = rtc.pwr_off();
			} else {
				reply_status = EPERM;
			}
			break;

		default:
			/* Unrecognized call */
			reply_status = EINVAL;
			break;
		}

		/* Send Reply */
		m.m_type = RTCDEV_REPLY;
		m.m_readclock_lc_rtcdev.status = reply_status;

		log_debug(&log, "Sending Reply");

		r = ipc_sendnb(caller, &m);
		if (r != OK) {
			log_warn(&log, "ipc_sendnb() failed\n");
			continue;
		}
	}

	rtc.exit();
	return 0;
}

static int
sef_cb_init(int type, sef_init_info_t * UNUSED(info))
{
	int r;

	r = arch_setup(&rtc);
	if (r != OK) {
		log_warn(&log, "Clock setup failed\n");
		return r;
	}

	r = rtc.init();
	if (r != OK) {
		log_warn(&log, "Clock initalization failed\n");
		return r;
	}

	return OK;
}

static void
sef_local_startup()
{
	/*
	 * Register init callbacks. Use the same function for all event types
	 */
	sef_setcb_init_fresh(sef_cb_init);
	sef_setcb_init_lu(sef_cb_init);
	sef_setcb_init_restart(sef_cb_init);

	/* Let SEF perform startup. */
	sef_startup();
}

int
bcd_to_dec(int n)
{
	return ((n >> 4) & 0x0F) * 10 + (n & 0x0F);
}

int
dec_to_bcd(int n)
{
	return ((n / 10) << 4) | (n % 10);
}

static int
fetch_t(endpoint_t who_e, vir_bytes rtcdev_tm, struct tm *t)
{
	return sys_datacopy(who_e, rtcdev_tm, SELF, (vir_bytes) t,
	    sizeof(struct tm));
}

static int
store_t(endpoint_t who_e, vir_bytes rtcdev_tm, struct tm *t)
{
	return sys_datacopy(SELF, (vir_bytes) t, who_e, rtcdev_tm,
	    sizeof(struct tm));
}
