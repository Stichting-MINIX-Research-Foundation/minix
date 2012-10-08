#include "kernel/kernel.h"
#include "kernel/clock.h"
#include <sys/types.h>
#include <machine/cpu.h>
#include <io.h>
#include "arch_proto.h"
#include "omap_timer.h"
#include "omap_intr.h"

static irq_hook_t omap3_timer_hook;		/* interrupt handler hook */

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
    /* Stop timer */
    mmio_clear(OMAP3_GPTIMER1_TCLR, OMAP3_TCLR_ST);

    /* Use 32 KHz clock source for GPTIMER1 */
    mmio_clear(OMAP3_CM_CLKSEL_WKUP, OMAP3_CLKSEL_GPT1);

    /* Use 1-ms tick mode for GPTIMER1 */
    mmio_write(OMAP3_GPTIMER1_TPIR, 232000);
    mmio_write(OMAP3_GPTIMER1_TNIR, -768000);
    mmio_write(OMAP3_GPTIMER1_TLDR, 0xffffffe0);
    mmio_write(OMAP3_GPTIMER1_TCRR, 0xffffffe0);

    /* Set frequency */
    mmio_write(OMAP3_GPTIMER1_TOWR, TIMER_COUNT(freq));

    /* Set up overflow interrupt */
    mmio_write(OMAP3_GPTIMER1_TISR, ~0);
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

static u64_t tsc;
void omap3_timer_int_handler()
{
    /* Clear the interrupt */
    mmio_write(OMAP3_GPTIMER1_TISR, ~0);
    tsc++;
}

void read_tsc_64(u64_t *t)
{
    *t = tsc;
}
