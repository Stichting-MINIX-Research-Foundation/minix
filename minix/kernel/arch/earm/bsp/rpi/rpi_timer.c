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
	vir_bytes st_base;
	int size;
	int irq_nr;
	u32_t freq;

	int st_workaround;
};

static struct arm_timer arm_timer;

static kern_phys_map st_timer_phys_map;
static kern_phys_map st_timer_user_phys_map;

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
kern_phys_st_user_mapped(vir_bytes id, phys_bytes address)
{
	arm_frclock.tcrr = address + RPI_ST_CLO;
	arm_frclock.hz = RPI_ST_FREQ;

	return 0;
}

void
bsp_timer_init(unsigned freq)
{
	arm_timer.freq = freq;

	if (BOARD_IS_RPI_2_B(machine.board_id) ||
	    BOARD_IS_RPI_3_B(machine.board_id)) {
		arm_timer.st_base = RPI_ST_BASE;
		arm_timer.size = 0x1000;	/* 4K */
	} else {
		panic
		    ("Can not do the timer setup. machine (0x%08x) is unknown\n",
		    machine.board_id);
	}

	kern_phys_map_ptr(arm_timer.st_base, arm_timer.size,
	    VMMF_UNCACHED | VMMF_WRITE,
	    &st_timer_phys_map, (vir_bytes) & arm_timer.st_base);

	/* Check if we need to workaround QEMU's lack of ST support */
	if (mmio_read(arm_timer.st_base + RPI_ST_CLO) == 0) {
		/*
		 * Uh oh. We'll have to rely on the ARM processor's internal
		 * timers. Not good.
		 */
		printf("Working around lack of system timer support - please fix\n");
		arm_timer.st_workaround = 1;
		arm_timer.irq_nr = RPI2_IRQ_ARMTIMER;
	}
	else {
		arm_timer.st_workaround = 0;
		arm_timer.irq_nr = RPI_IRQ_ST_C3;

		/* the timer is also mapped in user space hence the this */
		/* second mapping and callback to set kerninfo frclock_tcrr */
		kern_req_phys_map(arm_timer.st_base, ARM_PAGE_SIZE,
		    VMMF_UNCACHED | VMMF_USER,
		    &st_timer_user_phys_map, kern_phys_st_user_mapped, 0);
	}
}

void
bsp_timer_stop()
{
	bsp_irq_mask(arm_timer.irq_nr);
}

void
bsp_timer_int_handler()
{
	if (arm_timer.st_workaround) {
		/* Arm next timer countdown and enable timer */
		write_cntv_cval(-1);
		write_cntv_tval(read_cntfrq() / arm_timer.freq);
		write_cntv_ctl(ARMTIMER_ENABLE);
	}
	else {
		/* Set next timer alarm and enable timer */
		u32_t next_alarm = mmio_read(arm_timer.st_base + RPI_ST_CLO);
		next_alarm += RPI_ST_FREQ / arm_timer.freq;

		mmio_write(arm_timer.st_base + RPI_ST_C3, next_alarm);
		mmio_write(arm_timer.st_base + RPI_ST_CS, RPI_ST_M3);
	}
}

/* Use the free running clock as TSC */
void
read_tsc_64(u64_t * t)
{
	if (arm_timer.st_workaround)
		*t = read_cntv_cval();
	else {
		*t = mmio_read(arm_timer.st_base + RPI_ST_CHI);
		*t = (*t << 32) | mmio_read(arm_timer.st_base + RPI_ST_CLO);
	}
}
