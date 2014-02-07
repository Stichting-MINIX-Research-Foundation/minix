#ifndef __HW_INTR_ARM_H__
#define __HW_INTR_ARM_H__


#include "kernel/kernel.h"
void irq_handle(int irq);

int hw_intr_mask(int irq);
int hw_intr_unmask(int irq);
int hw_intr_ack(int irq);
int hw_intr_used(int irq);
int hw_intr_not_used(int irq);
int hw_intr_disable_all();

#endif /* __HW_INTR_ARM_H__ */
