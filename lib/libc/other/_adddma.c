/* adddma.c 
 */

#include <lib.h>
#define adddma	_adddma
#include <unistd.h>
#include <stdarg.h>

int adddma(proc_e, start, size)
endpoint_t proc_e;
phys_bytes start;
phys_bytes size;
{
  message m;

  m.m2_i1= proc_e;
  m.m2_l1= start;
  m.m2_l2= size;

  return _syscall(PM_PROC_NR, ADDDMA, &m);
}
