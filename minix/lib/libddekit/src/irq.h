#ifndef DDEKIT_IRQ_MINIX_H
#define DDEKIT_IRQ_MINIX_H 1 
void ddekit_init_irqs(void);
void _ddekit_interrupt_trigger(int irq_id);
#endif 
