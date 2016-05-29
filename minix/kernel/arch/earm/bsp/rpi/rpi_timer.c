#include "kernel/kernel.h"
#include "kernel/clock.h"
#include <sys/types.h>
#include <machine/cpu.h>
#include <minix/board.h>
#include <minix/mmio.h>
#include <assert.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include "arch_proto.h"
#include "bsp_timer.h"
#include "rpi_timer_registers.h"
#include "rpi_intr_registers.h"
#include "bsp_intr.h"

#include "cpufunc_timer.h"

static irq_hook_t arm_timer_hook;

struct arm_timer
{
	int irq_nr;
	u32_t freq;
};

static struct arm_timer arm_timer = {
	.irq_nr = RPI2_IRQ_ARMTIMER,
	.freq = 0,
};

static kern_phys_map stc_timer_phys_map;

int
bsp_register_timer_handler(const irq_handler_t handler)
{
	/* Initialize the CLOCK's interrupt hook. */
	arm_timer_hook.proc_nr_e = NONE;
	arm_timer_hook.irq = arm_timer.irq_nr;

	put_irq_handler(&arm_timer_hook, arm_timer.irq_nr, handler);

	/* Prepare next firing of timer */
	bsp_timer_int_handler();

	/* only unmask interrupts after registering */
	bsp_irq_unmask(arm_timer.irq_nr);

	return 0;
}

/* callback for when the free running clock gets mapped */
int
kern_phys_fr_user_mapped(vir_bytes id, phys_bytes address)
{
	return 0;
}

void
bsp_timer_init(unsigned freq)
{
	arm_timer.freq = freq;
}

void
bsp_timer_stop()
{
	bsp_irq_mask(arm_timer.irq_nr);
}

void
bsp_timer_int_handler()
{
	/* Arm next timer countdown and enable timer */
	write_cntv_cval(-1);
	write_cntv_tval(read_cntfrq() / arm_timer.freq);
	write_cntv_ctl(ARMTIMER_ENABLE);
}

/* Use the free running clock as TSC */
void
read_tsc_64(u64_t * t)
{
	*t = read_cntv_cval();
}
