#include "kernel/kernel.h"
#include "kernel/watchdog.h"
#include "proto.h"
#include <minix/minlib.h>
#include <minix/u64.h>

#include "apic.h"

#define CPUID_UNHALTED_CORE_CYCLES_AVAILABLE	0

#define MSR_PERFMON_CRT0	0xc1
#define MSR_PERFMON_SEL0	0x186

#define MSR_PERFMON_SEL0_ENABLE	(1 << 22)

/*
 * Intel architecture performance counters watchdog
 */

PRIVATE void intel_arch_watchdog_init(int cpu)
{
	u64_t cpuf;
	u32_t val;

	ia32_msr_write(MSR_PERFMON_CRT0, 0, 0);

	/* Int, OS, USR, Core ccyles */
	val = 1 << 20 | 1 << 17 | 1 << 16 | 0x3c;
	ia32_msr_write(MSR_PERFMON_SEL0, 0, val);

	/*
	 * should give as a tick approx. every 0.5-1s, the perf counter has only
	 * lowest 31 bits writable :(
	 */
	cpuf = cpu_get_freq(cpu);
	while (cpuf.hi || cpuf.lo > 0x7fffffffU)
		cpuf = div64u64(cpuf, 2);
	watchdog->resetval = cpuf.lo;

	ia32_msr_write(MSR_PERFMON_CRT0, 0, -cpuf.lo);

	ia32_msr_write(MSR_PERFMON_SEL0, 0, val | MSR_PERFMON_SEL0_ENABLE);

	/* unmask the performance counter interrupt */
	lapic_write(LAPIC_LVTPCR, APIC_ICR_DM_NMI);
}

PRIVATE void intel_arch_watchdog_reinit(const int cpu)
{
	lapic_write(LAPIC_LVTPCR, APIC_ICR_DM_NMI);
	ia32_msr_write(MSR_PERFMON_CRT0, 0, -watchdog->resetval);
}

PRIVATE struct arch_watchdog intel_arch_watchdog = {
	/*.init = */	intel_arch_watchdog_init,
	/*.reinit = */	intel_arch_watchdog_reinit
};

int arch_watchdog_init(void)
{
	u32_t eax, ebx, ecx, edx;

	eax = 0xA;

	_cpuid(&eax, &ebx, &ecx, &edx);

	/* FIXME currently we support only watchdog base on the intel
	 * architectural performance counters. Some Intel CPUs don't have this
	 * feature
	 */
	if (ebx & (1 << CPUID_UNHALTED_CORE_CYCLES_AVAILABLE))
		return -1;
	if (!((((eax >> 8)) & 0xff) > 0))
		return -1;

	watchdog = &intel_arch_watchdog;

	/* Setup PC tas NMI for watchdog, is is masked for now */
	lapic_write(LAPIC_LVTPCR, APIC_ICR_INT_MASK | APIC_ICR_DM_NMI);
	(void) lapic_read(LAPIC_LVTPCR);

	/* double check if LAPIC is enabled */
	if (lapic_addr && watchdog_enabled && watchdog->init) {
		watchdog->init(cpuid);
	}

	return 0;
}

void arch_watchdog_lockup(const struct nmi_frame * frame)
{
	printf("KERNEL LOCK UP\n"
			"eax    0x%08x\n"
			"ecx    0x%08x\n"
			"edx    0x%08x\n"
			"ebx    0x%08x\n"
			"ebp    0x%08x\n"
			"esi    0x%08x\n"
			"edi    0x%08x\n"
			"gs     0x%08x\n"
			"fs     0x%08x\n"
			"es     0x%08x\n"
			"ds     0x%08x\n"
			"pc     0x%08x\n"
			"cs     0x%08x\n"
			"eflags 0x%08x\n",
			frame->eax,
			frame->ecx,
			frame->edx,
			frame->ebx,
			frame->ebp,
			frame->esi,
			frame->edi,
			frame->gs,
			frame->fs,
			frame->es,
			frame->ds,
			frame->pc,
			frame->cs,
			frame->eflags
			);
	panic("Kernel lockup");
}

void i386_watchdog_start(void)
{
	if (watchdog_enabled) {
		if (arch_watchdog_init()) {
			printf("WARNING watchdog initialization "
					"failed! Disabled\n");
			watchdog_enabled = 0;
		}
		else
			BOOT_VERBOSE(printf("Watchdog enabled\n"););
	}
}
