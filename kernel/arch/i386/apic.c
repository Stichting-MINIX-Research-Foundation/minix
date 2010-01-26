/*
 * APIC handling routines. APIC is a requirement for SMP
 */
#include "../../kernel.h"

#include <unistd.h>
#include <minix/portio.h>

#include <minix/syslib.h>

#include "../../proc.h"
#include "../..//glo.h"
#include "proto.h"

#include <minix/u64.h>

#include "apic.h"
#include "apic_asm.h"
#include "../../clock.h"
#include "glo.h"

#ifdef CONFIG_WATCHDOG
#include "../../watchdog.h"
#endif

#define IA32_APIC_BASE	0x1b
#define IA32_APIC_BASE_ENABLE_BIT	11

/* currently only 2 interrupt priority levels are used */
#define SPL0				0x0
#define	SPLHI				0xF

/*
 * to make APIC work if SMP is not configured, we need to set the maximal number
 * of CPUS to 1, cpuid to return 0 and the current cpu is always BSP
 */
#define CONFIG_MAX_CPUS 1
#define cpu_is_bsp(x) 1

#define lapic_write_icr1(val)	lapic_write(LAPIC_ICR1, val)
#define lapic_write_icr2(val)	lapic_write(LAPIC_ICR2, val)

#define lapic_read_icr1(x)	lapic_read(LAPIC_ICR1)
#define lapic_read_icr2(x)	lapic_read(LAPIC_ICR2)

#define VERBOSE_APIC(x) x

PUBLIC int reboot_type;
PUBLIC int ioapic_enabled;
PUBLIC u32_t ioapic_id_mask[8], lapic_id_mask[8];
PUBLIC u32_t lapic_addr_vaddr;
PUBLIC u32_t lapic_addr;
PUBLIC u32_t lapic_eoi_addr;
PUBLIC u32_t lapic_taskpri_addr;
PUBLIC int bsp_lapic_id;

PRIVATE volatile int probe_ticks;
PRIVATE	u64_t tsc0, tsc1;
PRIVATE	u32_t lapic_tctr0, lapic_tctr1;

u8_t apicid2cpuid[MAX_NR_APICIDS+1];
unsigned apic_imcrp;
unsigned nioapics;
unsigned nbuses;
unsigned nintrs;
unsigned nlints;

/*
 * FIXME this should be a cpulocal variable but there are some problems with
 * arch specific cpulocals. As this variable is write-once-read-only it is ok to
 * have at as an array until we resolve the cpulocals properly
 */
PRIVATE u32_t lapic_bus_freq[CONFIG_MAX_CPUS];
/* the probe period will be roughly 100ms */
#define PROBE_TICKS	(system_hz / 10)

PRIVATE u32_t pci_config_intr_data;
PRIVATE u32_t ioapic_extint_assigned = 0;
PRIVATE int lapic_extint_assigned = 0;

PRIVATE int calib_clk_handler(irq_hook_t * hook)
{
	u32_t tcrt;
	u64_t tsc;

	probe_ticks++;
	read_tsc_64(&tsc);
	tcrt = lapic_read(LAPIC_TIMER_CCR);


	if (probe_ticks == 1) {
		lapic_tctr0 = tcrt;
		tsc0 = tsc;
	}
	else if (probe_ticks == PROBE_TICKS) {
		lapic_tctr1 = tcrt;
		tsc1 = tsc;
	}

	return 1;
}

PUBLIC void apic_calibrate_clocks(void)
{
	u32_t lvtt, val, lapic_delta;
	u64_t tsc_delta;
	u32_t cpu_freq;

	irq_hook_t calib_clk;

	BOOT_VERBOSE(kprintf("Calibrating clock\n"));
	/*
	 * Set Initial count register to the highest value so it does not
	 * underflow during the testing period
	 * */
	val = 0xffffffff;
	lapic_write (LAPIC_TIMER_ICR, val);

	/* Set Current count register */
	val = 0;
	lapic_write (LAPIC_TIMER_CCR, val);

	lvtt = lapic_read(LAPIC_TIMER_DCR) & ~0x0b;
	 /* Set Divide configuration register to 1 */
	lvtt = APIC_TDCR_1;
	lapic_write(LAPIC_TIMER_DCR, lvtt);

	/*
	 * mask the APIC timer interrupt in the LVT Timer Register so that we
	 * don't get an interrupt upon underflow which we don't know how to
	 * handle right know. If underflow happens, the system will not continue
	 * as something is wrong with the clock IRQ 0 and we cannot calibrate
	 * the clock which mean that we cannot run processes
	 */
	lvtt = lapic_read (LAPIC_LVTTR);
	lvtt |= APIC_LVTT_MASK;
	lapic_write (LAPIC_LVTTR, lvtt);

	/* set the probe, we use the legacy timer, IRQ 0 */
	put_irq_handler(&calib_clk, CLOCK_IRQ, calib_clk_handler);

	/* set the PIC timer to get some time */
	intr_enable();
	init_8253A_timer(system_hz);

	/* loop for some time to get a sample */
	while(probe_ticks < PROBE_TICKS);

	intr_disable();
	stop_8253A_timer();

	/* remove the probe */
	rm_irq_handler(&calib_clk);

	lapic_delta = lapic_tctr0 - lapic_tctr1;
	tsc_delta = sub64(tsc1, tsc0);

	lapic_bus_freq[cpuid] = system_hz * lapic_delta / (PROBE_TICKS - 1);
	BOOT_VERBOSE(kprintf("APIC bus freq %lu MHz\n",
				lapic_bus_freq[cpuid] / 1000000));
	cpu_freq = div64u(tsc_delta, PROBE_TICKS - 1) * system_hz;
	BOOT_VERBOSE(kprintf("CPU %d freq %lu MHz\n", cpuid,
				cpu_freq / 1000000));

	cpu_set_freq(cpuid, cpu_freq);
}

PRIVATE void lapic_set_timer_one_shot(u32_t value)
{
	/* sleep in micro seconds */
	u32_t lvtt;
	u32_t ticks_per_us;
	u8_t cpu = cpuid;

	ticks_per_us = lapic_bus_freq[cpu] / 1000000;

	/* calculate divisor and count from value */
	lvtt = APIC_TDCR_1;
	lapic_write(LAPIC_TIMER_DCR, lvtt);

	/* configure timer as one-shot */
	lvtt = APIC_TIMER_INT_VECTOR;
	lapic_write(LAPIC_LVTTR, lvtt);

	lapic_write(LAPIC_TIMER_ICR, value * ticks_per_us);
}

PUBLIC void lapic_set_timer_periodic(unsigned freq)
{
	/* sleep in micro seconds */
	u32_t lvtt;
	u32_t lapic_ticks_per_clock_tick;
	u8_t cpu = cpuid;

	lapic_ticks_per_clock_tick = lapic_bus_freq[cpu] / freq;

	lvtt = APIC_TDCR_1;
	lapic_write(LAPIC_TIMER_DCR, lvtt);

	/* configure timer as periodic */
	lvtt = APIC_LVTT_TM | APIC_TIMER_INT_VECTOR;
	lapic_write(LAPIC_LVTTR, lvtt);

	lapic_write(LAPIC_TIMER_ICR, lapic_ticks_per_clock_tick);
}

PUBLIC void lapic_stop_timer(void)
{
	u32_t lvtt;
	lvtt = lapic_read(LAPIC_LVTTR);
	lapic_write(LAPIC_LVTTR, lvtt | APIC_LVTT_MASK);
}

PUBLIC void lapic_microsec_sleep(unsigned count)
{
	lapic_set_timer_one_shot(count);
	while (lapic_read (LAPIC_TIMER_CCR));
}

PUBLIC  u32_t lapic_errstatus (void)
{
	lapic_write(LAPIC_ESR, 0);
	return lapic_read(LAPIC_ESR);
}

PUBLIC void lapic_disable(void)
{
	/* Disable current APIC and close interrupts from PIC */
	u32_t val;

	if (!lapic_addr)
		return;
	{
		/* leave it enabled if imcr is not set */
		val = lapic_read(LAPIC_LINT0);
		val &= ~(APIC_ICR_DM_MASK|APIC_ICR_INT_MASK);
		val |= APIC_ICR_DM_EXTINT; /* ExtINT at LINT0 */
		lapic_write (LAPIC_LINT0, val);
		return;
	}

	val = lapic_read(LAPIC_LINT0) & 0xFFFE58FF;
	val |= APIC_ICR_INT_MASK;
	lapic_write (LAPIC_LINT0, val);

	val = lapic_read(LAPIC_LINT1) & 0xFFFE58FF;
	val |= APIC_ICR_INT_MASK;
	lapic_write (LAPIC_LINT1, val);

	val = lapic_read(LAPIC_SIVR) & 0xFFFFFF00;
	val &= ~APIC_ENABLE;
	lapic_write(LAPIC_SIVR, val);
}

PRIVATE void lapic_enable_no_lints(void)
{
	u32_t val;

	val = lapic_read(LAPIC_LINT0);
	lapic_extint_assigned =	(val & APIC_ICR_DM_MASK) == APIC_ICR_DM_EXTINT;
	val &= ~(APIC_ICR_DM_MASK|APIC_ICR_INT_MASK);

	if (!ioapic_enabled && cpu_is_bsp(cpuid))
		val |= (APIC_ICR_DM_EXTINT); /* ExtINT at LINT0 */
	else
		val |= (APIC_ICR_DM_EXTINT|APIC_ICR_INT_MASK); /* Masked ExtINT at LINT0 */

	lapic_write (LAPIC_LINT0, val);

	val = lapic_read(LAPIC_LINT1);
	val &= ~(APIC_ICR_DM_MASK|APIC_ICR_INT_MASK);

	if (!ioapic_enabled && cpu_is_bsp(cpuid))
		val |= APIC_ICR_DM_NMI;
	else
		val |= (APIC_ICR_DM_NMI | APIC_ICR_INT_MASK); /* NMI at LINT1 */
	lapic_write (LAPIC_LINT1, val);
}

PRIVATE int lapic_enable_in_msr(void)
{
	u64_t msr;
	u32_t addr;

	ia32_msr_read(IA32_APIC_BASE, &msr.hi, &msr.lo);

	/*
	 * FIXME if the location is different (unlikely) then the one we expect,
	 * update it
	 */
	addr = (msr.lo >> 12) | ((msr.hi & 0xf) << 20);
	if (phys2vir(addr) != (lapic_addr >> 12)) {
		if (msr.hi & 0xf) {
			kprintf("ERROR : APIC address needs more then 32 bits\n");
			return 0;
		}
		lapic_addr = phys2vir(msr.lo & ~((1 << 12) - 1));
	}

	msr.lo |= (1 << IA32_APIC_BASE_ENABLE_BIT);
	ia32_msr_write(IA32_APIC_BASE, msr.hi, msr.lo);

	return 1;
}

PUBLIC int lapic_enable(void)
{
	u32_t val, nlvt;
	unsigned cpu = cpuid;

	if (!lapic_addr)
		return 0;

	cpu_has_tsc = _cpufeature(_CPUF_I386_TSC);
	if (!cpu_has_tsc) {
		kprintf("CPU lacks timestamp counter, "
			"cannot calibrate LAPIC timer\n");
		return 0;
	}

	if (!lapic_enable_in_msr())
		return 0;

	lapic_eoi_addr = LAPIC_EOI;
	/* clear error state register. */
	val = lapic_errstatus ();

	/* Enable Local APIC and set the spurious vector to 0xff. */

	val = lapic_read(LAPIC_SIVR) & 0xFFFFFF00;
	val |= APIC_ENABLE | APIC_SPURIOUS_INT_VECTOR;
	val &= ~APIC_FOCUS_DISABLED;
	lapic_write(LAPIC_SIVR, val);
	lapic_read(LAPIC_SIVR);

	*((u32_t *)lapic_eoi_addr) = 0;

	cpu = cpuid;

	/* Program Logical Destination Register. */
	val = lapic_read(LAPIC_LDR) & ~0xFF000000;
	val |= (cpu & 0xFF) << 24;
	lapic_write(LAPIC_LDR, val);

	/* Program Destination Format Register for Flat mode. */
	val = lapic_read(LAPIC_DFR) | 0xF0000000;
	lapic_write (LAPIC_DFR, val);

	if (nlints == 0) {
		lapic_enable_no_lints();
	}

	val = lapic_read (LAPIC_LVTER) & 0xFFFFFF00;
	lapic_write (LAPIC_LVTER, val);

	nlvt = (lapic_read(LAPIC_VERSION)>>16) & 0xFF;

	if(nlvt >= 4) {
		val = lapic_read(LAPIC_LVTTMR);
		lapic_write(LAPIC_LVTTMR, val | APIC_ICR_INT_MASK);
	}

	if(nlvt >= 5) {
		val = lapic_read(LAPIC_LVTPCR);
		lapic_write(LAPIC_LVTPCR, val | APIC_ICR_INT_MASK);
	}

	/* setup TPR to allow all interrupts. */
	val = lapic_read (LAPIC_TPR);
	/* accept all interrupts */
	lapic_write (LAPIC_TPR, val & ~0xFF);

	lapic_read (LAPIC_SIVR);
	*((u32_t *)lapic_eoi_addr) = 0;

	apic_calibrate_clocks();
	BOOT_VERBOSE(kprintf("APIC timer calibrated\n"));

	return 1;
}

PRIVATE void apic_spurios_intr(void)
{
	kprintf("WARNING spurious interrupt\n");
	for(;;);
}

PRIVATE struct gate_table_s gate_table_ioapic[] = {
	{ apic_hwint00, VECTOR( 0), INTR_PRIVILEGE },
	{ apic_hwint01, VECTOR( 1), INTR_PRIVILEGE },
	{ apic_hwint02, VECTOR( 2), INTR_PRIVILEGE },
	{ apic_hwint03, VECTOR( 3), INTR_PRIVILEGE },
	{ apic_hwint04, VECTOR( 4), INTR_PRIVILEGE },
	{ apic_hwint05, VECTOR( 5), INTR_PRIVILEGE },
	{ apic_hwint06, VECTOR( 6), INTR_PRIVILEGE },
	{ apic_hwint07, VECTOR( 7), INTR_PRIVILEGE },
	{ apic_hwint08, VECTOR( 8), INTR_PRIVILEGE },
	{ apic_hwint09, VECTOR( 9), INTR_PRIVILEGE },
	{ apic_hwint10, VECTOR(10), INTR_PRIVILEGE },
	{ apic_hwint11, VECTOR(11), INTR_PRIVILEGE },
	{ apic_hwint12, VECTOR(12), INTR_PRIVILEGE },
	{ apic_hwint13, VECTOR(13), INTR_PRIVILEGE },
	{ apic_hwint14, VECTOR(14), INTR_PRIVILEGE },
	{ apic_hwint15, VECTOR(15), INTR_PRIVILEGE },
	{ apic_spurios_intr, APIC_SPURIOUS_INT_VECTOR, INTR_PRIVILEGE },
	{ NULL, 0, 0}
};

PRIVATE struct gate_table_s gate_table_common[] = {
	{ syscall_entry, SYS386_VECTOR, USER_PRIVILEGE },
	{ level0_call, LEVEL0_VECTOR, TASK_PRIVILEGE },
	{ NULL, 0, 0}
};

#ifdef CONFIG_APIC_DEBUG
PRIVATE void lapic_set_dummy_handlers(void)
{
	char * handler;
	int vect = 32;

	handler = &lapic_intr_dummy_handles_start;
	handler += vect * LAPIC_INTR_DUMMY_HANDLER_SIZE;
	for(; handler < &lapic_intr_dummy_handles_end;
			handler += LAPIC_INTR_DUMMY_HANDLER_SIZE) {
		int_gate(vect++, (vir_bytes) handler,
				PRESENT | INT_GATE_TYPE |
				(INTR_PRIVILEGE << DPL_SHIFT));
	}
}
#endif

/* Build descriptors for interrupt gates in IDT. */
PUBLIC void apic_idt_init(int reset)
{
	/* Set up idt tables for smp mode.
	 */
	vir_bytes local_timer_intr_handler;

	if (reset) {
		idt_copy_vectors(gate_table_pic);
		idt_copy_vectors(gate_table_common);
		return;
	}

#ifdef CONFIG_APIC_DEBUG
	if (cpu_is_bsp(cpuid))
		kprintf("APIC debugging is enabled\n");
	lapic_set_dummy_handlers();
#endif

	/* Build descriptors for interrupt gates in IDT. */
	if (ioapic_enabled)
		idt_copy_vectors(gate_table_ioapic);
	else
		idt_copy_vectors(gate_table_pic);

	idt_copy_vectors(gate_table_common);

	/* configure the timer interupt handler */
	if (cpu_is_bsp(cpuid)) {
		local_timer_intr_handler = (vir_bytes) lapic_bsp_timer_int_handler;
		BOOT_VERBOSE(kprintf("Initiating BSP timer handler\n"));
	} else {
		local_timer_intr_handler = (vir_bytes) lapic_ap_timer_int_handler;
		BOOT_VERBOSE(kprintf("Initiating AP timer handler\n"));
	}

	/* register the timer interrupt handler for this CPU */
	int_gate(APIC_TIMER_INT_VECTOR, (vir_bytes) local_timer_intr_handler,
		PRESENT | INT_GATE_TYPE | (INTR_PRIVILEGE << DPL_SHIFT));
}

PUBLIC int apic_single_cpu_init(void)
{
	if (!cpu_feature_apic_on_chip())
		return 0;

	lapic_addr = phys2vir(LOCAL_APIC_DEF_ADDR);
	ioapic_enabled = 0;

	if (!lapic_enable()) {
		lapic_addr = 0x0;
		return 0;
	}

	apic_idt_init(0); /* Not a reset ! */
	idt_reload();
	return 1;
}
