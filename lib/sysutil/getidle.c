/* getidle.c - by David van Moolenbroek <dcvmoole@cs.vu.nl> */

/* Usage:
 *
 *   double idleperc;
 *   getidle();
 *   ...
 *   idleperc = getidle();
 *   printf("CPU usage: %lg%%\n", 100.0 - idleperc);
 *
 * This routine goes through PM to get the idle time, rather than making the
 * sys_getinfo() call to the kernel directly. This means that it can be used
 * by non-system processes as well, but it will incur some extra overhead in
 * the system case. The overhead does not end up being measured, because the
 * system is clearly not idle while the system calls are being made. In any
 * case, for this reason, only one getidle() run is allowed at a time.
 *
 * Note that the kernel has to be compiled with CONFIG_IDLE_TSC support.
 */

#define _MINIX 1
#define _SYSTEM 1
#include <minix/sysinfo.h>
#include <minix/u64.h>
#include <minix/sysutil.h>

static u64_t start, idle;
static int running = 0;

static double make_double(u64_t d)
{
/* Convert a 64-bit fixed point value into a double.
 * This whole thing should be replaced by something better eventually.
 */
  double value;
  int i;

  value = (double) ex64hi(d);
  for (i = 0; i < sizeof(unsigned long); i += 2)
	value *= 65536.0;

  value += (double) ex64lo(d);

  return value;
}

double getidle(void)
{
  u64_t stop, idle2;
  u64_t idelta, tdelta;
  double ifp, tfp, rfp;
  int r;

  if (!running) {
	r = getsysinfo_up(PM_PROC_NR, SIU_IDLETSC, sizeof(idle), &idle);
	if (r != sizeof(idle))
		return -1.0;

	running = 1;

	read_tsc_64(&start);

	return 0.0;
  }
  else {
	read_tsc_64(&stop);

	running = 0;

	r = getsysinfo_up(PM_PROC_NR, SIU_IDLETSC, sizeof(idle2), &idle2);
	if (r != sizeof(idle2))
		return -1.0;

	idelta = sub64(idle2, idle);
	tdelta = sub64(stop, start);

	if (cmp64(idelta, tdelta) >= 0)
		return 100.0;

	ifp = make_double(idelta);
	tfp = make_double(tdelta);

	rfp = ifp / tfp * 100.0;

	if (rfp < 0.0) rfp = 0.0;
	else if (rfp > 100.0) rfp = 100.0;

	return rfp;
  }

  running = !running;
}
