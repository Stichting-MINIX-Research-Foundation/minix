#include <lib.h>
#include <unistd.h>


int getnprocnr(pid_t pid)
{
  message m;
  int t = GETPROCNR;
  m.m1_i1 = pid;		/* pass pid >=0 to search for */
  m.m1_i2 = 0;			/* don't pass name to search for */
  if (_syscall(PM_PROC_NR, t, &m) < 0) return(-1);
  return(m.m1_i1);		/* return search result */
}

