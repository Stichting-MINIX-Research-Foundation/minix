#ifndef __HW_INTR_ARM_H__
#define __HW_INTR_ARM_H__

#include "kernel/kernel.h"
void irq_handle(int irq);

#define hw_intr_mask(irq)	omap3_irq_mask(irq)
#define hw_intr_unmask(irq)	omap3_irq_unmask(irq)
#define hw_intr_ack(irq)
#define hw_intr_used(irq)
#define hw_intr_not_used(irq)
#define hw_intr_disable_all()

#endif /* __HW_INTR_ARM_H__ */
