#include "syslib.h"

PUBLIC int sys_exec(proc, ptr, traced, prog_name, initpc)
int proc;			/* process that did exec */
char *ptr;			/* new stack pointer */
int traced;			/* is tracing enabled? */
char *prog_name;		/* name of the new program */
vir_bytes initpc;
{
/* A process has exec'd.  Tell the kernel. */

  message m;

  m.PR_PROC_NR = proc;
  m.PR_TRACING = traced;
  m.PR_STACK_PTR = ptr;
  m.PR_NAME_PTR = prog_name;
  m.PR_IP_PTR = (char *)initpc;
  return(_taskcall(SYSTASK, SYS_EXEC, &m));
}
