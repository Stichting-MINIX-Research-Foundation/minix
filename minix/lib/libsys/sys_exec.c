#include "syslib.h"

int sys_exec(endpoint_t proc_ep, vir_bytes stack_ptr, vir_bytes progname,
	vir_bytes pc, vir_bytes ps_str)
{
/* A process has exec'd.  Tell the kernel. */

	message m;

	m.m_lsys_krn_sys_exec.endpt = proc_ep;
	m.m_lsys_krn_sys_exec.stack = stack_ptr;
	m.m_lsys_krn_sys_exec.name = progname;
	m.m_lsys_krn_sys_exec.ip = pc;
	m.m_lsys_krn_sys_exec.ps_str = ps_str;

	return _kernel_call(SYS_EXEC, &m);
}
