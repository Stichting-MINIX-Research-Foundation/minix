#include "syslib.h"
#include <unistd.h>
#include <string.h>

/*
 * Return a negative error code if the query itself or the processing of the
 * query resulted in an error.
 * Return 1 if there are more processes waiting to be queried.
 * Return 0 if there are no more processes.
 * Note that for the return value of 0 and 1, the 'endpt' is set accordingly.
 */
int
vm_query_exit(int *endpt)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	r = _taskcall(VM_PROC_NR, VM_QUERY_EXIT, &m);
	if (r != OK)
		return r;
	if (endpt == NULL)
		return EFAULT;

	*endpt = m.m_lsys_vm_query_exit.ret_pt;
	return (m.m_lsys_vm_query_exit.is_more ? 1 : 0);
}

int
vm_watch_exit(endpoint_t ep)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lsys_vm_watch_exit.ep = ep;
	return _taskcall(VM_PROC_NR, VM_WATCH_EXIT, &m);
}
