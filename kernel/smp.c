#include "smp.h"
#include "interrupt.h"

unsigned ncpus;
unsigned ht_per_core;
unsigned bsp_cpu_id;

PUBLIC struct cpu cpus[CONFIG_MAX_CPUS];

/* flags passed to another cpu along with a sched ipi */
struct sched_ipi_data {
	volatile u32_t	flags;
	volatile u32_t	data;
};

PRIVATE struct sched_ipi_data  sched_ipi_data[CONFIG_MAX_CPUS];

#define SCHED_IPI_STOP_PROC	1

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
	/*
	 * check if the cpu is processing some other ipi already. If yes, no
	 * need to wake it up
	 */
	if ((volatile unsigned)sched_ipi_data[cpu].flags != 0)
		return;
	arch_send_smp_schedule_ipi(cpu);
}

PUBLIC void smp_schedule_stop_proc(struct proc * p)
{
	unsigned cpu = p->p_cpu;

	sched_ipi_data[cpu].flags |= SCHED_IPI_STOP_PROC;
	sched_ipi_data[cpu].data = (u32_t) p;
	arch_send_smp_schedule_ipi(cpu);
	BKL_UNLOCK();
	while ((volatile unsigned)sched_ipi_data[cpu].flags != 0);
	BKL_LOCK();
}

PUBLIC void smp_ipi_sched_handler(void)
{
	struct proc * p;
	unsigned mycpu = cpuid;
	unsigned flgs;
	
	ipi_ack();
	
	p = get_cpu_var(mycpu, proc_ptr);
	flgs = sched_ipi_data[mycpu].flags;

	if (flgs & SCHED_IPI_STOP_PROC) {
		RTS_SET((struct proc *)sched_ipi_data[mycpu].data, RTS_PROC_STOP);
	}
	else if (p->p_endpoint != IDLE) {
		RTS_SET(p, RTS_PREEMPTED);
	}
	sched_ipi_data[cpuid].flags = 0;
}

