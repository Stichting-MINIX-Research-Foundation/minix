#include <sys/types.h>
#include <machine/cpu.h>
#include <io.h>
#include "omap_intr.h"

int intr_init(const int auto_eoi)
{
    return 0;
}

void omap3_irq_unmask(int irq)
{
    mmio_write(OMAP3_INTR_MIR_CLEAR(irq >> 5), 1 << (irq & 0x1f));
}

void omap3_irq_mask(const int irq)
{
    mmio_write(OMAP3_INTR_MIR_SET(irq >> 5), 1 << (irq & 0x1f));
}
