
/* i386-specific clock functions. */

#include <machine/ports.h>
#include <minix/portio.h>

#include "kernel/kernel.h"

#include "kernel/clock.h"
#include "kernel/proc.h"
#include <minix/u64.h>


#ifdef CONFIG_APIC
#include "apic.h"
#endif

#define CLOCK_ACK_BIT   0x80    /* PS/2 clock interrupt acknowledge bit */

/* Clock parameters. */
#define COUNTER_FREQ (2*TIMER_FREQ) /* counter frequency using square wave */
#define LATCH_COUNT     0x00    /* cc00xxxx, c = channel, x = any */
#define SQUARE_WAVE     0x36    /* ccaammmb, a = access, m = mode, b = BCD */
                                /*   11x11, 11 = LSB then MSB, x11 = sq wave */
#define TIMER_FREQ  1193182    /* clock frequency for timer in PC and AT */
#define TIMER_COUNT(freq) (TIMER_FREQ/(freq)) /* initial value for counter*/

/* FIXME make it cpu local! */
PRIVATE u64_t tsc_ctr_switch; /* when did we switched time accounting */

PRIVATE irq_hook_t pic_timer_hook;		/* interrupt handler hook */

PRIVATE unsigned probe_ticks;
PRIVATE u64_t tsc0, tsc1;
#define PROBE_TICKS	(system_hz / 10)

PRIVATE unsigned tsc_per_ms[CONFIG_MAX_CPUS];

/*===========================================================================*
 *				init_8235A_timer			     *
 *===========================================================================*/
PUBLIC int init_8253A_timer(const unsigned freq)
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
PUBLIC void stop_8253A_timer(void)
{
	/* Reset the clock to the BIOS rate. (For rebooting.) */
	outb(TIMER_MODE, 0x36);
	outb(TIMER0, 0);
	outb(TIMER0, 0);
}

PRIVATE int calib_cpu_handler(irq_hook_t * UNUSED(hook))
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

	return 1;
}

PRIVATE void estimate_cpu_freq(void)
{
	u64_t tsc_delta;
	u64_t cpu_freq;

	irq_hook_t calib_cpu;

	/* set the probe, we use the legacy timer, IRQ 0 */
	put_irq_handler(&calib_cpu, CLOCK_IRQ, calib_cpu_handler);

	/* set the PIC timer to get some time */
	intr_enable();

	/* loop for some time to get a sample */
	while(probe_ticks < PROBE_TICKS) {
		intr_enable();
	}

	intr_disable();

	/* remove the probe */
	rm_irq_handler(&calib_cpu);

	tsc_delta = sub64(tsc1, tsc0);

	cpu_freq = mul64(div64u64(tsc_delta, PROBE_TICKS - 1), make64(system_hz, 0));
	cpu_set_freq(cpuid, cpu_freq);
	BOOT_VERBOSE(cpu_print_freq(cpuid));
}

PUBLIC int arch_init_local_timer(unsigned freq)
{
#ifdef CONFIG_APIC
	/* if we know the address, lapic is enabled and we should use it */
	if (lapic_addr) {
		unsigned cpu = cpuid;
		lapic_set_timer_periodic(freq);
		tsc_per_ms[cpu] = div64u(cpu_get_freq(cpu), 1000);
	} else
	{
		BOOT_VERBOSE(printf("Initiating legacy i8253 timer\n"));
#else
	{
#endif
		init_8253A_timer(freq);
		estimate_cpu_freq();
		/* always only 1 cpu in the system */
		tsc_per_ms[0] = div64u(cpu_get_freq(0), 1000);
	}

	return 0;
}

PUBLIC void arch_stop_local_timer(void)
{
#ifdef CONFIG_APIC
	if (lapic_addr) {
		lapic_stop_timer();
	} else
#endif
	{
		stop_8253A_timer();
	}
}

PUBLIC int arch_register_local_timer_handler(const irq_handler_t handler)
{
#ifdef CONFIG_APIC
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

PUBLIC void cycles_accounting_init(void)
{
	read_tsc_64(&tsc_ctr_switch);
}

PUBLIC void context_stop(struct proc * p)
{
	u64_t tsc, tsc_delta;

	read_tsc_64(&tsc);
	tsc_delta = sub64(tsc, tsc_ctr_switch);
	p->p_cycles = add64(p->p_cycles, tsc_delta);
	tsc_ctr_switch = tsc;

	/*
	 * deduct the just consumed cpu cycles from the cpu time left for this
	 * process during its current quantum. Skip IDLE and other pseudo kernel
	 * tasks
	 */
	if (p->p_endpoint >= 0) {
#if DEBUG_RACE
		make_zero64(p->p_cpu_time_left);
#else
		/* if (tsc_delta < p->p_cpu_time_left) in 64bit */
		if (tsc_delta.hi < p->p_cpu_time_left.hi ||
				(tsc_delta.hi == p->p_cpu_time_left.hi &&
				 tsc_delta.lo < p->p_cpu_time_left.lo))
			p->p_cpu_time_left = sub64(p->p_cpu_time_left, tsc_delta);
		else {
			make_zero64(p->p_cpu_time_left);
		}
#endif
	}
}

PUBLIC void context_stop_idle(void)
{
	context_stop(proc_addr(IDLE));
}

PUBLIC u64_t ms_2_cpu_time(unsigned ms)
{
	return mul64u(tsc_per_ms[cpuid], ms);
}
