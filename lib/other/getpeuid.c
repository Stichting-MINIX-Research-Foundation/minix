#include <lib.h>
#include <unistd.h>

PUBLIC uid_t getpeuid(ep)
endpoint_t ep;
{
  message m;

  m.m1_i1= ep;
  if (_syscall(MM, GETPUID, &m) < 0) return ( (uid_t) -1);
  return( (uid_t) m.m2_i1);
}
