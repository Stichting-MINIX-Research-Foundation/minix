#include "kernel/kernel.h"
#include "kernel/clock.h"
#include <sys/types.h>
#include <machine/cpu.h>
#include <io.h>
#include "arch_proto.h"
#include "omap_timer.h"
#include "omap_intr.h"

static irq_hook_t omap3_timer_hook;		/* interrupt handler hook */
static u64_t tsc;

int omap3_register_timer_handler(const irq_handler_t handler)
{
	/* Initialize the CLOCK's interrupt hook. */
	omap3_timer_hook.proc_nr_e = NONE;
	omap3_timer_hook.irq = OMAP3_GPT1_IRQ;

	put_irq_handler(&omap3_timer_hook, OMAP3_GPT1_IRQ, handler);

	return 0;
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

void omap3_timer_int_handler()
{
    /* Clear all interrupts */
    u32_t tisr;

    tisr = OMAP3_TISR_MAT_IT_FLAG | OMAP3_TISR_OVF_IT_FLAG |
           OMAP3_TISR_TCAR_IT_FLAG;
    mmio_write(OMAP3_GPTIMER1_TISR, tisr);
    tsc++;
}

/* Don't use libminlib's read_tsc_64, but our own version instead. We emulate
 * the ARM Cycle Counter (CCNT) with 1 cycle per ms. We can't rely on the
 * actual counter hardware to be working (i.e., qemu doesn't emulate it at all)
 */
void read_tsc_64(u64_t *t)
{
    *t = tsc;
}
