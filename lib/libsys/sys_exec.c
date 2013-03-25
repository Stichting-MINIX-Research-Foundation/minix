#include "syslib.h"

int sys_exec(endpoint_t proc_ep, char *stack_ptr, char *progname,
	vir_bytes pc, vir_bytes ps_str)
{
/* A process has exec'd.  Tell the kernel. */

	message m;

	m.PR_ENDPT = proc_ep;
	m.PR_STACK_PTR = stack_ptr;
	m.PR_NAME_PTR = progname;
	m.PR_IP_PTR = (char *)pc;
	m.PR_PS_STR_PTR = (char *)ps_str; 

	return _kernel_call(SYS_EXEC, &m);
}
