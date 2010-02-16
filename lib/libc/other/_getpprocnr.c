#include <lib.h>
#define getpprocnr	_getpprocnr
#include <unistd.h>


PUBLIC int getpprocnr()
{
  message m;
  m.m1_i1 = -1;			/* don't pass pid to search for */
  m.m1_i2 = 0;			/* don't pass name to search for */
  if (_syscall(PM_PROC_NR, GETPROCNR, &m) < 0) return(-1);
  return(m.m1_i2);		/* return parent process number */
}

