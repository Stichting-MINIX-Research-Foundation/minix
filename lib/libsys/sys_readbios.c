#include "syslib.h"

int sys_readbios(address, buf, size)
phys_bytes address;		/* Absolute memory address */
void *buf;			/* Buffer to store the results */
size_t size;			/* Amount of data to read */
{
/* Read data from BIOS locations */
  message m;

  m.RDB_SIZE = size;
  m.RDB_ADDR = address;
  m.RDB_BUF = buf;
  return(_kernel_call(SYS_READBIOS, &m));
}
