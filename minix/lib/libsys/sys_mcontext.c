#include "syslib.h"

int sys_getmcontext(endpoint_t proc, vir_bytes mcp)
{
/* A process wants to store its context in mcp. */

  message m;
  int r;

  m.m_lsys_krn_sys_getmcontext.endpt = proc;
  m.m_lsys_krn_sys_getmcontext.ctx_ptr = mcp;
  r = _kernel_call(SYS_GETMCONTEXT, &m);
  return r;
}

int sys_setmcontext(endpoint_t proc, vir_bytes mcp)
{
/* A process wants to restore context stored in ucp. */

  message m;
  int r;

  m.m_lsys_krn_sys_setmcontext.endpt = proc;
  m.m_lsys_krn_sys_setmcontext.ctx_ptr = mcp;
  r = _kernel_call(SYS_SETMCONTEXT, &m);
  return r;
}

