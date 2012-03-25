#include <lib.h>
#include <unistd.h>

pid_t getnpid(endpoint_t proc_ep)
{
  message m;
  m.m1_i1 = proc_ep;		/* search pid for this process */
  return _syscall(PM_PROC_NR, GETEPINFO, &m);
}
