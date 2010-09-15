#include "smp.h"

unsigned ncpus;
unsigned ht_per_core;
unsigned bsp_cpu_id;

struct cpu cpus[CONFIG_MAX_CPUS];

SPINLOCK_DEFINE(big_kernel_lock)
