#include <sys/cdefs.h>
#include "namespace.h"

#include <lib.h>
#include <sys/times.h>

#ifdef __weak_alias
__weak_alias(times, _times)
#endif

clock_t times(struct tms *buf)
{
  message m;

  m.m4_l5 = 0;			/* return this if system is pre-1.6 */
  if (_syscall(PM_PROC_NR, TIMES, &m) < 0) return( (clock_t) -1);
  buf->tms_utime = m.m4_l1;
  buf->tms_stime = m.m4_l2;
  buf->tms_cutime = m.m4_l3;
  buf->tms_cstime = m.m4_l4;
  return(m.m4_l5);
}
