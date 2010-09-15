#ifndef __SMP_H__
#define __SMP_H__

#ifdef CONFIG_SMP

#ifndef __ASSEMBLY__

#include "kernel.h"
#include "arch_smp.h"
#include "spinlock.h"

/* number of CPUs (execution strands in the system */
EXTERN unsigned ncpus;
/* Number of virtual strands per physical core */
EXTERN unsigned ht_per_core;
/* which cpu is bootstraping */
EXTERN unsigned bsp_cpu_id;

#define cpu_is_bsp(cpu)	(bsp_cpu_id == cpu)

/*
 * SMP initialization is largely architecture dependent and each architecture
 * must provide a method how to do it. If initiating SMP fails the function does
 * not report it. However it must put the system in such a state that it falls
 * back to a uniprocessor system. Although the uniprocessor configuration may be
 * suboptimal, the system must be able to run on the bootstrap processor as if
 * it was the only processor in the system
 */
_PROTOTYPE(void smp_init, (void));

_PROTOTYPE(void smp_ipi_err_int, (void));
_PROTOTYPE(void smp_ipi_spv_int, (void));
_PROTOTYPE(void smp_ipi_sched, (void));
_PROTOTYPE(void smp_ipi_dequeue, (void));
_PROTOTYPE(void smp_ipi_stop, (void));
_PROTOTYPE(void smp_ipi_reboot, (void));

#define CPU_IS_BSP	1
#define CPU_IS_READY	2

struct cpu {
	u32_t flags;
};

EXTERN struct cpu cpus[CONFIG_MAX_CPUS];

#define cpu_set_flag(cpu, flag)	do { cpus[cpu].flags |= (flag); } while(0)
#define cpu_clear_flag(cpu, flag) do { cpus[cpu].flags &= ~(flag); } while(0)
#define cpu_test_flag(cpu, flag) (cpus[cpu].flags & (flag))
#define cpu_is_ready(cpu) cpu_test_flag(cpu, CPU_IS_READY)

SPINLOCK_DECLARE(big_kernel_lock)

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_SMP */

#endif /* __SMP_H__ */
