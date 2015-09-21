#include "sysutil.h"

/*
 * Retrieve the system's uptime (number of clock ticks since system boot),
 * real time (corrected number of clock ticks since system boot), and
 * boot time (in number of seconds since the UNIX epoch).
 */
int
getuptime(clock_t * uptime, clock_t * realtime, time_t * boottime)
{
	struct minix_kerninfo *minix_kerninfo;

	minix_kerninfo = get_minix_kerninfo();

	/* We assume atomic 32-bit field retrieval.  TODO: 64-bit support. */
	*uptime = minix_kerninfo->kclockinfo->uptime;
	*realtime = minix_kerninfo->kclockinfo->realtime;
	*boottime = minix_kerninfo->kclockinfo->boottime;

	return OK;
}
