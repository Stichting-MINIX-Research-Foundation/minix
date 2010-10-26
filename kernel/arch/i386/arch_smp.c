/* This file contains essentially the MP handling code of the Minix kernel.
 *
 * Changes:
 *   Apr 1,  2008     Added SMP support.
 */

#define _SMP

#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "arch_proto.h"
#include "kernel/glo.h"
#include <unistd.h>
#include <machine/cmos.h>
#include <machine/bios.h>
#include <minix/portio.h>

#include "kernel/spinlock.h"
#include "kernel/smp.h"
#include "apic.h"
#include "acpi.h"

#include "glo.h"

_PROTOTYPE(void trampoline, (void));

/*
 * arguments for trampoline. We need to pass the logical cpu id, gdt and idt.
 * They have to be in location which is reachable using absolute addressing in
 * 16-bit mode
 */
extern volatile u32_t __ap_id;
extern volatile struct segdesc_s __ap_gdt, __ap_idt;
extern void * __trampoline_end;

extern u32_t busclock[CONFIG_MAX_CPUS];
extern int panicking;

static int volatile ap_cpu_ready;
static int volatile cpu_down;

/* there can be at most 255 local APIC ids, each fits in 8 bits */
PRIVATE unsigned char apicid2cpuid[255];
PUBLIC unsigned char cpuid2apicid[CONFIG_MAX_CPUS];

SPINLOCK_DEFINE(smp_cpu_lock)
SPINLOCK_DEFINE(dispq_lock)

FORWARD _PROTOTYPE(void smp_init_vars, (void));
FORWARD _PROTOTYPE(void smp_reinit_vars, (void));

/*
 * copies the 16-bit AP trampoline code to the first 1M of memory
 */
PRIVATE phys_bytes copy_trampoline(void)
{
	char * s, *end;
	phys_bytes tramp_base;
	unsigned tramp_size;

	tramp_size = (unsigned) &__trampoline_end - (unsigned)&trampoline;
	s = env_get("memory");
	s = (char *) get_value(params_buffer, "memory");
	if (!s)
		return 0;
	
	while (*s != 0) {
		phys_bytes base = 0xfffffff;
		unsigned size;
		/* Read fresh base and expect colon as next char. */ 
		base = strtoul(s, &end, 0x10);		/* get number */
		if (end != s && *end == ':')
			s = ++end;	/* skip ':' */ 
		else
			*s=0;

		/* Read fresh size and expect comma or assume end. */ 
		size = strtoul(s, &end, 0x10);		/* get number */
		if (end != s && *end == ',')
			s = ++end;	/* skip ',' */

		tramp_base = (base + 0xfff) & ~(0xfff);
		/* the address must be less than 1M */
		if (tramp_base >= (1 << 20))
			continue;
		if (size - (tramp_base - base) < tramp_size)
			continue;
		break;
	}

	phys_copy(vir2phys(trampoline), tramp_base, tramp_size);

	return tramp_base;
}

PRIVATE void smp_start_aps(void)
{
	/* 
	 * Find an address and align it to a 4k boundary.
	 */
	unsigned cpu;
	u32_t biosresetvector;
	phys_bytes trampoline_base, __ap_id_phys;

	/* TODO hack around the alignment problem */

	phys_copy (0x467, vir2phys(&biosresetvector), sizeof(u32_t));

	/* set the bios shutdown code to 0xA */
	outb(RTC_INDEX, 0xF);
	outb(RTC_IO, 0xA);

	/* prepare gdt and idt for the new cpus */
	__ap_gdt = gdt[GDT_INDEX];
	__ap_idt = gdt[IDT_INDEX];

	if (!(trampoline_base = copy_trampoline())) {
		printf("Copying trampoline code failed, cannot boot SMP\n");
		ncpus = 1;
	}
	__ap_id_phys = trampoline_base +
		(phys_bytes) &__ap_id - (phys_bytes)&trampoline;

	/* setup the warm reset vector */
	phys_copy(vir2phys(&trampoline_base), 0x467, sizeof(u32_t));

	/* okay, we're ready to go.  boot all of the ap's now.  we loop through
	 * using the processor's apic id values.
	 */
	for (cpu = 0; cpu < ncpus; cpu++) {
		ap_cpu_ready = -1;
		/* Don't send INIT/SIPI to boot cpu.  */
		if((apicid() == cpuid2apicid[cpu]) && 
				(apicid() == bsp_lapic_id)) {
			continue;
		}

		__ap_id = cpu;
		phys_copy(vir2phys(__ap_id), __ap_id_phys, sizeof(__ap_id));
		mfence();
		if (apic_send_init_ipi(cpu, trampoline_base) ||
				apic_send_startup_ipi(cpu, trampoline_base)) {
			printf("WARNING cannot boot cpu %d\n", cpu);
			continue;
		}

		/* wait for 5 secs for the processors to boot */
		lapic_set_timer_one_shot(5000000); 

		while (lapic_read(LAPIC_TIMER_CCR)) {
			if (ap_cpu_ready == cpu) {
				cpu_set_flag(cpu, CPU_IS_READY);
				break;
			}
		}
		if (ap_cpu_ready == -1) {
			printf("WARNING : CPU %d didn't boot\n", cpu);
		}
	}

	phys_copy(vir2phys(&biosresetvector),(phys_bytes)0x467,sizeof(u32_t));

	outb(RTC_INDEX, 0xF);
	outb(RTC_IO, 0);

	bsp_finish_booting();
	NOT_REACHABLE;
}

PUBLIC void smp_halt_cpu (void) 
{
	NOT_IMPLEMENTED;
}

PUBLIC void smp_shutdown_aps(void)
{
	unsigned cpu;
	unsigned aid = apicid();
	unsigned local_cpu = cpuid;
	
	if (ncpus == 1)
		goto exit_shutdown_aps;
	
	/* we must let the other cpus enter the kernel mode */
	BKL_UNLOCK();

	for (cpu = 0; cpu < ncpus; cpu++) {
		if (cpu == cpuid)
			continue;
		if (!cpu_test_flag(cpu, CPU_IS_READY)) {
			printf("CPU %d didn't boot\n", cpu);
			continue;
		}

		cpu_down = -1;
		barrier();
		apic_send_ipi(APIC_SMP_CPU_HALT_VECTOR, cpu, APIC_IPI_DEST);
		/* wait for the cpu to be down */
		while (cpu_down != cpu);
		printf("CPU %d is down\n", cpu);
		cpu_clear_flag(cpu, CPU_IS_READY);
	}

exit_shutdown_aps:
	ioapic_disable_all();

	lapic_disable();

	ncpus = 1; /* hopefully !!! */
	lapic_addr = lapic_eoi_addr = 0;
	return;
}

PRIVATE void ap_finish_booting(void)
{
	unsigned cpu = cpuid;

	/* inform the world of our presence. */
	ap_cpu_ready = cpu;

	while(!i386_paging_enabled)
		arch_pause();

	/*
	 * Finish processor initialisation.  CPUs must be excluded from running.
	 * lapic timer calibration locks and unlocks the BKL because of the
	 * nested interrupts used for calibration. Therefore BKL is not good
	 * enough, the boot_lock must be held.
	 */
	spinlock_lock(&boot_lock);
	BKL_LOCK();

	/*
	 * we must load some page tables befre we turn paging on. As VM is
	 * always present we use those
	 */
	segmentation2paging(proc_addr(VM_PROC_NR));
	
	printf("CPU %d paging is on\n", cpu);

	cpu_identify();

	lapic_enable(cpu);
	fpu_init();

	if (app_cpu_init_timer(system_hz)) {
		panic("FATAL : failed to initialize timer interrupts CPU %d, "
				"cannot continue without any clock source!", cpu);
	}

	/* FIXME assign CPU local idle structure */
	get_cpulocal_var(proc_ptr) = get_cpulocal_var_ptr(idle_proc);
	get_cpulocal_var(bill_ptr) = get_cpulocal_var_ptr(idle_proc);

	ap_boot_finished(cpu);
	spinlock_unlock(&boot_lock);

	switch_to_user();
	NOT_REACHABLE;
}

PUBLIC void smp_ap_boot(void)
{
	switch_k_stack((char *)get_k_stack_top(__ap_id) -
			X86_STACK_TOP_RESERVED, ap_finish_booting);
}

PRIVATE void smp_reinit_vars(void)
{
	int i;
	lapic_addr = lapic_eoi_addr = 0;
	ioapic_enabled = 0;

	ncpus = 1;
}

PRIVATE void tss_init_all(void)
{
	unsigned cpu;

	for(cpu = 0; cpu < ncpus ; cpu++)
		tss_init(cpu, get_k_stack_top(cpu)); 
}

PRIVATE int discover_cpus(void)
{
	struct acpi_madt_lapic * cpu;

	while (ncpus < CONFIG_MAX_CPUS && (cpu = acpi_get_lapic_next())) {
		apicid2cpuid[cpu->apic_id] = ncpus;
		cpuid2apicid[ncpus] = cpu->apic_id;
		printf("CPU %3d local APIC id %3d\n", ncpus, cpu->apic_id);
		ncpus++;
	}

	return ncpus;
}

PUBLIC void smp_init (void)
{
	/* read the MP configuration */
	if (!discover_cpus()) {
		ncpus = 1;
		goto uniproc_fallback;
	}

	lapic_addr = phys2vir(LOCAL_APIC_DEF_ADDR);
	ioapic_enabled = 0;

	tss_init_all();

	/* 
	 * we still run on the boot stack and we cannot use cpuid as its value
	 * wasn't set yet. apicid2cpuid initialized in mps_init()
	 */
	bsp_cpu_id = apicid2cpuid[apicid()];

	if (!lapic_enable(bsp_cpu_id)) {
		printf("ERROR : failed to initialize BSP Local APIC\n");
		goto uniproc_fallback;
	}

	bsp_lapic_id = apicid();
	
	acpi_init();

	if (!detect_ioapics()) {
		lapic_disable();
		lapic_addr = 0x0;
		goto uniproc_fallback;
	}

	ioapic_enable_all();
	
	if (ioapic_enabled)
		machine.apic_enabled = 1;

	/* set smp idt entries. */ 
	apic_idt_init(0); /* Not a reset ! */
	idt_reload();

	BOOT_VERBOSE(printf("SMP initialized\n"));

	switch_k_stack((char *)get_k_stack_top(bsp_cpu_id) -
			X86_STACK_TOP_RESERVED, smp_start_aps);
	
	return;

uniproc_fallback:
	apic_idt_init(1); /* Reset to PIC idt ! */
	idt_reload();
	smp_reinit_vars (); /* revert to a single proc system. */
	intr_init (INTS_MINIX, 0); /* no auto eoi */
	printf("WARNING : SMP initialization failed\n");
}
	
PUBLIC void arch_smp_halt_cpu(void)
{
	/* say that we are down */
	cpu_down = cpuid;
	barrier();
	/* unlock the BKL and don't continue */
	BKL_UNLOCK();
	for(;;);
}

PUBLIC void arch_send_smp_schedule_ipi(unsigned cpu)
{
	apic_send_ipi(APIC_SMP_SCHED_PROC_VECTOR, cpu, APIC_IPI_DEST);
}
