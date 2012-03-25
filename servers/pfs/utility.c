#include "fs.h"


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
int no_sys(message *pfs_m_in, message *pfs_m_out)
{
/* Somebody has used an illegal system call number */
  printf("no_sys: invalid call 0x%x to pfs\n", req_nr);
  return(EINVAL);
}


/*===========================================================================*
 *				clock_time				     *
 *===========================================================================*/
time_t clock_time()
{
/* This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.
 */

  int r;
  clock_t uptime;	/* Uptime in ticks */
  time_t boottime;

  if ((r = getuptime2(&uptime, &boottime)) != OK)
		panic("clock_time: getuptme2 failed: %d", r);

  return( (time_t) (boottime + (uptime/sys_hz())));
}
