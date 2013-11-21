/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

static u64_t time_offset;

/*===========================================================================*
 *				time_init				     *
 *===========================================================================*/
void time_init(void)
{
/* Initialize the time conversion module.
 */

  /* Generate a 64-bit value for the offset to use in time conversion. The
   * HGFS time format uses Windows' FILETIME standard, expressing time in
   * 100ns-units since Jan 1, 1601 UTC. The value that is generated is
   * the difference between that time and the UNIX epoch, in 100ns units.
   */
  /* FIXME: we currently do not take into account timezones. */
  time_offset = (u64_t)116444736 * 1000000000;
}

/*===========================================================================*
 *				time_put				     *
 *===========================================================================*/
void time_put(struct timespec *tsp)
{
/* Store a POSIX timestamp pointed to by the given pointer onto the RPC buffer,
 * in HGFS timestamp format. If a NULL pointer is given, store a timestamp of
 * zero instead.
 */
  u64_t hgfstime;

  if (tsp != NULL) {
	hgfstime = ((u64_t)tsp->tv_sec * 10000000) + (tsp->tv_nsec / 100);
	hgfstime += time_offset;

	RPC_NEXT32 = ex64lo(hgfstime);
	RPC_NEXT32 = ex64hi(hgfstime);
  } else {
	RPC_NEXT32 = 0;
	RPC_NEXT32 = 0;
  }
}

/*===========================================================================*
 *				time_get				     *
 *===========================================================================*/
void time_get(struct timespec *tsp)
{
/* Get a HGFS timestamp from the RPC buffer, convert it into a POSIX timestamp,
 * and store the result in the given time pointer. If the given pointer is
 * NULL, however, simply skip over the timestamp in the RPC buffer.
 */
  u64_t hgfstime;
  u32_t time_lo, time_hi;

  if (tsp != NULL) {
	time_lo = RPC_NEXT32;
	time_hi = RPC_NEXT32;

	hgfstime = make64(time_lo, time_hi) - time_offset;

	tsp->tv_sec  = (unsigned long)(hgfstime / 10000000);
	tsp->tv_nsec = (unsigned)(hgfstime % 10000000) * 100;
  }
  else RPC_ADVANCE(sizeof(u32_t) * 2);
}
