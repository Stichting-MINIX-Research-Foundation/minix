/*
 * Some real time clocks are embedded within other multi-function chips.
 * Drivers for such chips will implement the RTCDEV protocol and the
 * readclock driver will simply forward on the message to the driver.
 * This keeps things simple for any other services that need to access
 * the RTC as they only have to know / care about the readclock driver.
 */

#include <minix/syslib.h>
#include <minix/drvlib.h>
#include <minix/sysutil.h>
#include <minix/log.h>
#include <minix/rs.h>
#include <minix/ds.h>

#include <sys/mman.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <lib.h>

#include "forward.h"
#include "readclock.h"

static int fwd_msg(int type, struct tm *t, int flags);

static struct log log = {
	.name = "readclock.fwd",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/*
 * Label of the driver that messages should be forwarded to.
 */
static char *target_label;

int
fwd_set_label(char *label)
{
	target_label = label;
	return OK;
}

int
fwd_init(void)
{
	if (target_label == NULL) {
		return EINVAL;
	}
	return OK;
}

static int
fwd_msg(int type, struct tm *t, int flags)
{
	int r;
	message m;
	endpoint_t ep;

	r = ds_retrieve_label_endpt(target_label, &ep);
	if (r != 0) {
		return -1;
	}

	m.RTCDEV_TM = t;
	m.RTCDEV_FLAGS = flags;

	r = _syscall(ep, type, &m);
	if (r != RTCDEV_REPLY || m.RTCDEV_STATUS != 0) {
		log_warn(&log, "Call to '%s' failed.\n", target_label);
		return -1;
	}

	return OK;
}

int
fwd_get_time(struct tm *t, int flags)
{
	return fwd_msg(RTCDEV_GET_TIME, t, flags);
}

int
fwd_set_time(struct tm *t, int flags)
{
	return fwd_msg(RTCDEV_SET_TIME, t, flags);
}

int
fwd_pwr_off(void)
{
	return fwd_msg(RTCDEV_PWR_OFF, NULL, RTCDEV_NOFLAGS);
}

void
fwd_exit(void)
{
	target_label = NULL;
}
