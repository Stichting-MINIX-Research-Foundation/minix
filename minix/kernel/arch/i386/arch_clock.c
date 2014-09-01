
/* i386-specific clock functions. */

#include <machine/ports.h>
#include <minix/portio.h>

#include "kernel/kernel.h"

#include "kernel/clock.h"
#include "kernel/interrupt.h"
#include <minix/u64.h>
#include "glo.h"
#include "kernel/profile.h"


#ifdef USE_APIC
#include "apic.h"
#endif

#include "kernel/spinlock.h"

#ifdef CONFIG_SMP
#include "kernel/smp.h"
#endif

#define CLOCK_ACK_BIT   0x80    /* PS/2 clock interrupt acknowledge bit */

/* Clock parameters. */
#define COUNTER_FREQ (2*TIMER_FREQ) /* counter frequency using square wave */
#define LATCH_COUNT     0x00    /* cc00xxxx, c = channel, x = any */
#define SQUARE_WAVE     0x36    /* ccaammmb, a = access, m = mode, b = BCD */
                                /*   11x11, 11 = LSB then MSB, x11 = sq wave */
#define TIMER_FREQ  1193182    /* clock frequency for timer in PC and AT */
#define TIMER_COUNT(freq) (TIMER_FREQ/(freq)) /* initial value for counter*/

static irq_hook_t pic_timer_hook;		/* interrupt handler hook */

static unsigned probe_ticks;
static u64_t tsc0, tsc1;
#define PROBE_TICKS	(system_hz / 10)

static unsigned tsc_per_ms[CONFIG_MAX_CPUS];

/*===========================================================================*
 *				init_8235A_timer			     *
 *===========================================================================*/
int init_8253A_timer(const unsigned freq)
{
	/* Initialize channel 0 of the 8253A timer to, e.g., 60 Hz,
	 * and register the CLOCK task's interrupt handler to be run
	 * on every clock tick.
	 */
	outb(TIMER_MODE, SQUARE_WAVE);  /* run continuously */
	outb(TIMER0, (TIMER_COUNT(freq) & 0xff)); /* timer low byte */
	outb(TIMER0, TIMER_COUNT(freq) >> 8); /* timer high byte */

	return OK;
}

/*===========================================================================*
 *				stop_8235A_timer			     *
 *===========================================================================*/
void stop_8253A_timer(void)
{
	/* Reset the clock to the BIOS rate. (For rebooting.) */
	outb(TIMER_MODE, 0x36);
	outb(TIMER0, 0);
	outb(TIMER0, 0);
}

void arch_timer_int_handler(void)
{
}

static int calib_cpu_handler(irq_hook_t * UNUSED(hook))
{
	u64_t tsc;

	probe_ticks++;
	read_tsc_64(&tsc);


	if (probe_ticks == 1) {
		tsc0 = tsc;
	}
	else if (probe_ticks == PROBE_TICKS) {
		tsc1 = tsc;
	}

	/* just in case we are in an SMP single cpu fallback mode */
	BKL_UNLOCK();
	return 1;
}

static void estimate_cpu_freq(void)
{
	u64_t tsc_delta;
	u64_t cpu_freq;

	irq_hook_t calib_cpu;

	/* set the probe, we use the legacy timer, IRQ 0 */
	put_irq_handler(&calib_cpu, CLOCK_IRQ, calib_cpu_handler);

	/* just in case we are in an SMP single cpu fallback mode */
	BKL_UNLOCK();
	/* set the PIC timer to get some time */
	intr_enable();

	/* loop for some time to get a sample */
	while(probe_ticks < PROBE_TICKS) {
		intr_enable();
	}

	intr_disable();
	/* just in case we are in an SMP single cpu fallback mode */
	BKL_LOCK();

	/* remove the probe */
	rm_irq_handler(&calib_cpu);

	tsc_delta = tsc1 - tsc0;

	cpu_freq = (tsc_delta / (PROBE_TICKS - 1)) * system_hz;
	cpu_set_freq(cpuid, cpu_freq);
	cpu_info[cpuid].freq = (unsigned long)(cpu_freq / 1000000);
	BOOT_VERBOSE(cpu_print_freq(cpuid));
}

int init_local_timer(unsigned freq)
{
#ifdef USE_APIC
	/* if we know the address, lapic is enabled and we should use it */
	if (lapic_addr) {
		unsigned cpu = cpuid;
		tsc_per_ms[cpu] = (unsigned long)(cpu_get_freq(cpu) / 1000);
		lapic_set_timer_one_shot(1000000 / system_hz);
	} else {
		DEBUGBASIC(("Initiating legacy i8253 timer\n"));
#else
	{
#endif
		init_8253A_timer(freq);
		estimate_cpu_freq();
		/* always only 1 cpu in the system */
		tsc_per_ms[0] = (unsigned long)(cpu_get_freq(0) / 1000);
	}

	return 0;
}

void stop_local_timer(void)
{
#ifdef USE_APIC
	if (lapic_addr) {
		lapic_stop_timer();
		apic_eoi();
	} else
#endif
	{
		stop_8253A_timer();
	}
}

void restart_local_timer(void)
{
#ifdef USE_APIC
	if (lapic_addr) {
		lapic_restart_timer();
	}
#endif
}

int register_local_timer_handler(const irq_handler_t handler)
{
#ifdef USE_APIC
	if (lapic_addr) {
		/* Using APIC, it is configured in apic_idt_init() */
		BOOT_VERBOSE(printf("Using LAPIC timer as tick source\n"));
	} else
#endif
	{
		/* Using PIC, Initialize the CLOCK's interrupt hook. */
		pic_timer_hook.proc_nr_e = NONE;
		pic_timer_hook.irq = CLOCK_IRQ;

		put_irq_handler(&pic_timer_hook, CLOCK_IRQ, handler);
	}

	return 0;
}

void cycles_accounting_init(void)
{
#ifdef CONFIG_SMP
	unsigned cpu = cpuid;
#endif

	read_tsc_64(get_cpu_var_ptr(cpu, tsc_ctr_switch));

       get_cpu_var(cpu, cpu_last_tsc) = 0;
       get_cpu_var(cpu, cpu_last_idle) = 0;
}

void context_stop(struct proc * p)
{
	u64_t tsc, tsc_delta;
	u64_t * __tsc_ctr_switch = get_cpulocal_var_ptr(tsc_ctr_switch);
#ifdef CONFIG_SMP
	unsigned cpu = cpuid;
	int must_bkl_unlock = 0;

	/*
	 * This function is called only if we switch from kernel to user or idle
	 * or back. Therefore this is a perfect location to place the big kernel
	 * lock which will hopefully disappear soon.
	 *
	 * If we stop accounting for KERNEL we must unlock the BKL. If account
	 * for IDLE we must not hold the lock
	 */
	if (p == proc_addr(KERNEL)) {
		u64_t tmp;

		read_tsc_64(&tsc);
		tmp = tsc - *__tsc_ctr_switch;
		kernel_ticks[cpu] = kernel_ticks[cpu] + tmp;
		p->p_cycles = p->p_cycles + tmp;
		must_bkl_unlock = 1;
	} else {
		u64_t bkl_tsc;
		atomic_t succ;
		
		read_tsc_64(&bkl_tsc);
		/* this only gives a good estimate */
		succ = big_kernel_lock.val;
		
		BKL_LOCK();
		
		read_tsc_64(&tsc);

		bkl_ticks[cpu] = bkl_ticks[cpu] + tsc - bkl_tsc;
		bkl_tries[cpu]++;
		bkl_succ[cpu] += !(!(succ == 0));

		p->p_cycles = p->p_cycles + tsc - *__tsc_ctr_switch;

#ifdef CONFIG_SMP
		/*
		 * Since at the time we got a scheduling IPI we might have been
		 * waiting for BKL already, we may miss it due to a similar IPI to
		 * the cpu which is already waiting for us to handle its. This
		 * results in a live-lock of these two cpus.
		 *
		 * Therefore we always check if there is one pending and if so,
		 * we handle it straight away so the other cpu can continue and
		 * we do not deadlock.
		 */
		smp_sched_handler();
#endif
	}
#else
	read_tsc_64(&tsc);
	p->p_cycles = p->p_cycles + tsc - *__tsc_ctr_switch;
#endif
	
	tsc_delta = tsc - *__tsc_ctr_switch;

	if (kbill_ipc) {
		kbill_ipc->p_kipc_cycles =
			kbill_ipc->p_kipc_cycles + tsc_delta;
		kbill_ipc = NULL;
	}

	if (kbill_kcall) {
		kbill_kcall->p_kcall_cycles =
			kbill_kcall->p_kcall_cycles + tsc_delta;
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
		/* if (tsc_delta < p->p_cpu_time_left) in 64bit */
		if (ex64hi(tsc_delta) < ex64hi(p->p_cpu_time_left) ||
				(ex64hi(tsc_delta) == ex64hi(p->p_cpu_time_left) &&
				 ex64lo(tsc_delta) < ex64lo(p->p_cpu_time_left)))
			p->p_cpu_time_left = p->p_cpu_time_left - tsc_delta;
		else {
			p->p_cpu_time_left = 0;
		}
#endif
	}

	*__tsc_ctr_switch = tsc;

#ifdef CONFIG_SMP
	if(must_bkl_unlock) {
		BKL_UNLOCK();
	}
#endif
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

u64_t ms_2_cpu_time(unsigned ms)
{
	return (u64_t)tsc_per_ms[cpuid] * ms;
}

unsigned cpu_time_2_ms(u64_t cpu_time)
{
	return (unsigned long)(cpu_time / tsc_per_ms[cpuid]);
}

short cpu_load(void)
{
	u64_t current_tsc, *current_idle;
	u64_t tsc_delta, idle_delta, busy;
	struct proc *idle;
	short load;
#ifdef CONFIG_SMP
	unsigned cpu = cpuid;
#endif

	u64_t *last_tsc, *last_idle;

	last_tsc = get_cpu_var_ptr(cpu, cpu_last_tsc);
	last_idle = get_cpu_var_ptr(cpu, cpu_last_idle);

	idle = get_cpu_var_ptr(cpu, idle_proc);;
	read_tsc_64(&current_tsc);
	current_idle = &idle->p_cycles; /* ptr to idle proc */

	/* calculate load since last cpu_load invocation */
	if (*last_tsc) {
		tsc_delta = current_tsc - *last_tsc;
		idle_delta = *current_idle - *last_idle;

		busy = tsc_delta - idle_delta;
		busy = busy * 100;
		load = ex64lo(busy / tsc_delta);

		if (load > 100)
			load = 100;
	} else
		load = 0;
	
	*last_tsc = current_tsc;
	*last_idle = *current_idle;
	return load;
}

void busy_delay_ms(int ms)
{
	u64_t cycles = ms_2_cpu_time(ms), tsc0, tsc, tsc1;
	read_tsc_64(&tsc0);
	tsc1 = tsc0 + cycles;
	do { read_tsc_64(&tsc); } while(tsc < tsc1);
	return;
}

