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
#include <minix/safecopies.h>

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

static int fwd_msg(int type, struct tm *t, int t_access, int flags);

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
fwd_msg(int type, struct tm *t, int t_access, int flags)
{
	int r;
	message m;
	endpoint_t ep;
	cp_grant_id_t gid;

	r = ds_retrieve_label_endpt(target_label, &ep);
	if (r != 0) {
		return -1;
	}

	if (type == RTCDEV_PWR_OFF) {
		/* RTCDEV_PWR_OFF messages don't contain any data/flags. */
		return _syscall(ep, RTCDEV_PWR_OFF, &m);
	}

	gid = cpf_grant_direct(ep, (vir_bytes) t, sizeof(struct tm), t_access);
	if (!GRANT_VALID(gid)) {
		log_warn(&log, "Could not create grant.\n");
		return -1;
	}

	m.m_lc_readclock_rtcdev.grant = gid;
	m.m_lc_readclock_rtcdev.flags = flags;

	r = _syscall(ep, type, &m);
	cpf_revoke(gid);
	if (r != RTCDEV_REPLY || m.m_readclock_lc_rtcdev.status != 0) {
		log_warn(&log, "Call to '%s' failed.\n", target_label);
		return -1;
	}

	return OK;
}

int
fwd_get_time(struct tm *t, int flags)
{
	return fwd_msg(RTCDEV_GET_TIME_G, t, CPF_WRITE, flags);
}

int
fwd_set_time(struct tm *t, int flags)
{
	return fwd_msg(RTCDEV_SET_TIME_G, t, CPF_READ, flags);
}

int
fwd_pwr_off(void)
{
	return fwd_msg(RTCDEV_PWR_OFF, NULL, 0, RTCDEV_NOFLAGS);
}

void
fwd_exit(void)
{
	target_label = NULL;
}
