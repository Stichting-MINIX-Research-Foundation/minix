
#include "syslib.h"

PUBLIC int sys_sysctl(int code, char *arg1, int arg2)
{
  message m;

  m.SYSCTL_CODE = code;
  m.SYSCTL_ARG1 = arg1;
  m.SYSCTL_ARG2 = arg2;

  return(_taskcall(SYSTASK, SYS_SYSCTL, &m));

}
