#include "syslib.h"

int sys_memset(endpoint_t who, unsigned long pattern,
	phys_bytes base, phys_bytes bytes)
{
/* Zero a block of data.  */
  message mess;

  if (bytes == 0L) return(OK);

  mess.MEM_PTR = (char *) base;
  mess.MEM_COUNT   = bytes;
  mess.MEM_PATTERN = pattern;
  mess.MEM_PROCESS = who;

  return(_kernel_call(SYS_MEMSET, &mess));
}

