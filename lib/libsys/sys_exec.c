#include "syslib.h"

int sys_exec(proc_ep, ptr, prog_name, initpc)
endpoint_t proc_ep;		/* process that did exec */
char *ptr;			/* new stack pointer */
char *prog_name;		/* name of the new program */
vir_bytes initpc;
{
/* A process has exec'd.  Tell the kernel. */

  message m;

  m.PR_ENDPT = proc_ep;
  m.PR_STACK_PTR = ptr;
  m.PR_NAME_PTR = prog_name;
  m.PR_IP_PTR = (char *)initpc;
  return(_kernel_call(SYS_EXEC, &m));
}
