#define _SYSTEM 1
#include <lib.h>
#include <unistd.h>
#include <string.h>

/* return -1, when the query itself or the processing of query has errors.
 * return 1, when there are more processes waiting to be queried.
 * return 0, when there are no more processes.
 * note that for the return value of 0 and 1, the 'endpt' is set accordingly.
 */
int vm_query_exit(int *endpt)
{
	message m;
	int r;

	r = _syscall(VM_PROC_NR, VM_QUERY_EXIT, &m);
	if (r != OK)
		return -1;
	if (endpt == NULL)
		return -1;

	*endpt = m.VM_QUERY_RET_PT;
	return (m.VM_QUERY_IS_MORE ? 1 : 0);
}

int vm_watch_exit(endpoint_t ep)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.VM_WE_EP = ep;
	return _syscall(VM_PROC_NR, VM_WATCH_EXIT, &m);
}
