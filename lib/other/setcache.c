
#include <minix/config.h>

#include <lib.h>
#include <unistd.h>
#include <minix/type.h>
#include <minix/com.h>

int
setcache(int kb)
{
  message m;
  int r;

  m.m1_i1 = kb;
  m.m_type = SETCACHE;
  if ((r=_syscall(FS, SETCACHE, &m)) < 0)
  	return(-1);

  return(m.m_type);
}
