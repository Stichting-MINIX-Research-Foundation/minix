#include "syslib.h"

int sys_getmcontext(proc, mcp)
endpoint_t proc;		/* process retrieving context */
vir_bytes mcp;			/* where to store context */
{
/* A process wants to store its context in mcp. */

  message m;
  int r;

  m.m_lsys_krn_sys_getmcontext.endpt = proc;
  m.m_lsys_krn_sys_getmcontext.ctx_ptr = mcp;
  r = _kernel_call(SYS_GETMCONTEXT, &m);
  return r;
}

int sys_setmcontext(proc, mcp)
endpoint_t proc;		/* process setting context */
vir_bytes mcp;			/* where to get context from */
{
/* A process wants to restore context stored in ucp. */

  message m;
  int r;

  m.m_lsys_krn_sys_setmcontext.endpt = proc;
  m.m_lsys_krn_sys_setmcontext.ctx_ptr = mcp;
  r = _kernel_call(SYS_SETMCONTEXT, &m);
  return r;
}

