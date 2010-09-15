#include "smp.h"
#include "interrupt.h"

unsigned ncpus;
unsigned ht_per_core;
unsigned bsp_cpu_id;

struct cpu cpus[CONFIG_MAX_CPUS];

static volatile unsigned ap_cpus_booted;

SPINLOCK_DEFINE(big_kernel_lock)
SPINLOCK_DEFINE(boot_lock)

PUBLIC void wait_for_APs_to_finish_booting(void)
{
	/* we must let the other CPUs to run in kernel mode first */
	BKL_UNLOCK();
	while (ap_cpus_booted != (ncpus - 1))
		arch_pause();
	/* now we have to take the lock again as we continu execution */
	BKL_LOCK();
}

PUBLIC void ap_boot_finished(unsigned cpu)
{
	ap_cpus_booted++;
}

PUBLIC void smp_ipi_halt_handler(void)
{
	ipi_ack();
	stop_local_timer();
	arch_smp_halt_cpu();
}

PUBLIC void smp_schedule(unsigned cpu)
{
	arch_send_smp_schedule_ipi(cpu);
}

PUBLIC void smp_ipi_sched_handler(void)
{
	struct proc * p;
	
	ipi_ack();
	
	p = get_cpulocal_var(proc_ptr);

	if (p->p_endpoint != IDLE)
		RTS_SET(p, RTS_PREEMPTED); /* calls dequeue() */
}

