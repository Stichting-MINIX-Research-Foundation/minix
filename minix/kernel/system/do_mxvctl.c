/*
 * MXV kernel control call.
 *
 * VM server owns the page pin lifecycle. RS owns kernel VM/EPT objects for
 * Phase 2. Keep this split explicit until mxvd has a delegated capability.
 */
#include "kernel/system.h"
#include "arch_proto.h"
#include <minix/mxvctl.h>
#include <errno.h>

static int
mxv_unused_fields_zero(message *m)
{
	return m->m9_l1 == 0 && m->m9_l2 == 0 && m->m9_l3 == 0 &&
	    m->m9_l4 == 0 && m->m9_l5 == 0;
}

int
do_mxvctl(struct proc *caller, message *m_ptr)
{
	u64_t value, generation;
	endpoint_t owner;
	int r, is_rs, is_vm;

	is_rs = (caller->p_nr == RS_PROC_NR);
	is_vm = (caller->p_nr == VM_PROC_NR);
	if (!is_rs && !is_vm)
		return EPERM;
	if (m_ptr->m1_i1 != MXVCTL_API_VERSION)
		return EINVAL;

	value = m_ptr->m1_ull1;
	switch (m_ptr->m1_i2) {
	case MXV_QUERY_CAPS:
		if (!is_rs)
			return EPERM;
		m_ptr->m1_ull1 = mxv_vmx_capabilities();
		return OK;
	case MXV_VMXON:
		if (!is_rs)
			return EPERM;
		return mxv_vmx_cpu_enable();
	case MXV_VMXOFF:
		if (!is_rs)
			return EPERM;
		return mxv_vmx_cpu_disable();
	case MXV_VCPU_CREATE:
		if (!is_rs)
			return EPERM;
		r = mxv_vcpu_create(caller->p_endpoint, &value);
		if (r == OK)
			m_ptr->m1_ull1 = value;
		return r;
	case MXV_VCPU_DESTROY:
		if (!is_rs)
			return EPERM;
		return mxv_vcpu_destroy(caller->p_endpoint, value);
	case MXV_VCPU_RUN_PROBE:
		if (!is_rs)
			return EPERM;
		r = mxv_vcpu_run_probe(caller->p_endpoint, value, &value);
		if (r == OK)
			m_ptr->m1_ull1 = value;
		return r;

	case MXV_PIN_RANGE:
		if (!is_vm)
			return EPERM;
		if (m_ptr->m9_l1 <= 0 || m_ptr->m9_l2 != 0 ||
		    m_ptr->m9_l3 != 0 || m_ptr->m9_l4 != 0 ||
		    m_ptr->m9_l5 != 0)
			return EINVAL;
		owner = (endpoint_t)m_ptr->m9_ull1;
		if ((u64_t)(int64_t)owner != m_ptr->m9_ull1)
			return EINVAL;
		r = mxv_pin_range(owner, (vir_bytes)m_ptr->m9_ull2,
		    (u64_t)m_ptr->m9_l1, &value);
		if (r == OK)
			m_ptr->m9_ull2 = value;
		return r;
	case MXV_PIN_RELEASE:
		if (!is_vm || !mxv_unused_fields_zero(m_ptr))
			return EPERM;
		owner = (endpoint_t)m_ptr->m9_ull1;
		if ((u64_t)(int64_t)owner != m_ptr->m9_ull1)
			return EINVAL;
		return mxv_pin_release(owner, m_ptr->m9_ull2);
	case MXV_OWNER_DIED:
		if (!is_vm || m_ptr->m9_ull2 != 0 ||
		    !mxv_unused_fields_zero(m_ptr))
			return EPERM;
		owner = (endpoint_t)m_ptr->m9_ull1;
		if ((u64_t)(int64_t)owner != m_ptr->m9_ull1)
			return EINVAL;
		mxv_owner_died(owner);
		return OK;

	case MXV_VM_CREATE:
		if (!is_rs)
			return EPERM;
		r = mxv_vm_create(caller->p_endpoint, &value);
		if (r == OK)
			m_ptr->m1_ull1 = value;
		return r;
	case MXV_VM_DESTROY:
		if (!is_rs)
			return EPERM;
		return mxv_vm_destroy(caller->p_endpoint, m_ptr->m1_ull1);
	case MXV_MEM_ATTACH:
		if (!is_rs)
			return EPERM;
		if (m_ptr->m9_l2 <= 0 || m_ptr->m9_l4 < 0 ||
		    m_ptr->m9_l5 != 0)
			return EINVAL;
		r = mxv_mem_attach(caller->p_endpoint, m_ptr->m9_ull1,
		    m_ptr->m9_ull2, (u64_t)m_ptr->m9_l1,
		    (u64_t)m_ptr->m9_l2, (u32_t)m_ptr->m9_l3,
		    (u32_t)m_ptr->m9_l4, &generation);
		if (r == OK)
			m_ptr->m9_l5 = (long)generation;
		return r;
	case MXV_MEM_DETACH:
		if (!is_rs || m_ptr->m9_ull2 != 0 ||
		    m_ptr->m9_l1 != 0 || m_ptr->m9_l2 != 0 ||
		    m_ptr->m9_l3 != 0 || m_ptr->m9_l4 < 0 ||
		    m_ptr->m9_l5 <= 0)
			return EINVAL;
		return mxv_mem_detach(caller->p_endpoint, m_ptr->m9_ull1,
		    (u32_t)m_ptr->m9_l4, (u64_t)m_ptr->m9_l5);
	default:
		return EINVAL;
	}
}
