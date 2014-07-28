
#include "syslib.h"
#include "sysutil.h"

int sys_diagctl(int code, char *arg1, int arg2)
{
  message m;

  m.m_lsys_krn_sys_diagctl.code = code;

  switch(code) {
  case DIAGCTL_CODE_DIAG:
	m.m_lsys_krn_sys_diagctl.buf = (vir_bytes)arg1;
	m.m_lsys_krn_sys_diagctl.len = arg2;
	break;
  case DIAGCTL_CODE_STACKTRACE:
	m.m_lsys_krn_sys_diagctl.endpt = (endpoint_t)arg2;
	break;
  case DIAGCTL_CODE_REGISTER:
	break;
  case DIAGCTL_CODE_UNREGISTER:
	break;
  default:
	panic("Unknown SYS_DIAGCTL request %d\n", code);
  }

  return(_kernel_call(SYS_DIAGCTL, &m));
}
