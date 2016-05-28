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
#include "omap_timer_registers.h"
#include "omap_intr_registers.h"
#include "bsp_intr.h"

/* interrupt handler hook */
static irq_hook_t omap3_timer_hook;
static u64_t high_frc;

struct omap_timer_registers;

struct omap_timer
{
	vir_bytes base;
	int irq_nr;
	struct omap_timer_registers *regs;
};

struct omap_timer_registers
{
	vir_bytes TIDR;
	vir_bytes TIOCP_CFG;
	vir_bytes TISTAT;
	vir_bytes TISR;
	vir_bytes TIER;
	vir_bytes TWER;
	vir_bytes TCLR;
	vir_bytes TCRR;
	vir_bytes TLDR;
	vir_bytes TTGR;
	vir_bytes TWPS;
	vir_bytes TMAR;
	vir_bytes TCAR1;
	vir_bytes TSICR;
	vir_bytes TCAR2;
	vir_bytes TPIR;
	vir_bytes TNIR;
	vir_bytes TCVR;
	vir_bytes TOCR;
	vir_bytes TOWR;

};

static struct omap_timer_registers regs_v1 = {
	.TIDR = OMAP3_TIMER_TIDR,
	.TIOCP_CFG = OMAP3_TIMER_TIOCP_CFG,
	.TISTAT = OMAP3_TIMER_TISTAT,
	.TISR = OMAP3_TIMER_TISR,
	.TIER = OMAP3_TIMER_TIER,
	.TWER = OMAP3_TIMER_TWER,
	.TCLR = OMAP3_TIMER_TCLR,
	.TCRR = OMAP3_TIMER_TCRR,
	.TLDR = OMAP3_TIMER_TLDR,
	.TTGR = OMAP3_TIMER_TTGR,
	.TWPS = OMAP3_TIMER_TWPS,
	.TMAR = OMAP3_TIMER_TMAR,
	.TCAR1 = OMAP3_TIMER_TCAR1,
	.TSICR = OMAP3_TIMER_TSICR,
	.TCAR2 = OMAP3_TIMER_TCAR2,
	.TPIR = OMAP3_TIMER_TPIR,
	.TNIR = OMAP3_TIMER_TNIR,
	.TCVR = OMAP3_TIMER_TCVR,
	.TOCR = OMAP3_TIMER_TOCR,
	.TOWR = OMAP3_TIMER_TOWR,
};

/* AM335X has a different ip block for the non 
 1ms timers */
static struct omap_timer_registers regs_v2 = {
	.TIDR = AM335X_TIMER_TIDR,
	.TIOCP_CFG = AM335X_TIMER_TIOCP_CFG,
	.TISTAT = AM335X_TIMER_IRQSTATUS_RAW,
	.TISR = AM335X_TIMER_IRQSTATUS,
	.TIER = AM335X_TIMER_IRQENABLE_SET,
	.TWER = AM335X_TIMER_IRQWAKEEN,
	.TCLR = AM335X_TIMER_TCLR,
	.TCRR = AM335X_TIMER_TCRR,
	.TLDR = AM335X_TIMER_TLDR,
	.TTGR = AM335X_TIMER_TTGR,
	.TWPS = AM335X_TIMER_TWPS,
	.TMAR = AM335X_TIMER_TMAR,
	.TCAR1 = AM335X_TIMER_TCAR1,
	.TSICR = AM335X_TIMER_TSICR,
	.TCAR2 = AM335X_TIMER_TCAR2,
	.TPIR = -1,		/* UNDEF */
	.TNIR = -1,		/* UNDEF */
	.TCVR = -1,		/* UNDEF */
	.TOCR = -1,		/* UNDEF */
	.TOWR = -1		/* UNDEF */
};

static struct omap_timer dm37xx_timer = {
	.base = OMAP3_GPTIMER1_BASE,
	.irq_nr = OMAP3_GPT1_IRQ,
	.regs = &regs_v1
};

/* free running timer */
static struct omap_timer dm37xx_fr_timer = {
	.base = OMAP3_GPTIMER10_BASE,
	.irq_nr = OMAP3_GPT10_IRQ,
	.regs = &regs_v1
};

/* normal timer */
static struct omap_timer am335x_timer = {
	.base = AM335X_DMTIMER1_1MS_BASE,
	.irq_nr = AM335X_INT_TINT1_1MS,
	.regs = &regs_v1
};

/* free running timer */
static struct omap_timer am335x_fr_timer = {
	.base = AM335X_DMTIMER7_BASE,
	.irq_nr = AM335X_INT_TINT7,
	.regs = &regs_v2
};

static struct omap_timer *timer;
static struct omap_timer *fr_timer;

static int done = 0;

int
bsp_register_timer_handler(const irq_handler_t handler)
{
	/* Initialize the CLOCK's interrupt hook. */
	omap3_timer_hook.proc_nr_e = NONE;
	omap3_timer_hook.irq = timer->irq_nr;

	put_irq_handler(&omap3_timer_hook, timer->irq_nr, handler);
	/* only unmask interrupts after registering */
	bsp_irq_unmask(timer->irq_nr);

	return 0;
}

/* meta data for remapping */
static kern_phys_map timer_phys_map;
static kern_phys_map fr_timer_phys_map;
static kern_phys_map fr_timer_user_phys_map;	/* struct for when the free */
						/* running timer is mapped to */
						/* userland */

/* callback for when the free running clock gets mapped */
int
kern_phys_fr_user_mapped(vir_bytes id, phys_bytes address)
{
	/* the only thing we need to do at this stage is to set the address */
	/* in the kerninfo struct */
	if (BOARD_IS_BBXM(machine.board_id)) {
		arm_frclock.tcrr = address + OMAP3_TIMER_TCRR;
		arm_frclock.hz = 1625000;
	} else if (BOARD_IS_BB(machine.board_id)) {
		arm_frclock.tcrr = address + AM335X_TIMER_TCRR;
		arm_frclock.hz = 1500000;
	}
	return 0;
}

void
omap3_frclock_init(void)
{
	u32_t tisr;

	/* enable the clock */
	if (BOARD_IS_BBXM(machine.board_id)) {
		fr_timer = &dm37xx_fr_timer;

		kern_phys_map_ptr(fr_timer->base, ARM_PAGE_SIZE,
		    VMMF_UNCACHED | VMMF_WRITE, &fr_timer_phys_map,
		    (vir_bytes) & fr_timer->base);

		/* the timer is also mapped in user space hence the this */
		/* second mapping and callback to set kerninfo frclock_tcrr */
		kern_req_phys_map(fr_timer->base, ARM_PAGE_SIZE,
		    VMMF_UNCACHED | VMMF_USER,
		    &fr_timer_user_phys_map, kern_phys_fr_user_mapped, 0);

		/* Stop timer */
		mmio_clear(fr_timer->base + fr_timer->regs->TCLR,
		    OMAP3_TCLR_ST);

		/* Use functional clock source for GPTIMER10 */
		mmio_set(OMAP3_CM_CLKSEL_CORE, OMAP3_CLKSEL_GPT10);

		/* Scale timer down to 13/8 = 1.625 Mhz to roughly get
		 * microsecond ticks */
		/* The scale is computed as 2^(PTV+1). So if PTV == 2, we get
		 * 2^3 = 8. */
		mmio_set(fr_timer->base + fr_timer->regs->TCLR,
		    (2 << OMAP3_TCLR_PTV));
	} else if (BOARD_IS_BB(machine.board_id)) {
		fr_timer = &am335x_fr_timer;
		kern_phys_map_ptr(fr_timer->base, ARM_PAGE_SIZE,
		    VMMF_UNCACHED | VMMF_WRITE,
		    &fr_timer_phys_map, (vir_bytes) & fr_timer->base);

		/* the timer is also mapped in user space hence the this */
		/* second mapping and callback to set kerninfo frclock_tcrr */
		kern_req_phys_map(fr_timer->base, ARM_PAGE_SIZE,
		    VMMF_UNCACHED | VMMF_USER,
		    &fr_timer_user_phys_map, kern_phys_fr_user_mapped, 0);
		/* Disable the module and wait for the module to be disabled */
		set32(CM_PER_TIMER7_CLKCTRL, CM_MODULEMODE_MASK,
		    CM_MODULEMODE_DISABLED);
		while ((mmio_read(CM_PER_TIMER7_CLKCTRL) & CM_CLKCTRL_IDLEST)
		    != CM_CLKCTRL_IDLEST_DISABLE);

		set32(CLKSEL_TIMER7_CLK, CLKSEL_TIMER7_CLK_SEL_MASK,
		    CLKSEL_TIMER7_CLK_SEL_SEL2);
		while ((read32(CLKSEL_TIMER7_CLK) & CLKSEL_TIMER7_CLK_SEL_MASK)
		    != CLKSEL_TIMER7_CLK_SEL_SEL2);

		/* enable the module and wait for the module to be ready */
		set32(CM_PER_TIMER7_CLKCTRL, CM_MODULEMODE_MASK,
		    CM_MODULEMODE_ENABLE);
		while ((mmio_read(CM_PER_TIMER7_CLKCTRL) & CM_CLKCTRL_IDLEST)
		    != CM_CLKCTRL_IDLEST_FUNC);

		/* Stop timer */
		mmio_clear(fr_timer->base + fr_timer->regs->TCLR,
		    OMAP3_TCLR_ST);

		/* 24Mhz / 16 = 1.5 Mhz */
		mmio_set(fr_timer->base + fr_timer->regs->TCLR,
		    (3 << OMAP3_TCLR_PTV));
	}

	/* Start and auto-reload at 0 */
	mmio_write(fr_timer->base + fr_timer->regs->TLDR, 0x0);
	mmio_write(fr_timer->base + fr_timer->regs->TCRR, 0x0);

	/* Set up overflow interrupt */
	tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
	    OMAP3_TISR_TCAR_IT_FLAG;
	/* Clear interrupt status */
	mmio_write(fr_timer->base + fr_timer->regs->TISR, tisr);
	mmio_write(fr_timer->base + fr_timer->regs->TIER,
	    OMAP3_TIER_OVF_IT_ENA);

	/* Start timer */
	mmio_set(fr_timer->base + fr_timer->regs->TCLR,
	    OMAP3_TCLR_OVF_TRG | OMAP3_TCLR_AR | OMAP3_TCLR_ST |
	    OMAP3_TCLR_PRE);
	done = 1;
}

void
omap3_frclock_stop(void)
{
	mmio_clear(fr_timer->base + fr_timer->regs->TCLR, OMAP3_TCLR_ST);
}

void
bsp_timer_init(unsigned freq)
{
	/* we only support 1ms resolution */
	u32_t tisr;
	if (BOARD_IS_BBXM(machine.board_id)) {
		timer = &dm37xx_timer;
		kern_phys_map_ptr(timer->base, ARM_PAGE_SIZE,
		    VMMF_UNCACHED | VMMF_WRITE,
		    &timer_phys_map, (vir_bytes) & timer->base);
		/* Stop timer */
		mmio_clear(timer->base + timer->regs->TCLR, OMAP3_TCLR_ST);

		/* Use 32 KHz clock source for GPTIMER1 */
		mmio_clear(OMAP3_CM_CLKSEL_WKUP, OMAP3_CLKSEL_GPT1);
	} else if (BOARD_IS_BB(machine.board_id)) {
		timer = &am335x_timer;
		kern_phys_map_ptr(timer->base, ARM_PAGE_SIZE,
		    VMMF_UNCACHED | VMMF_WRITE,
		    &timer_phys_map, (vir_bytes) & timer->base);
		/* disable the module and wait for the module to be disabled */
		set32(CM_WKUP_TIMER1_CLKCTRL, CM_MODULEMODE_MASK,
		    CM_MODULEMODE_DISABLED);
		while ((mmio_read(CM_WKUP_TIMER1_CLKCTRL) & CM_CLKCTRL_IDLEST)
		    != CM_CLKCTRL_IDLEST_DISABLE);

		set32(CLKSEL_TIMER1MS_CLK, CLKSEL_TIMER1MS_CLK_SEL_MASK,
		    CLKSEL_TIMER1MS_CLK_SEL_SEL2);
		while ((read32(CLKSEL_TIMER1MS_CLK) &
			CLKSEL_TIMER1MS_CLK_SEL_MASK) !=
		    CLKSEL_TIMER1MS_CLK_SEL_SEL2);

		/* enable the module and wait for the module to be ready */
		set32(CM_WKUP_TIMER1_CLKCTRL, CM_MODULEMODE_MASK,
		    CM_MODULEMODE_ENABLE);
		while ((mmio_read(CM_WKUP_TIMER1_CLKCTRL) & CM_CLKCTRL_IDLEST)
		    != CM_CLKCTRL_IDLEST_FUNC);

		/* Stop timer */
		mmio_clear(timer->base + timer->regs->TCLR, OMAP3_TCLR_ST);
	}

	/* Use 1-ms tick mode for GPTIMER1 TRM 16.2.4.2.1 */
	mmio_write(timer->base + timer->regs->TPIR, 232000);
	mmio_write(timer->base + timer->regs->TNIR, -768000);
	mmio_write(timer->base + timer->regs->TLDR,
	    0xffffffff - (32768 / freq) + 1);
	mmio_write(timer->base + timer->regs->TCRR,
	    0xffffffff - (32768 / freq) + 1);

	/* Set up overflow interrupt */
	tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
	    OMAP3_TISR_TCAR_IT_FLAG;
	/* Clear interrupt status */
	mmio_write(timer->base + timer->regs->TISR, tisr);
	mmio_write(timer->base + timer->regs->TIER, OMAP3_TIER_OVF_IT_ENA);

	/* Start timer */
	mmio_set(timer->base + timer->regs->TCLR,
	    OMAP3_TCLR_OVF_TRG | OMAP3_TCLR_AR | OMAP3_TCLR_ST);
	/* also initilize the free runnning timer */
	omap3_frclock_init();
}

void
bsp_timer_stop(void)
{
	mmio_clear(timer->base + timer->regs->TCLR, OMAP3_TCLR_ST);
}

static u32_t
read_frc(void)
{
	if (done == 0) {
		return 0;
	}
	return mmio_read(fr_timer->base + fr_timer->regs->TCRR);
}

/*
 * Check if the free running clock has overflown and
 * increase the high free running clock counter if
 * so. This method takes the current timer value as
 * parameter to ensure the overflow check is done
 * on the current timer value.
 *
 * To compose the current timer value (64 bits) you
 * need to follow the following sequence:
 *  read the current timer value.
 *  call the overflow check
 *  compose the 64 bits time based on the current timer value
 *   and high_frc.
 */
static void
frc_overflow_check(u32_t cur_frc)
{
	static int prev_frc_valid;
	static u32_t prev_frc;
	if (prev_frc_valid && prev_frc > cur_frc) {
		high_frc++;
	}
	prev_frc = cur_frc;
	prev_frc_valid = 1;
}

void
bsp_timer_int_handler(void)
{
	/* Clear all interrupts */
	u32_t tisr, now;

	/* when the kernel itself is running interrupts are disabled. We
	 * should therefore also read the overflow counter to detect this as
	 * to not miss events. */
	tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
	    OMAP3_TISR_TCAR_IT_FLAG;
	mmio_write(timer->base + timer->regs->TISR, tisr);

	now = read_frc();
	frc_overflow_check(now);
}

/* Use the free running clock as TSC */
void
read_tsc_64(u64_t * t)
{
	u32_t now;
	now = read_frc();
	frc_overflow_check(now);
	*t = (u64_t) now + (high_frc << 32);
}
