/* hw_intr handles the hardware dependent part of the interrupts */
#include "hw_intr.h"
#include "bsp_intr.h"

void hw_intr_mask(int irq){
	bsp_irq_mask(irq);
}

void hw_intr_unmask(int irq){
	bsp_irq_unmask(irq);
}

void hw_intr_ack(int irq){};
void hw_intr_used(int irq){};
void hw_intr_not_used(int irq){};
void hw_intr_disable_all(void){};
