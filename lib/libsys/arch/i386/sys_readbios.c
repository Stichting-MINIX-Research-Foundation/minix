#include "syslib.h"

int sys_readbios(address, buf, size)
phys_bytes address;		/* Absolute memory address */
void *buf;			/* Buffer to store the results */
size_t size;			/* Amount of data to read */
{
/* Read data from BIOS locations */
  message m;

  m.m_lsys_krn_readbios.size = size;
  m.m_lsys_krn_readbios.addr = address;
  m.m_lsys_krn_readbios.buf = buf;
  return(_kernel_call(SYS_READBIOS, &m));
}
