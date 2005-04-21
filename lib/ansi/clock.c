/*
 * clock - determine the processor time used
 */

#define times _times
#include <time.h>
#include <sys/times.h>

clock_t clock(void)
{
	struct tms tms;

	times(&tms);
	return tms.tms_utime;
}
