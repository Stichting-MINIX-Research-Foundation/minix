#include <assert.h>
#include <minix/u64.h>

#include "smp.h"
#include "interrupt.h"
#include "clock.h"

unsigned ncpus;
unsigned ht_per_core;
unsigned bsp_cpu_id;

struct cpu cpus[CONFIG_MAX_CPUS];

/* info passed to another cpu along with a sched ipi */
struct sched_ipi_data {
	volatile u32_t		flags;
	volatile vir_bytes	data;	/* holds a struct proc *; must be
					 * pointer-wide (64-bit on x86_64) */
};

static struct sched_ipi_data  sched_ipi_data[CONFIG_MAX_CPUS];

/*
 * True while any cross-CPU scheduling request (STOP_PROC / VM_INHIBIT /
 * SAVE_CTX / migrate) is in flight.  smp_schedule_sync() drops the BKL and
 * spin-waits for the target CPU to ack, so during that window another CPU can
 * enter the kernel and observe the run queues / proc table mid-transition.
 * The DEBUG_SANITYCHECKS run-queue walk must not treat that transient as an
 * error.
 */
int smp_sched_ipi_pending(void)
{
	unsigned c;

	for (c = 0; c < ncpus; c++)
		if (sched_ipi_data[c].flags != 0)
			return 1;
	return 0;
}

#define SCHED_IPI_STOP_PROC	1
#define SCHED_IPI_VM_INHIBIT	2
#define SCHED_IPI_SAVE_CTX	4

SPINLOCK_DEFINE(big_kernel_lock)
SPINLOCK_DEFINE(boot_lock)

void wait_for_APs_to_finish_booting(void)
{
	unsigned cpu;
	int initializing;
	u64_t start, now, timeout;

	/*
	 * An AP owns INITIALIZING once it has entered C code. Bound this phase
	 * as well as the earlier INIT/SIPI handshake. The BSP may declare an AP
	 * failed only by winning INITIALIZING -> FAILED; the AP may declare it
	 * online only by winning INITIALIZING -> ONLINE. Exactly one can win.
	 *
	 * The BSP timer has been calibrated by boot_cpu_init_timer() before this
	 * function is called, so the TSC-based deadline works even while the
	 * kernel is still in its boot-time interrupt state.
	 */
	read_tsc_64(&start);
	timeout = ms_2_cpu_time(5000);

	BKL_UNLOCK();
	do {
		initializing = 0;
		for (cpu = 0; cpu < ncpus; cpu++) {
			if (cpu_state(cpu) == CPU_STATE_INITIALIZING) {
				initializing = 1;
				break;
			}
		}

		if (initializing) {
			read_tsc_64(&now);
			if (now - start >= timeout) {
				for (cpu = 0; cpu < ncpus; cpu++) {
					if (cpu_transition(cpu,
					    CPU_STATE_INITIALIZING,
					    CPU_STATE_FAILED))
						printf("WARNING CPU %u timed out during initialization\n",
						    cpu);
				}
				break;
			}
			arch_pause();
		}
	} while (initializing);

	barrier();
	BKL_LOCK();

	for (cpu = 0; cpu < ncpus; cpu++) {
		if (cpu_state(cpu) == CPU_STATE_FAILED)
			printf("WARNING CPU %u failed to initialize\n", cpu);
	}
}

int ap_boot_finished(unsigned cpu)
{
	assert(cpu < ncpus);

	/* Publish initialized CPU-local state with one terminal transition. */
	return cpu_transition(cpu, CPU_STATE_INITIALIZING, CPU_STATE_ONLINE);
}

void ap_boot_failed(unsigned cpu)
{
	assert(cpu < ncpus);

	/* A BSP timeout may already have won this transition. */
	(void)cpu_transition(cpu, CPU_STATE_INITIALIZING, CPU_STATE_FAILED);
}

unsigned smp_online_count(void)
{
	unsigned cpu, count = 0;

	for (cpu = 0; cpu < ncpus; cpu++) {
		if (cpu_state(cpu) == CPU_STATE_ONLINE)
			count++;
	}
	return count;
}

u32_t smp_online_mask(void)
{
	unsigned cpu;
	u32_t mask = 0;

	for (cpu = 0; cpu < ncpus; cpu++) {
		if (cpu_state(cpu) == CPU_STATE_ONLINE)
			mask |= (1U << cpu);
	}
	return mask;
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

	sched_ipi_data[cpu].data = (vir_bytes) p;
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

