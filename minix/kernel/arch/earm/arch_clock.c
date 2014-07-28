/* ARM-specific clock functions. */

#include "kernel/kernel.h"

#include "kernel/clock.h"
#include "kernel/proc.h"
#include "kernel/interrupt.h"
#include <minix/u64.h>
#include <minix/board.h>
#include "kernel/glo.h"
#include "kernel/profile.h"

#include <assert.h>


#include "kernel/spinlock.h"

#ifdef CONFIG_SMP
#include "kernel/smp.h"
#endif

#include "bsp_timer.h"
#include "bsp_intr.h"

static unsigned tsc_per_ms[CONFIG_MAX_CPUS];

int init_local_timer(unsigned freq)
{
	bsp_timer_init(freq);

	if (BOARD_IS_BBXM(machine.board_id)) {
		tsc_per_ms[0] = 16250;
	} else if (BOARD_IS_BB(machine.board_id)) {
		tsc_per_ms[0] = 15000;
	} else {
		panic("Can not do the clock setup. machine (0x%08x) is unknown\n",machine.board_id);
	};

	return 0;
}

void stop_local_timer(void)
{
	bsp_timer_stop();
}

void arch_timer_int_handler(void)
{
	bsp_timer_int_handler();
}

void cycles_accounting_init(void)
{
	read_tsc_64(get_cpu_var_ptr(cpu, tsc_ctr_switch));

	get_cpu_var(cpu, cpu_last_tsc) = 0;
	get_cpu_var(cpu, cpu_last_idle) = 0;
}

void context_stop(struct proc * p)
{
	u64_t tsc;
	u32_t tsc_delta;
	u64_t * __tsc_ctr_switch = get_cpulocal_var_ptr(tsc_ctr_switch);

	read_tsc_64(&tsc);
	assert(tsc >= *__tsc_ctr_switch);
	tsc_delta = tsc - *__tsc_ctr_switch;
	p->p_cycles += tsc_delta;

	if(kbill_ipc) {
		kbill_ipc->p_kipc_cycles += tsc_delta;
		kbill_ipc = NULL;
	}

	if(kbill_kcall) {
		kbill_kcall->p_kcall_cycles += tsc_delta;
		kbill_kcall = NULL;
	}

	/*
	 * deduct the just consumed cpu cycles from the cpu time left for this
	 * process during its current quantum. Skip IDLE and other pseudo kernel
	 * tasks
	 */
	if (p->p_endpoint >= 0) {
#if DEBUG_RACE
		p->p_cpu_time_left = 0;
#else
		if (tsc_delta < p->p_cpu_time_left) {
			p->p_cpu_time_left -= tsc_delta;
		} else p->p_cpu_time_left = 0;
#endif
	}

	*__tsc_ctr_switch = tsc;
}

void context_stop_idle(void)
{
	int is_idle;
#ifdef CONFIG_SMP
	unsigned cpu = cpuid;
#endif

	is_idle = get_cpu_var(cpu, cpu_is_idle);
	get_cpu_var(cpu, cpu_is_idle) = 0;

	context_stop(get_cpulocal_var_ptr(idle_proc));

	if (is_idle)
		restart_local_timer();
#if SPROFILE
	if (sprofiling)
		get_cpulocal_var(idle_interrupted) = 1;
#endif
}

void restart_local_timer(void)
{
}

int register_local_timer_handler(const irq_handler_t handler)
{
	return bsp_register_timer_handler(handler);
}

u64_t ms_2_cpu_time(unsigned ms)
{
	return (u64_t)(tsc_per_ms[cpuid]) * ms;
}

unsigned cpu_time_2_ms(u64_t cpu_time)
{
	return (unsigned long)(cpu_time / tsc_per_ms[cpuid]);
}

short cpu_load(void)
{
	return 0;
}
