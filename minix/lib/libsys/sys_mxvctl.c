#include "syslib.h"
#include <minix/mxvctl.h>
#include <errno.h>
#include <string.h>

/*
 * Invoke the Phase-1 MXV kernel control interface.
 *
 * The value is an input argument and, for operations that produce a value,
 * is replaced with the result.  For MXV_QUERY_CAPS it receives the
 * MXV_CAP_* bitmask; for create it receives an opaque vCPU handle; for the
 * probe it receives the VM-exit reason.
 */
int sys_mxvctl(int operation, u64_t *value)
{
	message m;
	int r;

	if (value == NULL)
		return EINVAL;

	memset(&m, 0, sizeof(m));
	m.m1_i1 = MXVCTL_API_VERSION;
	m.m1_i2 = operation;
	m.m1_ull1 = *value;

	r = _kernel_call(SYS_MXVCTL, &m);
	if (r == OK)
		*value = m.m1_ull1;

	return r;
}
