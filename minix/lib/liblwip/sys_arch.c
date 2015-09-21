#include <sys/types.h>
#include <minix/sysutil.h>
#include <errno.h>

#include "lwip/sys.h"

u32_t sys_jiffies(void)
{
	return getticks();
}

u32_t sys_now(void)
{
	static u32_t hz;
	u32_t jiffs;

	if (!hz)
		hz = sys_hz();

	/* use ticks not realtime as sys_now() is used to calculate timers */
	jiffs = sys_jiffies();

	return jiffs * (1000 / hz);
}

