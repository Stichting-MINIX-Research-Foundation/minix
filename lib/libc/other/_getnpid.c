#include <lib.h>
#define getnpid	_getnpid
#include <unistd.h>

PUBLIC pid_t getnpid(endpoint_t proc_ep)
{
  message m;
  m.m1_i1 = proc_ep;		/* search pid for this process */
  return _syscall(MM, GETEPINFO, &m);
}
