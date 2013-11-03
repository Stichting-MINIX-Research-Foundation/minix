
#include "syslib.h"
#include <string.h>
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
  memset(&m, 0, sizeof(m));
  m.SI_WHAT = what;
  m.SI_WHERE = where;
  m.SI_SIZE = size;
  return _taskcall(who, COMMON_GETSYSINFO, &m);
}
