#include <lib.h>
#define allocmem	_allocmem
#include <unistd.h>


PUBLIC int allocmem(size, base)
phys_bytes size;			/* size of mem chunk requested */
phys_bytes *base;			/* return base address */
{
  message m;
  m.m4_l1 = size;		
  if (_syscall(MM, ALLOCMEM, &m) < 0) return(-1);
  *base = m.m4_l2;
  return(0);
}

