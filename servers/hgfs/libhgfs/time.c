/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

PRIVATE u64_t time_offset;

/*===========================================================================*
 *				time_init				     *
 *===========================================================================*/
PUBLIC void time_init()
{
/* Initialize the time conversion module.
 */

  /* Generate a 64-bit value for the offset to use in time conversion. The
   * HGFS time format uses Windows' FILETIME standard, expressing time in
   * 100ns-units since Jan 1, 1601 UTC. The value that is generated is
   * 116444736000000000.
   */
  /* FIXME: we currently do not take into account timezones. */
  time_offset = make64(3577643008UL, 27111902UL);
}

/*===========================================================================*
 *				time_put				     *
 *===========================================================================*/
PUBLIC void time_put(timep)
time_t *timep;
{
/* Store a UNIX timestamp pointed to by the given pointer onto the RPC buffer,
 * in HGFS timestamp format. If a NULL pointer is given, store a timestamp of
 * zero instead.
 */
  u64_t hgfstime;

  if (timep != NULL) {
	hgfstime = add64(mul64u(*timep, 10000000), time_offset);

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
PUBLIC void time_get(timep)
time_t *timep;
{
/* Get a HGFS timestamp from the RPC buffer, convert it into a UNIX timestamp,
 * and store the result in the given time pointer. If the given pointer is
 * NULL, however, simply skip over the timestamp in the RPC buffer.
 */
  u64_t hgfstime;
  u32_t time_lo, time_hi;

  if (timep != NULL) {
  	time_lo = RPC_NEXT32;
  	time_hi = RPC_NEXT32;

  	hgfstime = make64(time_lo, time_hi);

	*timep = div64u(sub64(hgfstime, time_offset), 10000000);
  }
  else RPC_ADVANCE(sizeof(u32_t) * 2);
}
