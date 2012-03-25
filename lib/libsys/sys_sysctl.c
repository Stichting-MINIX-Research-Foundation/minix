
#include "syslib.h"

int sys_sysctl(int code, char *arg1, int arg2)
{
  message m;

  m.SYSCTL_CODE = code;
  m.SYSCTL_ARG1 = arg1;
  m.SYSCTL_ARG2 = arg2;

  return(_kernel_call(SYS_SYSCTL, &m));

}

int sys_sysctl_stacktrace(endpoint_t ep)
{
  return sys_sysctl(SYSCTL_CODE_STACKTRACE, NULL, ep);
}

