#define _SYSTEM 1

#include <lib.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>

int _kernel_call(int syscallnr, message *msgptr)
{
  int t, r;
  t = 1;
  while(1) {
      msgptr->m_type = syscallnr;
      do_kernel_call(msgptr);
      r = msgptr->m_type;
      if(r != ENOTREADY) {
          break;
      }
      tickdelay(t++);
  }
  return r;
}

