#include <lib.h>
#define getprocnr	_getprocnr
#include <unistd.h>


PUBLIC int getprocnr(proc_nr)
int *proc_nr;			/* return process number here */
{
  message m;

  if (_syscall(MM, GETPROCNR, &m) < 0) return(-1);
  *proc_nr = m.m1_i1;
  return(0);
}

