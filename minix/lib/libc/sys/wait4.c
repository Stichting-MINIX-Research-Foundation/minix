#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <sys/wait.h>

#ifdef __weak_alias
__weak_alias(wait4, __wait450)
#endif

pid_t
wait4(pid_t pid, int * status, int options, struct rusage * rusage)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_pm_wait4.pid = pid;
	m.m_lc_pm_wait4.options = options;
	m.m_lc_pm_wait4.addr = (vir_bytes)rusage;

	if (_syscall(PM_PROC_NR, PM_WAIT4, &m) < 0) return(-1);

	if (status != NULL) *status = m.m_pm_lc_wait4.status;
	return m.m_type;
}
