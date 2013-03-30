#include "syslib.h"
#include <time.h>

int sys_settime(now, clk_id, sec, nsec)
int now;
clockid_t clk_id;
time_t sec;
long nsec;
{
  message m;
  int r;

  m.T_SETTIME_NOW = now;
  m.T_CLOCK_ID = clk_id;
  m.T_TIME_SEC = sec;
  m.T_TIME_NSEC = nsec;

  r = _kernel_call(SYS_SETTIME, &m);
  return(r);
}
