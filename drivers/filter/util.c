/* Filter driver - utility functions */

#include "inc.h"
#include <sys/mman.h>
#include <signal.h>

static clock_t next_alarm;

/*===========================================================================*
 *				flt_malloc				     *
 *===========================================================================*/
char *flt_malloc(size_t size, char *sbuf, size_t ssize)
{
	/* Allocate a buffer for 'size' bytes. If 'size' is equal to or less
	 * than 'ssize', return the static buffer 'sbuf', otherwise, use
	 * malloc() to allocate memory dynamically.
	 */
	char *p;

	if (size <= ssize)
		return sbuf;

	p = mmap(NULL, size, PROT_READ | PROT_WRITE,
		MAP_PREALLOC | MAP_CONTIG | MAP_ANON, -1, 0);
	if (p == MAP_FAILED)
		panic(__FILE__, "out of memory", size);

	return p;
}

/*===========================================================================*
 *				flt_free				     *
 *===========================================================================*/
void flt_free(char *buf, size_t size, char *sbuf)
{	
	/* Free a buffer previously allocated with flt_malloc().
	 */

	if(buf != sbuf)
		munmap(buf, size);
}

/*===========================================================================*
 *				print64					     *
 *===========================================================================*/
char *print64(u64_t p)
{
#define NB 10
	static int n = 0;
	static char buf[NB][100];
	u32_t lo = ex64lo(p), hi = ex64hi(p);
	n = (n+1) % NB;
	if(!hi) sprintf(buf[n], "%lx", lo);
	else sprintf(buf[n], "%lx%08lx", hi, lo);
	return buf[n];
}

/*===========================================================================*
 *				flt_alarm				     *
 *===========================================================================*/
clock_t flt_alarm(clock_t dt)
{
	int r;

	if(dt < 0)
		return next_alarm;

	r = sys_setalarm(dt, 0);

	if(r != OK)
		panic(__FILE__, "sys_setalarm failed", r);

	if(dt == 0) {
		if(!next_alarm)
			panic(__FILE__, "clearing unset alarm", r);
		next_alarm = 0;
	} else {
		if(next_alarm)
			panic(__FILE__, "overwriting alarm", r);
		if ((r = getuptime(&next_alarm)) != OK)
			panic(__FILE__, "getuptime failed", r);
		next_alarm += dt;
	}

	return next_alarm;
}

/*===========================================================================*
 *				got_alarm				     *
 *===========================================================================*/
static void got_alarm(int sig)
{
	/* Do nothing. */
}

/*===========================================================================*
 *				flt_sleep				     *
 *===========================================================================*/
void flt_sleep(int secs)
{
	/* Sleep for the given number of seconds. Don't use sleep(), as that
	 * will end up calling select() to VFS. This implementation could be
	 * improved.
	 */

	signal(SIGALRM, got_alarm);
	alarm(secs);

	pause();
}
