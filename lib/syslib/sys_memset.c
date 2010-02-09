#include "syslib.h"

PUBLIC int sys_memset(unsigned long pattern, phys_bytes base, phys_bytes bytes)
{
/* Zero a block of data.  */
  message mess;

  if (bytes == 0L) return(OK);

  mess.MEM_PTR = (char *) base;
  mess.MEM_COUNT   = bytes;
  mess.MEM_PATTERN = pattern;

  return(_kernel_call(SYS_MEMSET, &mess));
}

