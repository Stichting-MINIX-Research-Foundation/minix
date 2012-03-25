
#include <lib.h>
#include <minix/sysinfo.h>
#include <minix/com.h>

int getsysinfo(
  endpoint_t who,		/* from whom to request info */
  int what,			/* what information is requested */
  void *where,			/* where to put it */
  size_t size 			/* how big it should be */
)
{
  message m;
  m.SI_WHAT = what;
  m.SI_WHERE = where;
  m.SI_SIZE = size;
  if (_syscall(who, COMMON_GETSYSINFO, &m) < 0) return(-1);
  return(0);
}
