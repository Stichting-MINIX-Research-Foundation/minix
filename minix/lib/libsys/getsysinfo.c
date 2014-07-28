
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
  int call_nr;

  switch (who) {
  case PM_PROC_NR: call_nr = PM_GETSYSINFO; break;
  case VFS_PROC_NR: call_nr = VFS_GETSYSINFO; break;
  case RS_PROC_NR: call_nr = RS_GETSYSINFO; break;
  case DS_PROC_NR: call_nr = DS_GETSYSINFO; break;
  default:
	return ENOSYS;
  }

  memset(&m, 0, sizeof(m));
  m.m_lsys_getsysinfo.what = what;
  m.m_lsys_getsysinfo.where = where;
  m.m_lsys_getsysinfo.size = size;
  return _taskcall(who, call_nr, &m);
}
