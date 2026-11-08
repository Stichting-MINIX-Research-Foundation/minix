#ifndef __SMP_H__
#define __SMP_H__

#ifdef CONFIG_SMP

#ifndef __ASSEMBLY__

#include "kernel/kernel.h"
#include "arch_smp.h"
#include "spinlock.h"

/* Upper bound of discovered logical CPU IDs; not the online CPU count. */
EXTERN unsigned ncpus;
/* Number of virtual strands per physical core */
EXTERN unsigned ht_per_core;
/* which cpu is bootstrapping */
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
void smp_init(void);

/*
 * CPU startup is a linear state machine. OFFLINE is the zero-initialized
 * state; only the listed forward transitions are permitted. In particular,
 * ONLINE and FAILED are mutually exclusive terminal states for boot.
 */
#define CPU_STATE_OFFLINE		0
#define CPU_STATE_STARTING		1
#define CPU_STATE_INITIALIZING		2
#define CPU_STATE_ONLINE		3
#define CPU_STATE_FAILED		4
#define CPU_STATE_STOPPING		5

struct cpu {
	volatile u32_t state;
};

EXTERN struct cpu cpus[CONFIG_MAX_CPUS];

/*
 * State changes race between the BSP and an AP. Compare-and-swap makes each
 * transition conditional on its exact predecessor, so a late AP cannot
 * overwrite a BSP timeout and an AP cannot become ONLINE after FAILED.
 */
#define cpu_state(cpu) \
	__sync_val_compare_and_swap(&cpus[cpu].state, \
		(u32_t)CPU_STATE_OFFLINE, (u32_t)CPU_STATE_OFFLINE)
#define cpu_transition(cpu, from, to) \
	__sync_bool_compare_and_swap(&cpus[cpu].state, \
		(u32_t)(from), (u32_t)(to))
#define cpu_is_ready(cpu) (cpu_state(cpu) == CPU_STATE_ONLINE)

/*
 * Big Kernel Lock prevents more then one cpu executing the kernel code
 */
SPINLOCK_DECLARE(big_kernel_lock)
/*
 * to sync the booting APs
 */
SPINLOCK_DECLARE(boot_lock)
	
void wait_for_APs_to_finish_booting(void);
int ap_boot_finished(unsigned cpu);
void ap_boot_failed(unsigned cpu);
unsigned smp_online_count(void);
u32_t smp_online_mask(void);
void smp_shutdown_aps(void );

/* IPI handlers */
void smp_ipi_halt_handler(void);
void smp_ipi_sched_handler(void);

void smp_schedule(unsigned cpu);
/* stop a processes on a different cpu */
void smp_schedule_stop_proc(struct proc * p);
/* stop a process on a different cpu because its address space is being changed */
void smp_schedule_vminhibit(struct proc * p);
/* stop the process and for saving its full context */
void smp_schedule_stop_proc_save_ctx(struct proc * p);
/* migrate the full context of a process to the destination CPU */
void smp_schedule_migrate_proc(struct proc * p, unsigned dest_cpu);
/* true while a cross-CPU scheduling request is in flight (run queues in flux) */
int smp_sched_ipi_pending(void);

void arch_send_smp_schedule_ipi(unsigned cpu);
void arch_smp_halt_cpu(void);

/* deal with x-cpu scheduling event */
void smp_sched_handler(void);

#endif /* __ASSEMBLY__ */

#endif /* CONFIG_SMP */

#endif /* __SMP_H__ */
