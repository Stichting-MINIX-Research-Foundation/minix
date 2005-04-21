#include "syslib.h"

PUBLIC int sys_trace(req, proc_nr, addr, data_p)
int req, proc_nr;
long addr, *data_p;
{
  message m;
  int r;

  m.CTL_PROC_NR = proc_nr;
  m.CTL_REQUEST = req;
  m.CTL_ADDRESS = addr;
  if (data_p) m.CTL_DATA = *data_p;
  r = _taskcall(SYSTASK, SYS_TRACE, &m);
  if (data_p) *data_p = m.CTL_DATA;
  return(r);
}
