#include "fs.h"


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* Somebody has used an illegal system call number */
  printf("no_sys: invalid call %d\n", req_nr);
  return(EINVAL);
}


/*===========================================================================*
 *				clock_time				     *
 *===========================================================================*/
PUBLIC time_t clock_time()
{
/* This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.
 */

  int r;
  clock_t uptime, boottime;

  if ((r = getuptime2(&uptime,&boottime)) != OK)
		panic(__FILE__,"clock_time: getuptme2 failed", r);
  
  return( (time_t) (boottime + (uptime/sys_hz())));
}

