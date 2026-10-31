#include "syslib.h"

int sys_readbios(phys_bytes address, void *buf, size_t size)
{
/* Read data from BIOS locations */
  message m;

  m.m_lsys_krn_readbios.size = size;
  m.m_lsys_krn_readbios.addr = address;
  m.m_lsys_krn_readbios.buf = (vir_bytes)buf;
  return(_kernel_call(SYS_READBIOS, &m));
}
