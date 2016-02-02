#include "sysutil.h"

/*
 * Return the number of clock ticks since system boot.  Note that the value may
 * wrap on overflow.
 */
clock_t
getticks(void)
{

	/* We assume atomic 32-bit field retrieval.  TODO: 64-bit support. */
	return get_minix_kerninfo()->kclockinfo->uptime;
}
