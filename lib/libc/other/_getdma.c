/* getdma.c 
 */

#include <lib.h>
#define getdma	_getdma
#include <unistd.h>
#include <stdarg.h>

int getdma(procp, basep, sizep)
endpoint_t *procp;
phys_bytes *basep;
phys_bytes *sizep;
{
  int r;
  message m;

  r= _syscall(PM_PROC_NR, GETDMA, &m);
  if (r == 0)
  {
	*procp= m.m2_i1;
	*basep= m.m2_l1;
	*sizep= m.m2_l2;
  }
  return r;
}
