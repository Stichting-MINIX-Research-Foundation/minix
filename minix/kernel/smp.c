#include <assert.h>

#include "smp.h"
#include "interrupt.h"
#include "clock.h"

unsigned ncpus;
unsigned ht_per_core;
unsigned bsp_cpu_id;

struct cpu cpus[CONFIG_MAX_CPUS];

/* info passed to another cpu along with a sched ipi */
struct sched_ipi_data {
	volatile u32_t	flags;
	volatile u32_t	data;
};

static struct sched_ipi_data  sched_ipi_data[CONFIG_MAX_CPUS];

#define SCHED_IPI_STOP_PROC	1
#define SCHED_IPI_VM_INHIBIT	2
#define SCHED_IPI_SAVE_CTX	4

static volatile unsigned ap_cpus_booted;

SPINLOCK_DEFINE(big_kernel_lock)
SPINLOCK_DEFINE(boot_lock)

void wait_for_APs_to_finish_booting(void)
{
	unsigned n = 0;
	int i;

	/* check how many cpus are actually alive */
	for (i = 0 ; i < ncpus ; i++) {
		if (cpu_test_flag(i, CPU_IS_READY))
			n++;
	}
	if (n != ncpus)
		printf("WARNING only %d out of %d cpus booted\n", n, ncpus);

	/* we must let the other CPUs to run in kernel mode first */
	BKL_UNLOCK();
	while (ap_cpus_booted != (n - 1))
		arch_pause();
	/* now we have to take the lock again as we continue execution */
	BKL_LOCK();
}

void ap_boot_finished(unsigned cpu)
{
	ap_cpus_booted++;
}

void smp_ipi_halt_handler(void)
{
	ipi_ack();
	stop_local_timer();
	arch_smp_halt_cpu();
}

void smp_schedule(unsigned cpu)
{
	arch_send_smp_schedule_ipi(cpu);
}

void smp_sched_handler(void);

/*
 * tell another cpu about a task to do and return only after the cpu acks that
 * the task is finished. Also wait before it finishes task sent by another cpu
 * to the same one.
 */
static void smp_schedule_sync(struct proc * p, unsigned task)
{
	unsigned cpu = p->p_cpu;
	unsigned mycpu = cpuid;

	assert(cpu != mycpu);
	/*
	 * if some other cpu made a request to the same cpu, wait until it is
	 * done before proceeding
	 */
	if (sched_ipi_data[cpu].flags != 0) {
		BKL_UNLOCK();
		while (sched_ipi_data[cpu].flags != 0) {
			if (sched_ipi_data[mycpu].flags) {
				BKL_LOCK();
				smp_sched_handler();
				BKL_UNLOCK();
			}
		}
		BKL_LOCK();
	}

	sched_ipi_data[cpu].data = (u32_t) p;
	sched_ipi_data[cpu].flags |= task;
	__insn_barrier();
	arch_send_smp_schedule_ipi(cpu);

	/* wait until the destination cpu finishes its job */
	BKL_UNLOCK();
	while (sched_ipi_data[cpu].flags != 0) {
		if (sched_ipi_data[mycpu].flags) {
			BKL_LOCK();
			smp_sched_handler();
			BKL_UNLOCK();
		}
	}
	BKL_LOCK();
}

void smp_schedule_stop_proc(struct proc * p)
{
	if (proc_is_runnable(p))
		smp_schedule_sync(p, SCHED_IPI_STOP_PROC);
	else
		RTS_SET(p, RTS_PROC_STOP);
	assert(RTS_ISSET(p, RTS_PROC_STOP));
}

void smp_schedule_vminhibit(struct proc * p)
{
	if (proc_is_runnable(p))
		smp_schedule_sync(p, SCHED_IPI_VM_INHIBIT);
	else
		RTS_SET(p, RTS_VMINHIBIT);
	assert(RTS_ISSET(p, RTS_VMINHIBIT));
}

void smp_schedule_stop_proc_save_ctx(struct proc * p)
{
	/*
	 * stop the processes and force the complete context of the process to
	 * be saved (i.e. including FPU state and such)
	 */
	smp_schedule_sync(p, SCHED_IPI_STOP_PROC | SCHED_IPI_SAVE_CTX);
	assert(RTS_ISSET(p, RTS_PROC_STOP));
}

void smp_schedule_migrate_proc(struct proc * p, unsigned dest_cpu)
{
	/*
	 * stop the processes and force the complete context of the process to
	 * be saved (i.e. including FPU state and such)
	 */
	smp_schedule_sync(p, SCHED_IPI_STOP_PROC | SCHED_IPI_SAVE_CTX);
	assert(RTS_ISSET(p, RTS_PROC_STOP));
	
	/* assign the new cpu and let the process run again */
	p->p_cpu = dest_cpu;
	RTS_UNSET(p, RTS_PROC_STOP);
}

void smp_sched_handler(void)
{
	unsigned flgs;
	unsigned cpu = cpuid;

	flgs = sched_ipi_data[cpu].flags;

	if (flgs) {
		struct proc * p;
		p = (struct proc *)sched_ipi_data[cpu].data;

		if (flgs & SCHED_IPI_STOP_PROC) {
			RTS_SET(p, RTS_PROC_STOP);
		}
		if (flgs & SCHED_IPI_SAVE_CTX) {
			/* all context has been saved already, FPU remains */
			if (proc_used_fpu(p) &&
					get_cpulocal_var(fpu_owner) == p) {
				disable_fpu_exception();
				save_local_fpu(p, FALSE /*retain*/);
				/* we're preparing to migrate somewhere else */
				release_fpu(p);
			}
		}
		if (flgs & SCHED_IPI_VM_INHIBIT) {
			RTS_SET(p, RTS_VMINHIBIT);
		}
	}

	__insn_barrier();
	sched_ipi_data[cpu].flags = 0;
}

/*
 * This function gets always called only after smp_sched_handler() has been
 * already called. It only serves the purpose of acknowledging the IPI and
 * preempting the current process if the CPU was not idle.
 */
void smp_ipi_sched_handler(void)
{
	struct proc * curr;

	ipi_ack();

	curr = get_cpulocal_var(proc_ptr);
	if (curr->p_endpoint != IDLE) {
		RTS_SET(curr, RTS_PREEMPTED);
	}
}

