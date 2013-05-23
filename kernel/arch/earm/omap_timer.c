#include "kernel/kernel.h"
#include "kernel/clock.h"
#include <sys/types.h>
#include <machine/cpu.h>
#include <minix/mmio.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include "arch_proto.h"
#include "omap_timer.h"
#include "omap_intr.h"

static irq_hook_t omap3_timer_hook;		/* interrupt handler hook */
static u64_t high_frc;

struct omap_timer_registers;

struct omap_timer {
	vir_bytes base;
	int irq_nr;
	struct omap_timer_registers *regs;
};

struct omap_timer_registers {
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

#ifdef DM37XX
static struct omap_timer timer = {
		.base = OMAP3_GPTIMER1_BASE,
		.irq_nr = OMAP3_GPT1_IRQ,
		.regs = &regs_v1
};

/* free running timer */
static struct omap_timer fr_timer = {
		.base = OMAP3_GPTIMER10_BASE,
		.irq_nr = OMAP3_GPT10_IRQ,
		.regs = &regs_v1
};

#endif

#ifdef AM335X

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
	.TPIR = -1 , /* UNDEF */
	.TNIR = -1 , /* UNDEF */
	.TCVR = -1 , /* UNDEF */
	.TOCR = -1 , /* UNDEF */
	.TOWR = -1  /* UNDEF */
};

/* normal timer */
static struct omap_timer timer = {
		.base = AM335X_DMTIMER1_1MS_BASE,
		.irq_nr = AM335X_INT_TINT1_1MS,
		.regs = &regs_v1

};

/* free running timer */
static struct omap_timer fr_timer = {
		.base = AM335X_DMTIMER7_BASE,
		.irq_nr = AM335X_INT_TINT7,
		.regs = &regs_v2
};

#endif

static int done = 0;

int omap3_register_timer_handler(const irq_handler_t handler)
{
	/* Initialize the CLOCK's interrupt hook. */
	omap3_timer_hook.proc_nr_e = NONE;
	omap3_timer_hook.irq = timer.irq_nr;

	put_irq_handler(&omap3_timer_hook, timer.irq_nr, handler);

	return 0;
}

void omap3_frclock_init(void)
{
    u32_t tisr;

    /* enable the clock */
#ifdef AM335X
    /* Disable the module and wait for the module to be disabled */
    set32(CM_PER_TIMER7_CLKCTRL, CM_MODULEMODE_MASK,CM_MODULEMODE_DISABLED);
    while( (mmio_read(CM_PER_TIMER7_CLKCTRL) & CM_CLKCTRL_IDLEST) != CM_CLKCTRL_IDLEST_DISABLE);

    set32(CLKSEL_TIMER7_CLK,CLKSEL_TIMER7_CLK_SEL_MASK, CLKSEL_TIMER7_CLK_SEL_SEL2);
    while( (read32(CLKSEL_TIMER7_CLK) & CLKSEL_TIMER7_CLK_SEL_MASK) != CLKSEL_TIMER7_CLK_SEL_SEL2);

    /* enable the module and wait for the module to be ready */
    set32(CM_PER_TIMER7_CLKCTRL,CM_MODULEMODE_MASK,CM_MODULEMODE_ENABLE);
    while( (mmio_read(CM_PER_TIMER7_CLKCTRL) & CM_CLKCTRL_IDLEST) != CM_CLKCTRL_IDLEST_FUNC);
#endif

    /* Stop timer */
    mmio_clear(fr_timer.base + fr_timer.regs->TCLR, OMAP3_TCLR_ST);

#ifdef DM37XX
    /* Use functional clock source for GPTIMER10 */
    mmio_set(OMAP3_CM_CLKSEL_CORE, OMAP3_CLKSEL_GPT10);
#endif

#ifdef DM37XX
    /* Scale timer down to 13/8 = 1.625 Mhz to roughly get microsecond ticks */
    /* The scale is computed as 2^(PTV+1). So if PTV == 2, we get 2^3 = 8.
     */
    mmio_set(fr_timer.base + fr_timer.regs->TCLR, (2 << OMAP3_TCLR_PTV));
#endif
#ifdef AM335X
   /* 24Mhz / 16 = 1.5 Mhz */
    mmio_set(fr_timer.base + fr_timer.regs->TCLR, (3 << OMAP3_TCLR_PTV));
#endif

    /* Start and auto-reload at 0 */
    mmio_write(fr_timer.base + fr_timer.regs->TLDR, 0x0);
    mmio_write(fr_timer.base + fr_timer.regs->TCRR, 0x0);

    /* Set up overflow interrupt */
    tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
          OMAP3_TISR_TCAR_IT_FLAG;
    mmio_write(fr_timer.base + fr_timer.regs->TISR, tisr); /* Clear interrupt status */
    mmio_write(fr_timer.base + fr_timer.regs->TIER, OMAP3_TIER_OVF_IT_ENA);

    /* Start timer */
    mmio_set(fr_timer.base + fr_timer.regs->TCLR,
            OMAP3_TCLR_OVF_TRG|OMAP3_TCLR_AR|OMAP3_TCLR_ST|OMAP3_TCLR_PRE);
}

void omap3_frclock_stop()
{
    mmio_clear(fr_timer.base + fr_timer.regs->TCLR, OMAP3_TCLR_ST);
}


void omap3_timer_init(unsigned freq)
{
    u32_t tisr;
#ifdef AM335X
    /* disable the module and wait for the module to be disabled */
    set32(CM_WKUP_TIMER1_CLKCTRL, CM_MODULEMODE_MASK,CM_MODULEMODE_DISABLED);
    while( (mmio_read(CM_WKUP_TIMER1_CLKCTRL) & CM_CLKCTRL_IDLEST) != CM_CLKCTRL_IDLEST_DISABLE);


    set32(CLKSEL_TIMER1MS_CLK,CLKSEL_TIMER1MS_CLK_SEL_MASK, CLKSEL_TIMER1MS_CLK_SEL_SEL2);
    while( (read32(CLKSEL_TIMER1MS_CLK) & CLKSEL_TIMER1MS_CLK_SEL_MASK) != CLKSEL_TIMER1MS_CLK_SEL_SEL2);


    /* enable the module and wait for the module to be ready */
    set32(CM_WKUP_TIMER1_CLKCTRL,CM_MODULEMODE_MASK,CM_MODULEMODE_ENABLE);
    while( (mmio_read(CM_WKUP_TIMER1_CLKCTRL) & CM_CLKCTRL_IDLEST) != CM_CLKCTRL_IDLEST_FUNC);
#endif
    /* Stop timer */
    mmio_clear(timer.base + fr_timer.regs->TCLR, OMAP3_TCLR_ST);

#ifdef DM37XX
    /* Use 32 KHz clock source for GPTIMER1 */
    mmio_clear(OMAP3_CM_CLKSEL_WKUP, OMAP3_CLKSEL_GPT1);
#endif

    /* Use 1-ms tick mode for GPTIMER1 TRM 16.2.4.2.1 */
    mmio_write(timer.base + timer.regs->TPIR, 232000);
    mmio_write(timer.base + timer.regs->TNIR, -768000);
    mmio_write(timer.base + timer.regs->TLDR, 0xffffffe0);
    mmio_write(timer.base + timer.regs->TCRR, 0xffffffe0);

    /* Set up overflow interrupt */
    tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
	   OMAP3_TISR_TCAR_IT_FLAG;
    mmio_write(timer.base + timer.regs->TISR, tisr); /* Clear interrupt status */
    mmio_write(timer.base + timer.regs->TIER, OMAP3_TIER_OVF_IT_ENA);
    omap3_irq_unmask(timer.irq_nr);

    /* Start timer */
    mmio_set(timer.base + timer.regs->TCLR,
	     OMAP3_TCLR_OVF_TRG|OMAP3_TCLR_AR|OMAP3_TCLR_ST);
}

void omap3_timer_stop()
{
    mmio_clear(timer.base + timer.regs->TCLR, OMAP3_TCLR_ST);
}

static u32_t read_frc(void)
{
	if (done == 0)
			return 0;
	return mmio_read(fr_timer.base  +  fr_timer.regs->TCRR);
}

static void frc_overflow_check(void)
{
	static int prev_frc_valid;
	static u32_t prev_frc;
	u32_t cur_frc = read_frc();
	if(prev_frc_valid && prev_frc > cur_frc)
		high_frc++;
	prev_frc = cur_frc;
	prev_frc_valid = 1;
}

void omap3_timer_int_handler()
{
    /* Clear all interrupts */
    u32_t tisr;


    /* when the kernel itself is running interrupts are disabled.
     * We should therefore also read the overflow counter to detect
     * this as to not miss events.
     */
    tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
           OMAP3_TISR_TCAR_IT_FLAG;
    mmio_write(timer.base + timer.regs->TISR, tisr);

   frc_overflow_check();
}

/* Use the free running clock as TSC */
void read_tsc_64(u64_t *t)
{
	u32_t now;
   	frc_overflow_check();
	now = read_frc();
	*t = (u64_t) now + (high_frc << 32);
}
