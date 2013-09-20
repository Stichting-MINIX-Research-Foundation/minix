
#include "syslib.h"

int sys_diagctl(int code, char *arg1, int arg2)
{
  message m;

  m.DIAGCTL_CODE = code;
  m.DIAGCTL_ARG1 = arg1;
  m.DIAGCTL_ARG2 = arg2;

  return(_kernel_call(SYS_DIAGCTL, &m));
}

int sys_diagctl_stacktrace(endpoint_t ep)
{
  return sys_diagctl(DIAGCTL_CODE_STACKTRACE, NULL, ep);
}

