/*
 * clock - determine the processor time used
 */
#include <sys/cdefs.h>
#include "namespace.h"

#include <time.h>
#include <sys/times.h>

clock_t clock(void)
{
	struct tms tms;

	times(&tms);
	return tms.tms_utime;
}
