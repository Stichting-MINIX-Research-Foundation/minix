#include "smp.h"

unsigned ncpus;
unsigned ht_per_core;
unsigned bsp_cpu_id;

struct cpu cpus[CONFIG_MAX_CPUS];

static volatile unsigned ap_cpus_booted;

SPINLOCK_DEFINE(big_kernel_lock)
SPINLOCK_DEFINE(boot_lock)

void wait_for_APs_to_finish_booting(void)
{
	/* we must let the other CPUs to run in kernel mode first */
	BKL_UNLOCK();
	while (ap_cpus_booted != (ncpus - 1))
		arch_pause();
	/* now we have to take the lock again as we continu execution */
	BKL_LOCK();
}

void ap_boot_finished(unsigned cpu)
{
	printf("CPU %d is running\n", cpu);

	ap_cpus_booted++;
}
