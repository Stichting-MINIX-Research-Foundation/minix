
#include <lib.h>
#include <minix/sysinfo.h>
#include <minix/com.h>

PUBLIC int getsysinfo(who, what, where)
endpoint_t who;			/* from whom to request info */
int what;			/* what information is requested */
void *where;			/* where to put it */
{
  message m;
  m.SI_WHAT = what;
  m.SI_WHERE = where;
  if (_syscall(who, COMMON_GETSYSINFO, &m) < 0) return(-1);
  return(0);
}
