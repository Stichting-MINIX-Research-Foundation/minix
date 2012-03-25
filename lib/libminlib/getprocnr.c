#include <lib.h>
#include <unistd.h>


int getprocnr()
{
  message m;
  m.m1_i1 = -1;			/* don't pass pid to search for */
  m.m1_i2 = 0;			/* don't pass name to search for */
  if (_syscall(PM_PROC_NR, GETPROCNR, &m) < 0) return(-1);
  return(m.m1_i1);		/* return own process number */
}

