#include <lib.h>
#define getsysinfo	_getsysinfo
#include <unistd.h>


PUBLIC int getsysinfo(who, what, where)
int who;			/* from whom to request info */
int what;			/* what information is requested */
void *where;			/* where to put it */
{
  message m;
  m.m1_i1 = what;
  m.m1_p1 = where;
  if (_syscall(who, GETSYSINFO, &m) < 0) return(-1);
  return(0);
}

