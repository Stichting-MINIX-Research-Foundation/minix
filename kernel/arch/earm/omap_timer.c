#include "kernel/kernel.h"
#include "kernel/clock.h"
#include <sys/types.h>
#include <machine/cpu.h>
#include <io.h>
#include "arch_proto.h"
#include "omap_timer.h"
#include "omap_intr.h"

static irq_hook_t omap3_timer_hook;		/* interrupt handler hook */
static u64_t high_frc;

vir_bytes omap3_gptimer10_base;

int omap3_register_timer_handler(const irq_handler_t handler)
{
	/* Initialize the CLOCK's interrupt hook. */
	omap3_timer_hook.proc_nr_e = NONE;
	omap3_timer_hook.irq = OMAP3_GPT1_IRQ;

	put_irq_handler(&omap3_timer_hook, OMAP3_GPT1_IRQ, handler);

	return 0;
}

void omap3_frclock_init(void)
{
    u32_t tisr;

    /* Stop timer */
    mmio_clear(OMAP3_GPTIMER10_TCLR, OMAP3_TCLR_ST);

    /* Use functional clock source for GPTIMER10 */
    mmio_set(OMAP3_CM_CLKSEL_CORE, OMAP3_CLKSEL_GPT10);

    /* Scale timer down to 13/8 = 1.625 Mhz to roughly get microsecond ticks */
    /* The scale is computed as 2^(PTV+1). So if PTV == 2, we get 2^3 = 8.
     */
    mmio_set(OMAP3_GPTIMER10_TCLR, (2 << OMAP3_TCLR_PTV));

    /* Start and auto-reload at 0 */
    mmio_write(OMAP3_GPTIMER10_TLDR, 0x0);
    mmio_write(OMAP3_GPTIMER10_TCRR, 0x0);

    /* Set up overflow interrupt */
    tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
          OMAP3_TISR_TCAR_IT_FLAG;
    mmio_write(OMAP3_GPTIMER10_TISR, tisr); /* Clear interrupt status */
    mmio_write(OMAP3_GPTIMER10_TIER, OMAP3_TIER_OVF_IT_ENA);

    /* Start timer */
    mmio_set(OMAP3_GPTIMER10_TCLR,
            OMAP3_TCLR_OVF_TRG|OMAP3_TCLR_AR|OMAP3_TCLR_ST|OMAP3_TCLR_PRE);
}

void omap3_frclock_stop()
{
    mmio_clear(OMAP3_GPTIMER10_TCLR, OMAP3_TCLR_ST);
}


void omap3_timer_init(unsigned freq)
{
    u32_t tisr;

    /* Stop timer */
    mmio_clear(OMAP3_GPTIMER1_TCLR, OMAP3_TCLR_ST);

    /* Use 32 KHz clock source for GPTIMER1 */
    mmio_clear(OMAP3_CM_CLKSEL_WKUP, OMAP3_CLKSEL_GPT1);

    /* Use 1-ms tick mode for GPTIMER1 TRM 16.2.4.2.1 */
    mmio_write(OMAP3_GPTIMER1_TPIR, 232000);
    mmio_write(OMAP3_GPTIMER1_TNIR, -768000);
    mmio_write(OMAP3_GPTIMER1_TLDR, 0xffffffe0);
    mmio_write(OMAP3_GPTIMER1_TCRR, 0xffffffe0);

    /* Set up overflow interrupt */
    tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
	   OMAP3_TISR_TCAR_IT_FLAG;
    mmio_write(OMAP3_GPTIMER1_TISR, tisr); /* Clear interrupt status */
    mmio_write(OMAP3_GPTIMER1_TIER, OMAP3_TIER_OVF_IT_ENA);
    omap3_irq_unmask(OMAP3_GPT1_IRQ);

    /* Start timer */
    mmio_set(OMAP3_GPTIMER1_TCLR,
	     OMAP3_TCLR_OVF_TRG|OMAP3_TCLR_AR|OMAP3_TCLR_ST);
}

void omap3_timer_stop()
{
    mmio_clear(OMAP3_GPTIMER1_TCLR, OMAP3_TCLR_ST);
}

static u32_t read_frc(void)
{
	u32_t frc = *(u32_t *) ((char *) omap3_gptimer10_base + OMAP3_TCRR);
	return frc;
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

    tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
           OMAP3_TISR_TCAR_IT_FLAG;
    mmio_write(OMAP3_GPTIMER1_TISR, tisr);

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
