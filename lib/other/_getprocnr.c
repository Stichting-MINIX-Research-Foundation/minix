#include <lib.h>
#define getprocnr	_getprocnr
#include <unistd.h>


PUBLIC int getprocnr()
{
  message m;
  m.m1_i1 = -1;			/* get own process number */
  m.m1_i2 = 0;			/* get own process number */
  if (_syscall(MM, GETPROCNR, &m) < 0) return(-1);
  return(m.m1_i1);
}

