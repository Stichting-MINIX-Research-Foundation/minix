#include <lib.h>
#define times	_times
#include <sys/times.h>
#include <time.h>

PUBLIC clock_t times(buf)
struct tms *buf;
{
  message m;

  m.m4_l5 = 0;			/* return this if system is pre-1.6 */
  if (_syscall(MM, TIMES, &m) < 0) return( (clock_t) -1);
  buf->tms_utime = m.m4_l1;
  buf->tms_stime = m.m4_l2;
  buf->tms_cutime = m.m4_l3;
  buf->tms_cstime = m.m4_l4;
  return(m.m4_l5);
}
