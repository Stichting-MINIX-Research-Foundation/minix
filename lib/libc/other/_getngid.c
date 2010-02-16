#include <lib.h>
#define getngid	_getngid
#include <unistd.h>

PUBLIC gid_t getngid(endpoint_t proc_ep)
{
  message m;
  m.m1_i1 = proc_ep;		/* search gid for this process */
  if (_syscall(MM, GETEPINFO, &m) < 0) return ( (gid_t) -1);
  return( (gid_t) m.m2_i2);	/* return search result */
}
