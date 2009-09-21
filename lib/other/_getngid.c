#include <lib.h>
#define getngid	_getngid
#include <unistd.h>

PUBLIC gid_t getngid(int proc_nr)
{
  message m;
  m.m1_i1 = proc_nr;		/* search gid for this process */
  if (_syscall(MM, GETGID, &m) < 0) return ( (gid_t) -1);
  return( (gid_t) m.m2_i2);	/* return search result */
}
