/* $NetBSD: hypercalls.h,v 1.2.20.1 2025/11/15 08:53:50 snj Exp $ */
/******************************************************************************
 * hypercall.h
 * 
 * Linux-specific hypervisor handling.
 * 
 * Copyright (c) 2002-2004, K A Fraser
 * 
 * 64-bit updates:
 *   Benjamin Liu <benjamin.liu@intel.com>
 *   Jun Nakajima <jun.nakajima@intel.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __HYPERCALL_H__
#define __HYPERCALL_H__

#define __STR(x) #x
#define STR(x) __STR(x)

#define HYPERCALL_STR(name)					\
	"call hypercall_page + ("STR(__HYPERVISOR_##name)" * 32)"

#define _hypercall0(type, name)			\
({						\
	long __res;				\
	asm volatile (				\
		HYPERCALL_STR(name)		\
		: "=a" (__res)			\
		:				\
		: "memory" );			\
	(type)__res;				\
})

#define _hypercall1(type, name, a1)				\
({								\
	long __res, __ign1;					\
	asm volatile (						\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1)			\
		: "1" ((long)(a1))				\
		: "memory" );					\
	(type)__res;						\
})

#define _hypercall2(type, name, a1, a2)				\
({								\
	long __res, __ign1, __ign2;				\
	asm volatile (						\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1), "=S" (__ign2)	\
		: "1" ((long)(a1)), "2" ((long)(a2))		\
		: "memory" );					\
	(type)__res;						\
})

#define _hypercall3(type, name, a1, a2, a3)			\
({								\
	long __res, __ign1, __ign2, __ign3;			\
	asm volatile (						\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1), "=S" (__ign2), 	\
		"=d" (__ign3)					\
		: "1" ((long)(a1)), "2" ((long)(a2)),		\
		"3" ((long)(a3))				\
		: "memory" );					\
	(type)__res;						\
})

#define _hypercall4(type, name, a1, a2, a3, a4)			\
({								\
	long __res, __ign1, __ign2, __ign3;			\
	asm volatile (						\
		"movq %7,%%r10; "				\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1), "=S" (__ign2),	\
		"=d" (__ign3)					\
		: "1" ((long)(a1)), "2" ((long)(a2)),		\
		"3" ((long)(a3)), "g" ((long)(a4))		\
		: "memory", "r10" );				\
	(type)__res;						\
})

#define _hypercall5(type, name, a1, a2, a3, a4, a5)		\
({								\
	long __res, __ign1, __ign2, __ign3;			\
	asm volatile (						\
		"movq %7,%%r10; movq %8,%%r8; "			\
		HYPERCALL_STR(name)				\
		: "=a" (__res), "=D" (__ign1), "=S" (__ign2),	\
		"=d" (__ign3)					\
		: "1" ((long)(a1)), "2" ((long)(a2)),		\
		"3" ((long)(a3)), "g" ((long)(a4)),		\
		"g" ((long)(a5))				\
		: "memory", "r10", "r8" );			\
	(type)__res;						\
})

static inline int
HYPERVISOR_set_trap_table(
	trap_info_t *table)
{
	return _hypercall1(int, set_trap_table, table);
}

static inline int
HYPERVISOR_mmu_update(
	mmu_update_t *req, int count, int *success_count, domid_t domid)
{
	return _hypercall4(int, mmu_update, req, count, success_count, domid);
}

static inline int
HYPERVISOR_mmuext_op(
	struct mmuext_op *op, int count, int *success_count, domid_t domid)
{
	return _hypercall4(int, mmuext_op, op, count, success_count, domid);
}

static inline int
HYPERVISOR_set_gdt(
	unsigned long *frame_list, int entries)
{
	return _hypercall2(int, set_gdt, frame_list, entries);
}

static inline int
HYPERVISOR_stack_switch(
	unsigned long ss, unsigned long esp)
{
	return _hypercall2(int, stack_switch, ss, esp);
}

static inline int
HYPERVISOR_set_callbacks(
	unsigned long event_address, unsigned long failsafe_address, 
	unsigned long syscall_address)
{
	return _hypercall3(int, set_callbacks,
			   event_address, failsafe_address, syscall_address);
}

static inline int
HYPERVISOR_fpu_taskswitch(
	int set)
{
	return _hypercall1(int, fpu_taskswitch, set);
}

static inline int
HYPERVISOR_sched_op_compat(
	int cmd, unsigned long arg)
{
	return _hypercall2(int, sched_op_compat, cmd, arg);
}

static inline int
HYPERVISOR_sched_op(
	int cmd, void *arg)
{
	return _hypercall2(int, sched_op, cmd, arg);
}

static inline long
HYPERVISOR_set_timer_op(
	u64 timeout)
{
	return _hypercall1(long, set_timer_op, timeout);
}

static inline int
HYPERVISOR_platform_op(
	struct xen_platform_op *platform_op)
{
	platform_op->interface_version = XENPF_INTERFACE_VERSION;
	return _hypercall1(int, platform_op, platform_op);
}

static inline int
HYPERVISOR_set_debugreg(
	int reg, unsigned long value)
{
	return _hypercall2(int, set_debugreg, reg, value);
}

static inline unsigned long
HYPERVISOR_get_debugreg(
	int reg)
{
	return _hypercall1(unsigned long, get_debugreg, reg);
}

static inline int
HYPERVISOR_update_descriptor(
	unsigned long ma, unsigned long word)
{
	return _hypercall2(int, update_descriptor, ma, word);
}

static inline int
HYPERVISOR_memory_op(
	unsigned int cmd, void *arg)
{
	return _hypercall2(int, memory_op, cmd, arg);
}

static inline int
HYPERVISOR_multicall(
	multicall_entry_t *call_list, int nr_calls)
{
	return _hypercall2(int, multicall, call_list, nr_calls);
}

static inline int
HYPERVISOR_update_va_mapping(
	unsigned long va, unsigned long new_val, unsigned long flags)
{
	return _hypercall3(int, update_va_mapping, va, new_val, flags);
}

static inline int
HYPERVISOR_event_channel_op(evtchn_op_t *op)
{
	KASSERT(op != NULL);
#if __XEN_INTERFACE_VERSION__ < 0x00030202
	return _hypercall1(int, event_channel_op, op);
#else
	return _hypercall2(int, event_channel_op, op->cmd, &op->u);
#endif
}

static inline int
HYPERVISOR_acm_op(
	int cmd, void *arg)
{
	return _hypercall2(int, acm_op, cmd, arg);
}

static inline int
HYPERVISOR_xen_version(
	int cmd, void *arg)
{
	return _hypercall2(int, xen_version, cmd, arg);
}

static inline int
HYPERVISOR_console_io(
	int cmd, int count, char *str)
{
	return _hypercall3(int, console_io, cmd, count, str);
}

static inline int
HYPERVISOR_physdev_op(int cmd, void *op)
{
	return _hypercall2(int, physdev_op, cmd, op);
}

static inline int
HYPERVISOR_grant_table_op(
	unsigned int cmd, void *uop, unsigned int count)
{
	return _hypercall3(int, grant_table_op, cmd, uop, count);
}

static inline int
HYPERVISOR_update_va_mapping_otherdomain(
	unsigned long va, unsigned long new_val, unsigned long flags,
	domid_t domid)
{
	return _hypercall4(int, update_va_mapping_otherdomain, va,
			   new_val, flags, domid);
}

static inline int
HYPERVISOR_vm_assist(
	unsigned int cmd, unsigned int type)
{
	return _hypercall2(int, vm_assist, cmd, type);
}

static inline int
HYPERVISOR_vcpu_op(
	int cmd, int vcpuid, void *extra_args)
{
	return _hypercall3(int, vcpu_op, cmd, vcpuid, extra_args);
}

static inline int
HYPERVISOR_set_segment_base(
	int reg, unsigned long value)
{
	return _hypercall2(int, set_segment_base, reg, value);
}

static inline int
HYPERVISOR_suspend(
	unsigned long srec)
{
#if __XEN_INTERFACE_VERSION__ >= 0x00030201

	struct sched_shutdown shutdown_reason = {
		.reason = SHUTDOWN_suspend,
	};

	return _hypercall3(int, sched_op, SCHEDOP_shutdown,
	    &shutdown_reason, srec);
#else
	return _hypercall3(int, sched_op, SCHEDOP_shutdown,
				 SHUTDOWN_suspend, srec);
#endif
}

static inline long
HYPERVISOR_yield(
	void)
{
	return _hypercall2(int, sched_op, SCHEDOP_yield, 0);
}

static inline long
HYPERVISOR_block(
	void)
{
	return _hypercall2(int, sched_op, SCHEDOP_block, 0);
}

static inline long
HYPERVISOR_shutdown(
	void)
{
#if __XEN_INTERFACE_VERSION__ >= 0x00030201

	struct sched_shutdown shutdown_reason = {
		.reason = SHUTDOWN_poweroff,
	};

	return _hypercall2(int, sched_op, SCHEDOP_shutdown,
	    &shutdown_reason);
#else
	return _hypercall2(int, sched_op, SCHEDOP_shutdown, SHUTDOWN_poweroff);
#endif
}

static inline long
HYPERVISOR_crash(
	void)
{
#if __XEN_INTERFACE_VERSION__ >= 0x00030201

	struct sched_shutdown shutdown_reason = {
		.reason = SHUTDOWN_crash,
	};

	return _hypercall2(int, sched_op, SCHEDOP_shutdown,
	    &shutdown_reason);
#else
	return _hypercall2(int, sched_op, SCHEDOP_shutdown, SHUTDOWN_crash);
#endif
}

static inline long
HYPERVISOR_reboot(
	void)
{
#if __XEN_INTERFACE_VERSION__ >= 0x00030201

	struct sched_shutdown shutdown_reason = {
		.reason = SHUTDOWN_reboot,
	};

	return _hypercall2(int, sched_op, SCHEDOP_shutdown,
	    &shutdown_reason);
#else
	return _hypercall2(int, sched_op, SCHEDOP_shutdown, SHUTDOWN_reboot);
#endif
}

static inline int
HYPERVISOR_nmi_op(
	unsigned long op, void *arg)
{
	return _hypercall2(int, nmi_op, op, arg);
}

static inline long
HYPERVISOR_hvm_op(
    int op, void *arg)
{
    return _hypercall2(long, hvm_op, op, arg);
}

static inline int
HYPERVISOR_callback_op(
	int cmd, void *arg)
{
	return _hypercall2(int, callback_op, cmd, arg);
}

static inline int
HYPERVISOR_xenoprof_op(
	int op, void *arg)
{
	return _hypercall2(int, xenoprof_op, op, arg);
}

static inline int
HYPERVISOR_kexec_op(
	unsigned long op, void *args)
{
	return _hypercall2(int, kexec_op, op, args);
}

#if __XEN_INTERFACE_VERSION__ < 0x00030204
static inline int
HYPERVISOR_dom0_op(
	dom0_op_t *dom0_op)
{
	dom0_op->interface_version = DOM0_INTERFACE_VERSION;
	return _hypercall1(int, dom0_op, dom0_op);
}
#endif	/* __XEN_INTERFACE_VERSION__ */

#include <xen/include/public/arch-x86/xen-mca.h>

static inline int
HYPERVISOR_machine_check(struct xen_mc *mc)
{
	mc->interface_version = XEN_MCA_INTERFACE_VERSION;
	return _hypercall1(int, mca, mc);
}

static inline int
HYPERVISOR_sysctl(void *sysctl)
{
	return _hypercall1(int, sysctl, sysctl);
}

#endif /* __HYPERCALL_H__ */
