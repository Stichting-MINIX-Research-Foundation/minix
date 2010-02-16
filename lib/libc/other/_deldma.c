/* deldma.c 
 */

#include <lib.h>
#define deldma	_deldma
#include <unistd.h>
#include <stdarg.h>

int deldma(proc_e, start, size)
endpoint_t proc_e;
phys_bytes start;
phys_bytes size;
{
  message m;

  m.m2_i1= proc_e;
  m.m2_l1= start;
  m.m2_l2= size;

  return _syscall(MM, DELDMA, &m);
}
