#include <lib.h>
#define getnpid	_getnpid
#include <unistd.h>

PUBLIC pid_t getnpid(int proc_nr)
{
  message m;
  m.m1_i1 = proc_nr;		/* search pid for this process */
  if (_syscall(MM, GETPID, &m) < 0) return ( (pid_t) -1);
  return( (pid_t) m.m2_i2);	/* return search result */
}
