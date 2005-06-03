#include <lib.h>
#define freemem	_freemem
#include <unistd.h>


PUBLIC int freemem(size, base)
phys_bytes size;			/* size of mem chunk requested */
phys_bytes base;			/* base address of mem chunk */
{
  message m;
  m.m4_l1 = size;		
  m.m4_l2 = base;		
  if (_syscall(MM, FREEMEM, &m) < 0) return(-1);
  return(0);
}

