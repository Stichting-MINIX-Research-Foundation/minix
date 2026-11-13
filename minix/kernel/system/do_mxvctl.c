/*
 * MXV kernel control call.
 *
 * Phase 1 is intentionally RS-only.  The userland VMM and the capability
 * delegation path are introduced in later phases.
 */
#include "kernel/system.h"
#include "arch_proto.h"
#include <minix/mxvctl.h>
#include <errno.h>

int do_mxvctl(struct proc *caller, message *m_ptr)
{
	u64_t value;
	int r;

	if (caller->p_nr != RS_PROC_NR)
		return EPERM;

	if (m_ptr->m1_i1 != MXVCTL_API_VERSION)
		return EINVAL;

	value = m_ptr->m1_ull1;

	switch (m_ptr->m1_i2) {
	case MXV_QUERY_CAPS:
		m_ptr->m1_ull1 = mxv_vmx_capabilities();
		return OK;

	case MXV_VMXON:
		return mxv_vmx_cpu_enable();

	case MXV_VMXOFF:
		return mxv_vmx_cpu_disable();

	case MXV_VCPU_CREATE:
		r = mxv_vcpu_create(caller->p_endpoint, &value);
		if (r == OK)
			m_ptr->m1_ull1 = value;
		return r;

	case MXV_VCPU_DESTROY:
		return mxv_vcpu_destroy(caller->p_endpoint, value);

	case MXV_VCPU_RUN_PROBE:
		r = mxv_vcpu_run_probe(caller->p_endpoint, value, &value);
		if (r == OK)
			m_ptr->m1_ull1 = value;
		return r;

	default:
		return EINVAL;
	}
}
