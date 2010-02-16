/*	sleep() - Sleep for a number of seconds.	Author: Erik van der Kouwe
 *								25 July 2009
 * (Avoids interfering with alarm/setitimer by using select, like usleep)
 */

#include <lib.h>
#define sleep _sleep
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

unsigned sleep(unsigned sleep_seconds)
{
	struct timespec rqtp, rmtp;

	/* nanosleep implements this call; ignore failure, it cannot be reported */
	rqtp.tv_sec = sleep_seconds;
	rqtp.tv_nsec = 0;
	nanosleep(&rqtp, &rmtp);

	/* round remainder up to seconds */
	return rmtp.tv_sec + ((rmtp.tv_nsec > 0) ? 1 : 0);
}
