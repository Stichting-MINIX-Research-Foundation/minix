#include "syslib.h"

int sys_getmcontext(proc, mcp)
endpoint_t proc;		/* process retrieving context */
mcontext_t *mcp;		/* where to store context */
{
/* A process wants to store its context in mcp. */

  message m;
  int r;

  m.PR_ENDPT = proc;
  m.PR_CTX_PTR = (char *) mcp;
  r = _kernel_call(SYS_GETMCONTEXT, &m);
  return r;
}

int sys_setmcontext(proc, mcp)
endpoint_t proc;		/* process setting context */
mcontext_t *mcp;		/* where to get context from */
{
/* A process wants to restore context stored in ucp. */

  message m;
  int r;

  m.PR_ENDPT = proc;
  m.PR_CTX_PTR = (char *) mcp;
  r = _kernel_call(SYS_SETMCONTEXT, &m);
  return r;
}

