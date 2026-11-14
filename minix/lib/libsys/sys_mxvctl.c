#include "syslib.h"
#include <minix/mxvctl.h>
#include <minix/com.h>
#include <minix/vm.h>
#include <errno.h>
#include <limits.h>
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

/* Internal fixed-layout bridge used by VM server for Phase-2 pin lifecycle. */
int
sys_mxvctl_req(struct mxvctl_req *req)
{
	message m;
	int r;

	if (req == NULL || req->version != MXVCTL_API_VERSION ||
	    req->grant_id != 0 || req->flags != 0 ||
	    req->reserved[0] != 0 || req->reserved[1] != 0)
		return EINVAL;
	memset(&m, 0, sizeof(m));
	m.m1_i1 = (int)req->version;
	m.m1_i2 = (int)req->operation;
	m.m9_ull1 = req->object_handle;
	m.m9_ull2 = req->arg0;
	m.m9_l1 = (long)req->arg1;
	r = _kernel_call(SYS_MXVCTL, &m);
	if (r == OK)
		req->arg0 = m.m9_ull2;
	return r;
}

int
sys_mxvctl_memslot(int operation, u64_t vm_handle,
    struct mxv_memslot_req *req)
{
	message m;
	int r;

	if (req == NULL || req->version != MXVCTL_API_VERSION ||
	    req->reserved != 0 ||
	    (operation != MXV_MEM_ATTACH && operation != MXV_MEM_DETACH))
		return EINVAL;
	if (operation == MXV_MEM_DETACH &&
	    (req->guest_phys != 0 || req->size != 0 ||
	     req->pin_token != 0 || req->flags != 0))
		return EINVAL;
	memset(&m, 0, sizeof(m));
	m.m1_i1 = (int)req->version;
	m.m1_i2 = operation;
	m.m9_ull1 = vm_handle;
	m.m9_ull2 = req->pin_token;
	m.m9_l1 = (long)req->guest_phys;
	m.m9_l2 = (long)req->size;
	m.m9_l3 = (long)req->flags;
	m.m9_l4 = (long)req->slot_id;
	m.m9_l5 = (long)req->generation;
	r = _kernel_call(SYS_MXVCTL, &m);
	if (r == OK && operation == MXV_MEM_ATTACH)
		req->generation = (u64_t)m.m9_l5;
	return r;
}

/* Public RS-side wrappers for the VM-owned region bookkeeping. */
int
vm_mxv_pin(void *addr, size_t len, u64_t *token)
{
	message m;
	int r;

	if (addr == NULL || len == 0 || token == NULL ||
	    len > (size_t)LONG_MAX)
		return EINVAL;
	memset(&m, 0, sizeof(m));
	m.m9_ull1 = (u64_t)(uintptr_t)addr;
	m.m9_l1 = (long)len;
	r = _taskcall(VM_PROC_NR, VM_MXV_MEM_PIN, &m);
	if (r == OK)
		*token = m.m9_ull2;
	return r;
}

int
vm_mxv_unpin(void *addr, u64_t token)
{
	message m;

	if (addr == NULL || token == 0)
		return EINVAL;
	memset(&m, 0, sizeof(m));
	m.m9_ull1 = (u64_t)(uintptr_t)addr;
	m.m9_ull2 = token;
	return _taskcall(VM_PROC_NR, VM_MXV_MEM_UNPIN, &m);
}
