#include <lib.h>
#define findproc	_findproc
#include <unistd.h>
#include <string.h>


PUBLIC int findproc(proc_name, proc_nr)
char *proc_name;		/* name of process to search for */
int *proc_nr;			/* return process number here */
{
  message m;

  m.m1_p1 = proc_name;
  m.m1_i1 = -1;			/* search by name */
  m.m1_i2 = strlen(proc_name) + 1;
  if (_syscall(MM, GETPROCNR, &m) < 0) return(-1);
  *proc_nr = m.m1_i1;
  return(0);
}

