#include "syslib.h"

PUBLIC int sys_physzero(phys_bytes base, phys_bytes bytes)
{
/* Zero a block of data.  */

  message mess;

  if (bytes == 0L) return(OK);

  mess.PZ_MEM_PTR = (char *) base;
  mess.PZ_COUNT   = bytes;

  return(_taskcall(SYSTASK, SYS_PHYSZERO, &mess));
}

