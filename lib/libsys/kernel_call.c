#include <lib.h>
#include <minix/syslib.h>

PUBLIC int _kernel_call(int syscallnr, message *msgptr)
{
  msgptr->m_type = syscallnr;
  _do_kernel_call(msgptr);
  return(msgptr->m_type);
}
